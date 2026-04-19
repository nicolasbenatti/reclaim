// === larson ===
// Build: make bench
// Run:   ./build/bench_larson

#include "common.h"
#include "reclaim.h"
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define _rand xorshift64

// Config parameters
#define MAX_THREADS 100
#define MAX_BLOCKS 20000000
#define MAX_PER_THREAD_ALLOCS 8192
#define CHUNK_SIZE_BYTES 64

int is_glibc = 0;
char **blkp;
int *blksize;
volatile int stopflag = 0;

typedef struct {
  int is_glibc;
  size_t t_idx;
  size_t n_threads;
  size_t n_iter;          // No. of loop iterations
  size_t chks_per_thread; // No. of per-thread chunks
  size_t n_allocs;        // No. of per-thread allocations
  size_t chk_size_min;    // Min size of each chunk
  size_t chk_size_max;    // Max size of each chunk
  int *blksize;           // Ptr to block size tracking array
  char **array;           // Ptr to chunks to operate on
  volatile int finished;  // Signals thread completion
  stats_t stats;
} thread_data;

static pthread_t threads[MAX_THREADS];
// Holds ptrs to the internally-spawn threads that perform remote deallocations
static pthread_t terminator_threads[MAX_THREADS];
thread_data args[MAX_THREADS];

static void warmup(char **blkp, int *blksize, size_t n_chks,
                   size_t chk_size_min, size_t chk_size_max, uint64_t *rng) {
  for (int64_t i = 0; i < n_chks; i++) {
    uint64_t size = chk_size_min + _rand(rng) % (chk_size_max - chk_size_min);
    // Pre-allocate random-size chunks
    blkp[i] = (char *)((is_glibc) ? malloc(size) : recl_malloc(size));
    blksize[i] = size;
  }

  int victim;
  void *tmp;
  // Randomly permutate the chunks
  for (uint64_t i = n_chks; i > 0; i--) {
    victim = _rand(rng) % n_chks;
    tmp = blkp[victim];
    blkp[victim] = blkp[i - 1];
    blkp[i - 1] = (char *)tmp;
  }

  // Perform random replacements (malloc-free operations)
  for (uint64_t i = 0; i < 4 * n_chks; i++) {
    victim = _rand(rng) % n_chks;
    (is_glibc) ? free(blkp[victim]) : recl_free(blkp[victim]);
    uint64_t size = chk_size_min + _rand(rng) % (chk_size_max - chk_size_min);
    blkp[victim] = (char *)((is_glibc) ? malloc(size) : recl_malloc(size));
    blksize[victim] = size;
  }
}

static void *run_benchmark(void *__data) {
  thread_data *data = (thread_data *)__data;

  if (stopflag)
    return (void *)0;

  const int id = data->t_idx;
  uint64_t rng = (uint64_t)id * 6364136223846793005ULL + 1;
  int victim;

  data->finished = 0;
  data->stats.n_allocs = 0;
  data->stats.n_frees = 0;

  // Perform random replacements
  for (uint64_t i = 0; i < data->n_allocs; i++) {
    victim = _rand(&rng) % data->chks_per_thread;
    (data->is_glibc) ? free(data->array[victim])
                     : recl_free(data->array[victim]);
    data->stats.n_frees++;
    uint64_t size = data->chk_size_min +
                    _rand(&rng) % (data->chk_size_max - data->chk_size_min);
    data->array[victim] =
        (char *)((data->is_glibc) ? malloc(size) : recl_malloc(size));
    data->stats.n_allocs++;
    data->blksize[victim] = size;

    // Do work
    volatile char *chptr = ((char *)data->array[victim]);
    *chptr++ = 'a';
    volatile char ch = *((char *)data->array[victim]);
    *chptr = 'b';

    if (stopflag)
      break;
  }

  data->finished = 1;

  if (!stopflag) {
    // Before terminating, spawn a new thread to handle remote deallocations
    // (i.e., non-local to the thread)
    pthread_create(&terminator_threads[data->t_idx - 1], NULL, run_benchmark,
                   data);
  }

  pthread_exit(NULL);
  return (void *)0;
}

int main(int argc, char **argv) {
  int nthreads = 1;
  int64_t n_iter = 50;
  int64_t chks_per_thread = 100;
  int64_t chk_size_min = 10;
  int64_t chk_size_max = 500;
  long sleep_cnt = 10; // runtime in seconds

  const char *csv_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      nthreads = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
      n_iter = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--chunks-per-thread") == 0 && i + 1 < argc) {
      chks_per_thread = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--min-size") == 0 && i + 1 < argc) {
      chk_size_min = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--max-size") == 0 && i + 1 < argc) {
      chk_size_max = atoll(argv[++i]);
    } else if (strcmp(argv[i], "--sleep") == 0 && i + 1 < argc) {
      sleep_cnt = atol(argv[++i]);
    } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
      csv_path = argv[++i];
    } else if (strcmp(argv[i], "--glibc") == 0) {
      is_glibc = 1;
    } else {
      fprintf(
          stderr,
          "Usage: %s [--threads N] [--iterations N] [--chunks-per-thread N]"
          " [--min-size N] [--max-size N] [--sleep N] [--csv FILE] [--glibc]\n",
          argv[0]);
      return 1;
    }
  }

  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "error: --threads must be between 1 and %d\n", MAX_THREADS);
    return 1;
  }

  if (chk_size_min > chk_size_max) {
    fprintf(stderr, "error: --min-size must be <= --max-size\n");
    return 1;
  }

  blkp = (char **)((is_glibc) ? malloc(MAX_BLOCKS * sizeof(char *))
                              : recl_malloc(MAX_BLOCKS * sizeof(char *)));
  blksize = (int *)((is_glibc) ? malloc(MAX_BLOCKS * sizeof(int))
                               : recl_malloc(MAX_BLOCKS * sizeof(int)));

  if (!is_glibc)
    recl_alloc_main_heap();

  // Pre-alocate chunks
  uint64_t rng = (uint64_t)985349838678833ULL;
  warmup(blkp, blksize, nthreads * chks_per_thread, chk_size_min, chk_size_max,
         &rng);

  struct timespec wall0, wall1;
  clock_gettime(CLOCK_MONOTONIC, &wall0);

  stopflag = 0;
  for (int i = 0; i < nthreads; i++) {
    args[i].is_glibc = is_glibc;
    args[i].t_idx = (size_t)i + 1;
    args[i].n_threads = (size_t)nthreads;
    args[i].n_iter = (size_t)n_iter;
    args[i].n_allocs = (size_t)n_iter * chks_per_thread;
    args[i].chks_per_thread = (size_t)chks_per_thread;
    args[i].chk_size_min = (size_t)chk_size_min;
    args[i].chk_size_max = (size_t)chk_size_max;
    args[i].array = &blkp[i * chks_per_thread];
    args[i].blksize = &blksize[i * chks_per_thread];
    args[i].finished = 0;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&threads[i], &attr, run_benchmark, &args[i]);
    pthread_attr_destroy(&attr);
  }
  // Let threads run for the specified duration, then signal stop
  sleep(sleep_cnt);
  stopflag = 1;

  for (int i = 0; i < nthreads; i++) {
    while (!args[i].finished)
      sched_yield();
  }

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
  snprintf(label, sizeof(label), "%s/threads:%d/%s", "larson", nthreads,
           is_glibc ? "glibc" : "reclaim");
  printf("%-45s %10.3f us %12lld %20.3f\n", label, us_per_op, (long long)n_iter,
         throughput);
  bench_csv_append(csv_path, "larson", us_per_op, (long long)n_iter, throughput,
                   is_glibc, nthreads);

  if (!is_glibc)
    recl_free_main_heap();

  return 0;
}
