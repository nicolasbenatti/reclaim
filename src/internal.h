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
  void *bin;  // singly-linked free list head
  int count;  // cached object count
} tcache_bin_t;

typedef struct {
  bool initialized;
  tcache_bin_t bins[NUM_SIZE_CLASSES];
} tcache_t;

/**
 * Central cache (ccache).
 */
typedef struct {
  pthread_spinlock_t lock;
  void *head; // singly-linked free list
  int count;
} central_bin_t;

// Size-class tables
static const size_t class_sizes[NUM_SIZE_CLASSES] = {
    16,   32,   64,    128,   256,   512,    1024,  2048,
    4096, 8192, 16384, 32768, 65536, 131072, 262144};

static const size_t large_sizes[NUM_LARGE_CLASSES] = {
    512 << 10,  1024 << 10,  2048 << 10,  4096 << 10,
    8192 << 10, 16384 << 10, 32768 << 10, 65536 << 10};

// Map a request size to a size-class index
static inline __attribute__((always_inline)) int size_to_class(size_t size) {
  if (size <= MIN_ALLOC)
    return 0;
  int bits = (int)(sizeof(size_t) * 8) - __builtin_clzl(size - 1);
  int sc = bits - MIN_ALLOC_LOG2;
  return sc < NUM_SIZE_CLASSES ? sc : NUM_SIZE_CLASSES - 1;
}

// Map a size-class index to the actual allocation size
static inline __attribute__((always_inline)) size_t class_to_size(int sc) {
  return class_sizes[sc];
}

// Same but for large classes
static inline __attribute__((always_inline)) int
size_to_class_large(size_t size) {
  if (size <= LARGE_THRESHOLD)
    return 0;
  int bits = (int)(sizeof(size_t) * 8) - __builtin_clzl(size - 1);
  int sc = bits - MIN_ALLOC_LARGE_LOG2;
  if (sc < 0)
    sc = 0;
  return sc < NUM_LARGE_CLASSES ? sc : NUM_LARGE_CLASSES - 1;
}

static inline __attribute__((always_inline)) size_t
class_to_size_large(int sc) {
  return large_sizes[sc];
}

void backend_init(void);
void backend_deinit(void);

span_t *span_alloc(int size_class);
void span_release(span_t *s);
void *large_alloc(size_t size);
void large_free(void *ptr);

void ccache_init(void);
void ccache_deinit(void);
void *ccache_fetch(int sc, int batch, int *out_count);
void ccache_return(int sc, void *list, void *tail, int count);

#endif // INTERNAL_H
