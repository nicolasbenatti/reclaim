#include "reclaim.h"
#include <stddef.h>
#include <unistd.h>

#define MAIN_ARENA_BASE 0x400000000000
#define MAIN_ARENA_LEN 0x4000 // 16KB

static 

/// Expands the arena by `nbytes` bytes
static int _expand(size_t nbytes) {}

/// Shrinks the arena by `nbytes` bytes
static int _shrink(size_t nbytes) {}

/// Initialise allocator data structures.
/// This function is lazily called
/// at the first allocation request.
static void _init() {

}

void *malloc(size_t size) {
  // Chop off a chunk of at least 'size' bytes
  // from the top chunk
}

void free(void *ptr) {}