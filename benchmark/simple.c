// === simple ===
// Simple allocation benchmark inspired to ltalloc,
// in which threads repeatedly allocate and free
// fixed-size chunks.
//
// Build: make bench
// Run:   ./build/bench_simple

#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define _rand xorshift64

// Config parameters
#define MAX_THREADS 32
#define CHUNK_SIZE_BYTES 128

typedef struct {
  int is_glibc;
  size_t t_idx;
  size_t n_iter;
  stats_t stats;
} thread_data;

static void *run_benchmark(void *__data) {
  thread_data *data = (thread_data *)__data;

  data->stats.n_allocs = 0;
  data->stats.n_frees = 0;

  for (int64_t _iter = 0; _iter < data->n_iter; _iter++) {
    char *p = (char *)((data->is_glibc) ? malloc(CHUNK_SIZE_BYTES)
                                        : recl_malloc(CHUNK_SIZE_BYTES));
    data->stats.n_allocs++;
    (data->is_glibc) ? free(p) : recl_free(p);
    data->stats.n_frees++;
  }
  return NULL;
}

int main(int argc, char **argv) {
  int nthreads, n_iter, is_glibc = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      nthreads = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
      n_iter = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--glibc") == 0) {
      is_glibc = 1;
    } else {
      fprintf(stderr, "Usage: %s [--threads N] [--iterations N] [--glibc]\n",
              argv[0]);
      return 1;
    }
  }

  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "error: --threads must be between 1 and %d\n", MAX_THREADS);
    return 1;
  }

  static pthread_t threads[MAX_THREADS];
  thread_data args[MAX_THREADS];

  recl_alloc_main_heap();

  struct timespec wall0, wall1;
  clock_gettime(CLOCK_MONOTONIC, &wall0);

  for (int i = 0; i < nthreads; i++) {
    args[i].is_glibc = is_glibc;
    args[i].t_idx = (size_t)i;
    args[i].n_iter = (size_t)n_iter;
    pthread_create(&threads[i], NULL, run_benchmark, &args[i]);
  }
  for (int i = 0; i < nthreads; i++)
    pthread_join(threads[i], NULL);

  clock_gettime(CLOCK_MONOTONIC, &wall1);

  double wall_us = ((double)(wall1.tv_sec - wall0.tv_sec) * 1e9 +
                    (double)(wall1.tv_nsec - wall0.tv_nsec)) /
                   1e3;
  double us_per_op = wall_us / (double)n_iter;
  double wall_s = wall_us / 1e6;

  uint64_t total_allocs = 0;
  for (int i = 0; i < nthreads; i++)
    total_allocs += args[i].stats.n_allocs;
  double throughput = (wall_s > 0.0) ? (double)total_allocs / wall_s : 0.0;

  char label[128];
  bench_print_header();
  snprintf(label, sizeof(label), "%s/threads:%d",
           is_glibc ? "BM_simple_glibc" : "BM_simple", nthreads);
  printf("%-45s %10.3f us %12lld %.3f\n", label, us_per_op, (long long)n_iter,
         throughput);

  recl_free_main_heap();

  return 0;
}
