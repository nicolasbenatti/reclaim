#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "reclaim.h"

typedef struct {
  uint64_t n_allocs;
  uint64_t n_frees;
} stats_t;

static inline uint64_t xorshift64(uint64_t *state) {
  // glibc's rand() has shared state, therefore it is not thread-safe.
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

static void bench_print_header(void) {
  printf("%-45s %10s    %12s %20s\n", "Benchmark", "Time", "Iterations",
         "throughput (allocs/s)");
  printf("-------------------------------------------"
         "---------------------------------------------------\n");
}

#endif // BENCH_COMMON_H
