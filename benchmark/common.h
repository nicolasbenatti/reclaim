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
         "Xput (alloc/s)");
  printf("-------------------------------------------"
         "--------------------------------------------------\n");
}

static FILE *bench_csv_open(const char *csv_path, const char *header) {
  if (!csv_path)
    return NULL;
  int write_header = 0;
  FILE *f = fopen(csv_path, "r");
  if (!f)
    write_header = 1;
  else
    fclose(f);
  f = fopen(csv_path, "a");
  if (!f) {
    fprintf(stderr, "warning: cannot open %s for writing\n", csv_path);
    return NULL;
  }
  if (write_header)
    fprintf(f, "%s\n", header);
  return f;
}

#endif // BENCH_COMMON_H
