#include "reclaim.h"
#include "internal.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAIN_HEAP_BASE 0x400000000000
#define MAIN_HEAP_LEN 0x4000 // 16KB
#define NUM_BINS 8

static char *top;
static chunk_t *bins[NUM_BINS];

/// Expands the arena by `nbytes` bytes
static int _expand(size_t nbytes) {}

/// Shrinks the arena by `nbytes` bytes
static int _shrink(size_t nbytes) {}

/// Initialise allocator data structures.
/// This function is lazily called
/// at the first allocation request.
static void _init() {
  printf("libreclaim: performing initial allocation of heap at %p\n",
         (void *)MAIN_HEAP_BASE);
  top = mmap((void *)MAIN_HEAP_BASE, MAIN_HEAP_LEN, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  if (top != MAP_FAILED) {
    printf("libreclaim: top chunk allocated at %p\n", top);
  } else {
    printf("libreclaim: mmap failed when allocating heap: %s\n",
           strerror(errno));
  }
}

void recl_alloc_main_heap(void) { _init(); }

void *recl_malloc(size_t size) {
  printf("libreclaim: malloc called.\n");
  // Chop off a chunk of at least 'size' bytes
  // from the top chunk
  if (((chunk_t *)top)->size == 0x0) {
    printf("libreclaim: first allocation.\n");
  }

  return top;
}

void recl_free(void *ptr) {}