#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Span.
 *
 * Each span is a contiguous 2MiB mmap'd region, 2MiB-aligned.
 * Spans serve as a basis storing objects, and they are all fixed size.
 */
typedef struct span {
  uint32_t magic;      // magic number
  uint32_t size_class; // index into size-class table
  uint32_t size;
  uint32_t total_objects; // total objects carved from span
  void *base;
  struct span *next; // relative scache
} span_t;

/**
 * Large allocation header.
 */
typedef struct large {
  uint32_t magic;     // LARGE_MAGIC for identification
  size_t total_size;  // total mmap size incl. header
  struct large *next; // Next large span in hugecache
} large_hdr_t;

/**
 * Thread-local cache (tcache).
 */
typedef struct {
  void *bin; // freelist
  int count; // cached object count
} tcache_bin_t;

typedef struct {
  bool initialized;
  tcache_bin_t bins[NUM_SIZE_CLASSES];
} tcache_t;

// Central cache (ccache).
//
// Each bin is padded to a full cache line so that adjacent
// size-class bins accessed by different threads never share
// the same cache line (prevents false sharing).
typedef struct {
  pthread_spinlock_t lock;
  void *head; // freelist
  int count;
} __attribute__((aligned(CACHE_LINE_SIZE))) central_bin_t;

// Size classes.
//
// There are no lookup tables for computing size classed; this is
// to avoid any memory access for such a heavily needed operation.
// This is true for both normal and large allocations.

static inline __attribute__((always_inline)) int size_to_class(size_t size) {
  if (size <= MIN_ALLOC)
    return 0;
  if (size <= 32)
    return 1;
  if (size <= 48)
    return 2;
  if (size <= 64)
    return 3;
  int k = 63 - __builtin_clzl(size - 1);
  return (k << 2) + (int)((size - 1) >> (k - 2)) - 24;
}

static inline __attribute__((always_inline)) size_t class_to_size(int sc) {
  if (sc == 0)
    return 16;
  if (sc == 1)
    return 32;
  if (sc == 2)
    return 48;
  return (size_t)(4 + ((sc - 3) & 3)) << (((sc - 3) >> 2) + 4);
}

static inline __attribute__((always_inline)) int
size_to_class_large(size_t size) {
  if (size <= LARGE_THRESHOLD)
    return 0;
  int bits = (int)(sizeof(size_t) * 8) - __builtin_clzl(size - 1);
  int sc = bits - MIN_ALLOC_LARGE_LOG2;
  return sc < NUM_LARGE_CLASSES ? sc : NUM_LARGE_CLASSES - 1;
}

static inline __attribute__((always_inline)) size_t
class_to_size_large(int sc) {
  return (size_t)1 << (sc + MIN_ALLOC_LARGE_LOG2);
}

void backend_init(void);
void backend_deinit(void);

span_t *span_alloc(int size_class);

void span_release(span_t *s);

__attribute__((cold, malloc, alloc_size(1))) void *large_alloc(size_t size);

__attribute__((cold)) void large_free(void *ptr);

void ccache_init(void);
void ccache_deinit(void);
void *ccache_fetch(int sc, int batch, int *out_count);
void ccache_return(int sc, void *list, void *tail, int count);

#endif // INTERNAL_H
