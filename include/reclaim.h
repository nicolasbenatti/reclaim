#ifndef RECLAIM_H
#define RECLAIM_H

#include <stddef.h>

// Allocate main working area.
//
// It is called on first recl_malloc(), but may be called
// explicitly to reserve a memory pool beforehand.
void recl_alloc_main_heap(void);

// Deallocate main working area.
//
// This function must be manually called to clear the
// memory pool reserved beforehand.
void recl_free_main_heap(void);

__attribute__((hot, malloc, alloc_size(1), assume_aligned(16)))
void *recl_malloc(size_t size);

__attribute__((hot))
void recl_free(void *ptr);

#endif // RECLAIM_H
