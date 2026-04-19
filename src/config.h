#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

// Each span is a 2MiB mmap'd region, 2MiB-aligned.
#define SPAN_SIZE ((size_t)(1 << 21))
#define SPAN_ALIGN SPAN_SIZE
// Span lookup from any chunk ptr: ptr & SPAN_MASK
#define SPAN_MASK (~(SPAN_SIZE - 1))

/*
 * Reclaim employs 15 size classes, each double of the previous, ranging
 * from 16B to 256KiB.
 */
#define NUM_SIZE_CLASSES 15
#define MIN_ALLOC ((size_t)16)
#define MIN_ALLOC_LOG2 4

// Large classes start from 512KiB
#define NUM_LARGE_CLASSES 8
#define MIN_ALLOC_LARGE_LOG2 19

/*
 * Threshold for considering a request as a large allocation.
 */
#define LARGE_THRESHOLD ((size_t)(256 * 1024))

// No. of objects moved between tcaches and ccache in a single batch transfer.
#define BATCH_SIZE 32
// Maximum objects in a tcache bin
#define MAX_CACHED 64

// Magic number for spans.
#define SPAN_MAGIC ((uint32_t)0x5043414C)

// Magic number for large allocations.
#define LARGE_MAGIC ((uint32_t)0x4C524543)

#endif // CONFIG_H