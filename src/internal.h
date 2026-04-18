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
  uint32_t total_objects;  // total objects carved from span
  atomic_uint alloc_count; // Objects currently outstanding
  void *base;
  struct span *next; // relative scache
} span_t;

/**
 * Large allocation header.
 */
typedef struct {
  uint32_t magic;    // LARGE_MAGIC for identification
  size_t total_size; // total mmap size incl. header
} large_hdr_t;

/**
 * Thread-local cache (tcache).
 */
typedef struct {
  void *bins[NUM_SIZE_CLASSES]; // singly-linked free list head
  int count[NUM_SIZE_CLASSES];  // cached object count per bin
  bool initialized;
} tcache_t;

/**
 * Central cache (ccache).
 */
typedef struct {
  pthread_spinlock_t lock;
  void *head; // singly-linked free list
  int count;
} central_bin_t;

// Map a request size (1..LARGE_THRESHOLD) to a size-class index [0..11]
int size_to_class(size_t size);

// Map a size-class index to the actual allocation size
size_t class_to_size(int sc);

void backend_init(void);
void backend_deinit(void);

span_t *span_alloc(int size_class);
void span_release(span_t *s);
void *large_alloc(size_t size);
void large_free(void *ptr);

void ccache_init(void);
void ccache_deinit(void);
void *central_fetch(int sc, int batch, int *out_count);
void central_return(int sc, void *list, int count);

#endif // INTERNAL_H
