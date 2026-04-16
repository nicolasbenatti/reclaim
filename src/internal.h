#ifndef INTERNAL_H
#define INTERNAL_H

#include <stddef.h>

/// Chunk.
typedef struct {
  size_t prv_size;
  // Chunks are multiple of 8 bytes, so the last three bits
  // encode status information:
  // A: 0 - chunk comes from main arena
  //    1 - chunk comes form an mmap'd arena
  // M: the chunk was allocated via mmap, and does not belong to any heap
  // P: previous chunk is in use (if 1, `prv_size` is ignored)
  size_t size;

  // Pointers to previous and next chunk in the freelist
  // (only used when the chunk is free'd)
  void *next;
  void *prv;
} chunk_t;

/// Metadata stored in a heap VMA
typedef struct {

} heap_info_t;

/// Return the size of a chunk
size_t chunk_size_get(chunk_t *chk);

/// Return the value of the A bit for a chunk
_Bool chunk_main_arena(chunk_t *chk);
/// Return the value of the M bit for a chunk
_Bool chunk_is_mmapped(chunk_t *chk);
/// Return the value of the P bit for a chunk
_Bool chunk_prev_inuse(chunk_t *chk);

/// Return whether a chunk is free
_Bool chunk_is_free(chunk_t *chk);

#endif // INTERNAL_H
