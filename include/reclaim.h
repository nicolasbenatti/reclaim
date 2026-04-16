#ifndef RECLAIM_H
#define RECLAIM_H

#include <stddef.h>

/// Utility function to reserve memory beforehand
void recl_alloc_main_heap(void);

void *recl_malloc(size_t size);

void recl_free(void *ptr);

#endif // RECLAIM_H
