#ifndef RECLAIM_H
#define RECLAIM_H

#include <stddef.h>

/*
 * Allocate main working area.
 * 
 * It is called on first recl_malloc(), but may be called
 * explicitly to reserve a memory pool beforehand.
 */
void recl_alloc_main_heap(void);

void *recl_malloc(size_t size);

void recl_free(void *ptr);

#endif // RECLAIM_H
