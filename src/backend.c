#include "internal.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * Span cache (scache).
 *
 * This global data structure performs bookkeping for free spans;
 * it serves requests coming from the ccache, requesting new spans
 * from the OS when needed.
 * Access is protected by a global lock.
 */
static pthread_spinlock_t span_lock;
static span_t *scache;

/**
 * Cache for large allocations (above `LARGE_THRESHOLD`).
 */
static pthread_spinlock_t large_lock;
static large_hdr_t *largecache[NUM_LARGE_CLASSES];
static long system_page_size;

void backend_init(void) {
  pthread_spin_init(&span_lock, PTHREAD_PROCESS_PRIVATE);
  pthread_spin_init(&large_lock, PTHREAD_PROCESS_PRIVATE);
  system_page_size = sysconf(_SC_PAGESIZE);
  scache = NULL;
  for (int32_t i = 0; i < NUM_LARGE_CLASSES; i++) {
    largecache[i] = NULL;
  }
}

void backend_deinit(void) {
  // Walk the scache list and release every span to the OS.
  span_t *runner = scache;
  while (runner) {
    span_t *next = runner->next;
    munmap(runner, SPAN_SIZE);
    runner = next;
  }
  runner = NULL;
  pthread_spin_destroy(&span_lock);
}

/**
 * Allocate a span.
 *
 * Returns: address of the span.
 */
__attribute__((cold)) static void *mmap_span(void) {
  /*
   * Allocate 2 * SPAN_SIZE so to carve out an aligned SPAN_SIZE
   * region, then unmap the excess before and after.
   * NOTE: spans actually consume physical memory only when
   * requested, thanks to on-demand paging.
   */
  size_t map_size = SPAN_SIZE * 2;
  void *raw = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (raw == MAP_FAILED)
    return NULL;

  uintptr_t aligned = ((uintptr_t)raw + SPAN_SIZE - 1) & SPAN_MASK;

  // Trim leading excess
  size_t leading = aligned - (uintptr_t)raw;
  if (leading > 0)
    munmap(raw, leading);

  // Trim trailing excess
  size_t trailing = map_size - leading - SPAN_SIZE;
  if (trailing > 0)
    munmap((void *)(aligned + SPAN_SIZE), trailing);

  return (void *)aligned;
}

/**
 * Initialise a memory region as a span.
 */
static void span_init(span_t *s, int size_class) {
  size_t obj_size = class_to_size(size_class);

  s->magic = SPAN_MAGIC;
  s->size_class = (uint32_t)size_class;
  s->size = (uint32_t)obj_size;
  s->next = NULL;

  uintptr_t base = (uintptr_t)s + sizeof(span_t);
  base = (base + obj_size - 1) & ~(obj_size - 1);
  s->base = (void *)base;

  uintptr_t span_end = (uintptr_t)s + SPAN_SIZE;
  uint32_t count = (uint32_t)((span_end - base) / obj_size);
  s->total_objects = count;

  // Build a linked free list through the objects, by storing the next
  // ptr in the first 4 bytes of a free object.
  void *prev = NULL;
  for (uint32_t i = count; i > 0; i--) {
    void *obj = (void *)(base + (i - 1) * obj_size);
    *(void **)obj = prev;
    prev = obj;
  }
}

/**
 * Allocate a span for a given size class.
 */
span_t *span_alloc(int size_class) {
  span_t *s = NULL;

  pthread_spin_lock(&span_lock);
  if (likely(scache)) {
    s = scache;
    scache = s->next;
  }
  pthread_spin_unlock(&span_lock);

  if (unlikely(!s)) {
    s = (span_t *)mmap_span();
    if (unlikely(!s))
      return NULL;
  }

  span_init(s, size_class);

  return s;
}

/**
 * Public: release an empty span back to the cache
 *
 * NOTE: for now, reclaim doesn't yield memory back
 * to the OS unless `recl_free_main_heap` is called.
 * It might in the future.
 */
void span_release(span_t *s) {
  s->magic = 0;

  pthread_spin_lock(&span_lock);
  s->next = scache;
  scache = s;
  pthread_spin_unlock(&span_lock);
}

/**
 * Perform a large allocation.
 *
 * In reclaim, a large allocation is one
 * exceeding `LARGE_THRESHOLD`, which bypasses
 * all caches and goes directly mmap'd from the backend.
 */
__attribute__((
    cold)) // marked as cold; that's a tradeoff we make to favour smaller allocs
void *
large_alloc(size_t size) {
  /*
   * Large allocations must be SPAN_SIZE-aligned so that
   * recl_free() can use (ptr & SPAN_MASK) to find the header
   * and distinguish large from span-based allocations.
   */

  // Compute needed chunk size (account for metadata)
  size_t hdr_offset = sizeof(large_hdr_t);
  if (hdr_offset < 16)
    hdr_offset = 16;
  else
    // Align start of chunk to 16 bytes
    hdr_offset = (hdr_offset + 15) & (~15);

  size_t needed = hdr_offset + size;
  needed =
      (needed + (size_t)system_page_size - 1) & ~((size_t)system_page_size - 1);

  // Look for a free chunk in largecache
  int class_idx = size_to_class_large(needed);
  size_t bin_size = class_to_size_large(class_idx);
  pthread_spin_lock(&large_lock);
  _Bool found = false;
  for (int32_t i = class_idx; !found && i < NUM_LARGE_CLASSES; i++) {
    if (largecache[i] != NULL) {
      // Cached entry found
      // printf("Found cached entry in bin %zu KiB for alloc_size %zu KiB\n",
      //        class_to_size_large(class_idx) >> 10, needed >> 10);
      /// TODO: if the bin is substantially bigger than requested, slice
      /// the span and cache the remainer.
      found = true;
      large_hdr_t *entry = largecache[i];
      largecache[i] = entry->next;
      pthread_spin_unlock(&large_lock);

      entry->next = NULL;
      entry->magic = LARGE_MAGIC;
      entry->total_size = class_to_size_large(i);
      return (char *)entry + hdr_offset;
    }
  }
  pthread_spin_unlock(&large_lock);

  // Over-allocate + trim, to ensure alignment
  size_t map_size = bin_size + SPAN_SIZE;
  void *raw = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (unlikely(raw == MAP_FAILED))
    return NULL;

  uintptr_t addr = (uintptr_t)raw;
  uintptr_t aligned = (addr + SPAN_SIZE - 1) & SPAN_MASK;

  // Trim leading excess
  size_t leading = aligned - addr;
  if (leading > 0)
    munmap(raw, leading);

  // Trim trailing excess
  size_t total_from_aligned = map_size - leading;
  if (total_from_aligned > bin_size)
    munmap((void *)(aligned + bin_size), total_from_aligned - bin_size);

  large_hdr_t *hdr = (large_hdr_t *)aligned;
  hdr->magic = LARGE_MAGIC;
  hdr->total_size = bin_size;

  return (char *)aligned + hdr_offset;
}

__attribute__((cold)) void large_free(void *ptr) {
  large_hdr_t *hdr = (large_hdr_t *)((uintptr_t)ptr & SPAN_MASK);
  size_t total = hdr->total_size;
  int class_idx = size_to_class_large(total);

  pthread_spin_lock(&large_lock);
  hdr->next = largecache[class_idx];
  largecache[class_idx] = hdr;
  // Release physical pages to the OS, preserving the header.
  madvise((char *)hdr + system_page_size, total - (size_t)system_page_size,
          MADV_DONTNEED);
  pthread_spin_unlock(&large_lock);
}
