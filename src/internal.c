#include "internal.h"
#include <stddef.h>
#include <stdint.h>

#define A_BITMASK 0x00000004
#define M_BITMASK 0x00000002
#define P_BITMASK 0x00000001
#define SIZE_BITMASK 0xfffffff8

_Bool chunk_main_arena(chunk_t *chk) { return !!(chk->size & A_BITMASK); }
_Bool chunk_is_mmapped(chunk_t *chk) { return !!(chk->size & M_BITMASK); }
_Bool chunk_prev_inuse(chunk_t *chk) { return !!(chk->size & P_BITMASK); }

size_t chunk_size_get(chunk_t *chk) { return chk->size & SIZE_BITMASK; }

_Bool chunk_is_free(chunk_t *chk) {
  // Get address of the next chunk
  chunk_t *next = (chunk_t *)((uintptr_t)chk + chunk_size_get(chk));

  // Look at the prev's P bit
  return chunk_prev_inuse(next);
}