// === malloc_large ===
// This benchmark has been reimplemented from the well-assessed mimalloc-bench
// suite. Tests allocation of large blocks (5 to 25 MiB) with up to 20 live at
// any time.
//
// Build: make bench
// Run:   ./build/bench_malloc_large

#include "common.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define _rand xorshift64

// Config parameters
#define MAX_THREADS 32
#define MALLOC_LARGE_MAX_LIVE 20
#define MALLOC_LARGE_MIN_BUFSIZE 5 * 1024 * 102
#define MALLOC_LARGE_MAX_BUFSIZE 25 * 1024 * 1024

typedef struct {
  int is_glibc;
  size_t t_idx;
  size_t n_iter;
} thread_data;

void *run_benchmark(void *__data) {
  thread_data *data = (thread_data *)__data;

  const int id = data->t_idx;
  uint64_t rng = (uint64_t)id * 6364136223846793005ULL + 1;

  char *live[MALLOC_LARGE_MAX_LIVE];
  memset(live, 0, sizeof(live));

  for (int64_t _iter = 0; _iter < data->n_iter; _iter++) {
    size_t buf_idx = _rand(&rng) % MALLOC_LARGE_MAX_LIVE;
    size_t rnd_size =
        MALLOC_LARGE_MIN_BUFSIZE +
        _rand(&rng) % (MALLOC_LARGE_MAX_BUFSIZE - MALLOC_LARGE_MIN_BUFSIZE);
    if (live[buf_idx] != NULL) {
      (data->is_glibc) ? free(live[buf_idx]) : recl_free(live[buf_idx]);
    }
    live[buf_idx] =
        (char *)((data->is_glibc) ? malloc(rnd_size) : recl_malloc(rnd_size));
  }

  // Free all remaining memory
  for (int i = 0; i < MALLOC_LARGE_MAX_LIVE; i++)
    if (live[i] != NULL)
      (data->is_glibc) ? free(live[i]) : recl_free(live[i]);

  return (void *)0;
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

  char label[128];
  bench_print_header();
  snprintf(label, sizeof(label), "%s/threads:%d",
           is_glibc ? "BM_malloc_large_glibc" : "BM_malloc_large", nthreads);
  printf("%-45s %10.3f us %12lld\n", label, us_per_op, (long long)n_iter);

  recl_free_main_heap();

  return 0;
}
