#include "internal.h"

/*
 * Size-class table:
 *   class 0  ->     16 B
 *   class 1  ->     32 B
 *   class 2  ->     64 B
 *   ...
 *   class 11 ->  32768 B  (32 KB)
 *   class 12 ->  65536 B  (64 KB)
 *   class 13 -> 131072 B (128 KB)
 *   class 14 -> 262144 B (256 KB)
 */
static const size_t class_sizes[NUM_SIZE_CLASSES] = {
    16,   32,   64,    128,   256,   512,    1024,  2048,
    4096, 8192, 16384, 32768, 65536, 131072, 262144};

/**
 * Map a requested size to a size-class.
 *
 * Returns: size-class index.
 */
int size_to_class(size_t size) {
  if (size <= MIN_ALLOC)
    return 0;

  // Round size up to next power of 2, then compute log2.
  size_t rounded = size - 1;
  int bits = (int)(sizeof(size_t) * 8) - __builtin_clzl(rounded);

  int sc = bits - MIN_ALLOC_LOG2;
  if (sc < 0)
    sc = 0;
  if (sc >= NUM_SIZE_CLASSES)
    sc = NUM_SIZE_CLASSES - 1;

  return sc;
}

size_t class_to_size(int sc) {
  if (sc < 0)
    return class_sizes[0];
  if (sc >= NUM_SIZE_CLASSES)
    return class_sizes[NUM_SIZE_CLASSES - 1];
  return class_sizes[sc];
}