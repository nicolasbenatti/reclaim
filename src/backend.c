#include "internal.h"
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

void backend_init(void) {
  pthread_spin_init(&span_lock, PTHREAD_PROCESS_PRIVATE);
  scache = NULL;
}

/**
 * Allocate a span.
 *
 * Returns: address of the span.
 */
static void *mmap_span(void) {
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
  atomic_store_explicit(&s->alloc_count, 0, memory_order_relaxed);

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
  if (scache) {
    s = scache;
    scache = s->next;
  }
  pthread_spin_unlock(&span_lock);

  if (!s) {
    s = (span_t *)mmap_span();
    if (!s)
      return NULL;
  }

  span_init(s, size_class);

  return s;
}

// ------------------------------------------------------------------
//  Public: release a fully-empty span back to the cache
// ------------------------------------------------------------------

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
void *large_alloc(size_t size) {
  /*
   * Large allocations must be SPAN_SIZE-aligned so that
   * recl_free() can use (ptr & SPAN_MASK) to find the header
   * and distinguish large from span-based allocations.
   */
  size_t hdr_offset = sizeof(large_hdr_t);
  if (hdr_offset < 16)
    hdr_offset = 16;

  size_t needed = hdr_offset + size;
  long page = sysconf(_SC_PAGESIZE);
  needed = (needed + (size_t)page - 1) & ~((size_t)page - 1);

  // Over-allocate + trim, to ensure alignment
  size_t map_size = needed + SPAN_SIZE;
  void *raw = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (raw == MAP_FAILED)
    return NULL;

  uintptr_t addr = (uintptr_t)raw;
  uintptr_t aligned = (addr + SPAN_SIZE - 1) & SPAN_MASK;

  // Trim leading excess
  size_t leading = aligned - addr;
  if (leading > 0)
    munmap(raw, leading);

  // Trim trailing excess
  size_t total_from_aligned = map_size - leading;
  if (total_from_aligned > needed)
    munmap((void *)(aligned + needed), total_from_aligned - needed);

  large_hdr_t *hdr = (large_hdr_t *)aligned;
  hdr->magic = LARGE_MAGIC;
  hdr->total_size = needed;

  return (char *)aligned + hdr_offset;
}

void large_free(void *ptr) {
  large_hdr_t *hdr = (large_hdr_t *)((uintptr_t)ptr & SPAN_MASK);
  munmap(hdr, hdr->total_size);
}
