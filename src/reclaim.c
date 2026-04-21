#include "reclaim.h"
#include "internal.h"
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_key_t tcache_key;

static void global_init(void);
static void tcache_destroy(void *arg);

static _Thread_local tcache_t tcache;

static inline __attribute__((always_inline)) void tcache_ensure_init(void) {
  if (likely(tcache.initialized))
    return;
  pthread_once(&init_once, global_init);
  tcache.initialized = true;
  pthread_setspecific(tcache_key, (void *)1);
}

/**
 * Flush half of a thread-cache bin back to the ccache.
 *
 * This happens when count exceeds MAX_CACHED.
 */
static void tcache_flush(int sc) {
  int to_flush = tcache.bins[sc].count >> 1;
  if (to_flush < 1)
    to_flush = 1;

  void *list = tcache.bins[sc].bin;
  void *tail = list;
  for (int i = 1; i < to_flush; i++)
    tail = *(void **)tail;

  void *rest = *(void **)tail;
  *(void **)tail = NULL;

  tcache.bins[sc].bin = rest;
  tcache.bins[sc].count -= to_flush;

  ccache_return(sc, list, tail, to_flush);
}

/**
 * Refill tcache.
 *
 * If the ccache has a chunk of the right size
 * available, then take it from there; otherwise fetch from backend.
 */
static void *tcache_refill(int sc) {
  int got = 0;
  void *list = ccache_fetch(sc, BATCH_SIZE, &got);

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
      ccache_return(sc, central_head, NULL, got - BATCH_SIZE);
      got = BATCH_SIZE;
    }
  }

  // Return first to the caller, cache the rest.
  void *result = list;
  void *rest = *(void **)list;

  tcache.bins[sc].bin = rest;
  tcache.bins[sc].count = got - 1;

  return result;
}

/**
 * Deallocate a tcache.
 */
static void tcache_destroy(void *arg) {
  (void)arg;
  for (int sc = 0; sc < NUM_SIZE_CLASSES; sc++) {
    if (tcache.bins[sc].bin) {
      ccache_return(sc, tcache.bins[sc].bin, NULL, tcache.bins[sc].count);
      tcache.bins[sc].bin = NULL;
      tcache.bins[sc].count = 0;
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

/**
 * Destruct the allocator subsystem.
 */
static void global_deinit(void) {
  tcache_destroy(NULL);
  ccache_deinit();
  // Release span memory to the OS.
  backend_deinit();
  pthread_key_delete(tcache_key);
}

// ===== Public API =====

void recl_alloc_main_heap(void) { pthread_once(&init_once, global_init); }
void recl_free_main_heap(void) { global_deinit(); }

void *recl_malloc(size_t size) {
  if (unlikely(size == 0))
    size = 1;

  tcache_ensure_init();

  // Large allocation goes directly to mmap...
  if (unlikely(size > LARGE_THRESHOLD))
    return large_alloc(size);

  int sc = size_to_class(size);

  // Hot allocation path: fetch from frontend.
  void *obj = tcache.bins[sc].bin;
  if (likely(obj)) {
    tcache.bins[sc].bin = *(void **)obj;
    tcache.bins[sc].count--;
    return obj;
  }

  // Cold allocation path: refill from central cache or fetch a span from the
  // backend.
  return tcache_refill(sc);
}

void recl_free(void *ptr) {
  if (unlikely(!ptr))
    return;

  // Determine whether this is a span-based or mmap
  // (> 256 KiB) allocation.
  span_t *s = (span_t *)((uintptr_t)ptr & SPAN_MASK);

  if (unlikely(s->magic != SPAN_MAGIC)) {
    large_free(ptr);
    return;
  }

  tcache_ensure_init();

  int sc = (int)s->size_class;

  // Push chunk to tcache.
  *(void **)ptr = tcache.bins[sc].bin;
  tcache.bins[sc].bin = ptr;
  tcache.bins[sc].count++;

  // Flush half if over limit.
  if (unlikely(tcache.bins[sc].count > MAX_CACHED))
    tcache_flush(sc);
}