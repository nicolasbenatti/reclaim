#include "reclaim.h"
#include "internal.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_key_t tcache_key;

static void global_init(void);
static void tcache_destroy(void *arg);

static _Thread_local tcache_t tcache;

static void tcache_ensure_init(void) {
  pthread_once(&init_once, global_init);

  if (!tcache.initialized) {
    memset(&tcache, 0, sizeof(tcache));
    tcache.initialized = true;
    // Register current thread so to deallocate its cache when it exits.
    pthread_setspecific(tcache_key, (void *)1);
  }
}

/**
 * Flush half of a thread-cache bin back to the ccache.
 *
 * This happens when count exceeds MAX_CACHED.
 */
static void tcache_flush(int sc) {
  int to_flush = tcache.count[sc] / 2;
  if (to_flush < 1)
    to_flush = 1;

  void *list = tcache.bins[sc];
  void *tail = list;
  for (int i = 1; i < to_flush; i++)
    tail = *(void **)tail;

  void *rest = *(void **)tail;
  *(void **)tail = NULL;

  tcache.bins[sc] = rest;
  tcache.count[sc] -= to_flush;

  central_return(sc, list, to_flush);
}

/**
 * Refill tcache.
 *
 * If the ccache has a chunk of the right size
 * available, then take it from there; otherwise fetch from backend.
 */
static void *tcache_refill(int sc) {
  int got = 0;
  void *list = central_fetch(sc, BATCH_SIZE, &got);

  if (!list) {
    // NOTE: the span is directly fetched to avoid calling
    // the ccache in between.
    span_t *s = span_alloc(sc);
    if (!s)
      return NULL;

    list = s->base;
    got = (int)s->total_objects;

    // Share excess objects with ccache for other threads
    if (got > BATCH_SIZE) {
      void *split = list;
      for (int i = 1; i < BATCH_SIZE; i++)
        split = *(void **)split;
      void *central_head = *(void **)split;
      *(void **)split = NULL;
      central_return(sc, central_head, got - BATCH_SIZE);
      got = BATCH_SIZE;
    }
  }

  // Return first to the caller, cache the rest.
  void *result = list;
  void *rest = *(void **)list;

  tcache.bins[sc] = rest;
  tcache.count[sc] = got - 1;

  return result;
}

/**
 * Deallocate a tcache.
 */
static void tcache_destroy(void *arg) {
  (void)arg;
  for (int sc = 0; sc < NUM_SIZE_CLASSES; sc++) {
    if (tcache.bins[sc]) {
      central_return(sc, tcache.bins[sc], tcache.count[sc]);
      tcache.bins[sc] = NULL;
      tcache.count[sc] = 0;
    }
  }
  tcache.initialized = false;
}

/**
 * Initialise the allocator subsystem.
 */
static void global_init(void) {
  backend_init();
  ccache_init();
  pthread_key_create(&tcache_key, tcache_destroy);
}

// ===== Public API =====

void recl_alloc_main_heap(void) { pthread_once(&init_once, global_init); }

void *recl_malloc(size_t size) {
  if (size == 0)
    size = 1;

  tcache_ensure_init();

  // Large allocation goes directly to mmap...
  if (size > LARGE_THRESHOLD)
    return large_alloc(size);

  int sc = size_to_class(size);

  // Hot allocation path: fetch from frontend.
  if (tcache.bins[sc]) {
    void *obj = tcache.bins[sc];
    tcache.bins[sc] = *(void **)obj;
    tcache.count[sc]--;
    return obj;
  }

  // Cold allocation path: refill from central cache or fetch a span from the
  // backend.
  return tcache_refill(sc);
}

void recl_free(void *ptr) {
  if (!ptr)
    return;

  // Determine whether this is a span-based or mmap
  // (> 256 KiB) allocation.
  span_t *s = (span_t *)((uintptr_t)ptr & SPAN_MASK);

  if (s->magic != SPAN_MAGIC) {
    large_free(ptr);
    return;
  }

  tcache_ensure_init();

  int sc = (int)s->size_class;

  // Push chunk to tcache.
  *(void **)ptr = tcache.bins[sc];
  tcache.bins[sc] = ptr;
  tcache.count[sc]++;

  // Flush half if over limit.
  if (tcache.count[sc] > MAX_CACHED)
    tcache_flush(sc);
}