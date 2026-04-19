// === threadtest ===
// Each benchmark iteration is one alloc or free decision.
// Build: make bench
// Run:   ./build/bench_threadtest

#include "common.h"
#include "reclaim.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Config parameters
#define MAX_THREADS 128
#define MAX_PER_THREAD_ALLOCS 65536
#define CHUNK_SIZE_BYTES 64

typedef struct {
  int is_glibc;
  size_t t_idx;
  size_t n_threads;
  size_t n_iter;     // No. of loop iterations
  size_t n_chks;     // No. of chunks to allocate
  size_t chk_size;   // Size of each chunk
  size_t work_coeff; // Amount of work to perform
  stats_t stats;
} thread_data;

static void *run_benchmark(void *__data) {
  thread_data *data = (thread_data *)__data;

  uint8_t *chunks[MAX_PER_THREAD_ALLOCS];

  data->stats.n_allocs = 0;
  data->stats.n_frees = 0;

  for (int64_t j = 0; j < data->n_iter; j++) {
    for (int64_t i = 0; i < (data->n_chks / data->n_threads); i++) {
      // Alloc
      chunks[i] = (uint8_t *)((data->is_glibc) ? malloc(data->chk_size)
                                               : recl_malloc(data->chk_size));
      data->stats.n_allocs++;

      // Do work
      for (volatile int64_t d = 0; d < data->work_coeff; d++) {
        volatile int64_t f = 1;
        f = f + f;
        f = f * f;
        f = f + f;
        f = f * f;
      }
    }

    for (int64_t i = 0; i < (data->n_chks / data->n_threads); i++) {
      (data->is_glibc) ? free(chunks[i]) : recl_free(chunks[i]);
      data->stats.n_frees++;

      for (volatile int64_t d = 0; d < data->work_coeff; d++) {
        volatile int64_t f = 1;
        f = f + f;
        f = f * f;
        f = f + f;
        f = f * f;
      }
    }
  }

  return (void *)0;
}

int main(int argc, char **argv) {
  int nthreads = 1;
  int64_t n_iter = 50;
  int64_t n_chks = 30000;
  int64_t chk_size = CHUNK_SIZE_BYTES;
  int64_t work = 0;
  int is_glibc = 0;
  const char *csv_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      nthreads = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
      n_iter = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--chunks") == 0 && i + 1 < argc) {
      n_chks = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--chunk-size") == 0 && i + 1 < argc) {
      chk_size = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--work") == 0 && i + 1 < argc) {
      work = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
      csv_path = argv[++i];
    } else if (strcmp(argv[i], "--glibc") == 0) {
      is_glibc = 1;
    } else {
      fprintf(stderr,
              "Usage: %s [--threads N] [--iterations N] [--chunks N]"
              " [--chunk-size N] [--work N] [--csv FILE] [--glibc]\n",
              argv[0]);
      return 1;
    }
  }

  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "error: --threads must be between 1 and %d\n", MAX_THREADS);
    return 1;
  }

  if (n_chks / nthreads > MAX_PER_THREAD_ALLOCS) {
    fprintf(stderr,
            "error: chunks/threads exceeds MAX_PER_THREAD_ALLOCS (%d)\n",
            MAX_PER_THREAD_ALLOCS);
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
    args[i].n_threads = (size_t)nthreads;
    args[i].n_iter = (size_t)n_iter;
    args[i].n_chks = (size_t)n_chks;
    args[i].chk_size = (size_t)chk_size;
    args[i].work_coeff = (size_t)work;
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
  snprintf(label, sizeof(label), "%s/threads:%d/%s", "threadtest", nthreads,
           is_glibc ? "glibc" : "reclaim");
  printf("%-45s %10.3f us %12lld %20.3f\n", label, us_per_op, (long long)n_iter,
         throughput);
  FILE *csv = bench_csv_open(csv_path,
      "benchmark,n_threads,iterations,is_glibc,chunks_per_thread,chunk_size,work,time_us,throughput_alloc_per_s");
  if (csv) {
    fprintf(csv, "threadtest,%d,%lld,%d,%lld,%lld,%lld,%.3f,%.3f\n",
            nthreads, (long long)n_iter, is_glibc,
            (long long)n_chks / nthreads, (long long)chk_size, (long long)work,
            us_per_op, throughput);
    fclose(csv);
  }

  recl_free_main_heap();
  return 0;
}
