#include "internal.h"
#include <pthread.h>
#include <sys/mman.h>

/**
 * Central cache (`ccache`).
 *
 * This global data structure is responsible for receiving
 * freed chunks from the tcaches when they become full.
 * When a chunk of the requested size is absent from the
 * local tcache, then they're fetched from the ccache.
 * ccaches are not lock-free; they are accessed using spinlocks,
 * which have lower latency w.r.t. mutexes.
 */
static central_bin_t ccache[NUM_SIZE_CLASSES];

void ccache_init(void) {
  for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
    pthread_spin_init(&ccache[i].lock, PTHREAD_PROCESS_PRIVATE);
    ccache[i].head = NULL;
    ccache[i].count = 0;
  }
}

void ccache_deinit(void) {
  // Walk every bin's free list and clear it.
  span_t *to_free = NULL;

  for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
    void *cur = ccache[i].head;
    while (cur) {
      void *next_obj = *(void **)cur;
      span_t *s = (span_t *)((uintptr_t)cur & SPAN_MASK);
      if (s->magic == SPAN_MAGIC) {
        // Set magic numbers to 0 to avoid re-queueing it.
        s->magic = 0;
        s->next = to_free;
        to_free = s;
      }
      cur = next_obj;
    }
    pthread_spin_destroy(&ccache[i].lock);
    ccache[i].head = NULL;
    ccache[i].count = 0;
  }

  span_t *s = to_free;
  while (s) {
    span_t *next = s->next;
    munmap(s, SPAN_SIZE);
    s = next;
  }
}

/**
 * Fetch up to `batch` objects from the ccache.
 *
 * The size class bin is determined by `sc`.
 *
 * Returns: * A linked list containing the fetched chunks.
 *          * NULL if the bin for size class `sc` is empty.
 */
void *central_fetch(int sc, int batch, int *out_count) {
  central_bin_t *bin = &ccache[sc];
  void *result = NULL;
  int got = 0;

  pthread_spin_lock(&bin->lock);

  void *cur = bin->head;
  void *tail = NULL;

  while (cur && got < batch) {
    if (got == 0)
      result = cur;
    tail = cur;
    cur = *(void **)cur;
    got++;
  }

  // Detach the fetched chain from the rest
  if (tail)
    *(void **)tail = NULL;

  bin->head = cur;
  bin->count -= got;

  pthread_spin_unlock(&bin->lock);

  *out_count = got;
  return result;
}

/**
 * Return `count` objects to the ccache.
 *
 * The size class bin is determined by `sc`.
 */
void ccache_return(int sc, void *list, void *tail, int count) {
  if (!list || count <= 0)
    return;

  central_bin_t *bin = &ccache[sc];

  if (tail == NULL) {
    // Walk to the tail of the incoming list
    tail = list;
    for (int i = 1; i < count; i++)
      tail = *(void **)tail;
  }

  pthread_spin_lock(&bin->lock);
  // Connect incoming chunks to the ccache freelist.
  *(void **)tail = bin->head;
  bin->head = list;
  bin->count += count;
  pthread_spin_unlock(&bin->lock);
}
