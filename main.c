#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "reclaim.h"

#define NUM_THREADS 16
#define OPS_PER_THREAD 1000000
#define MAX_LIVE 256 // max simultaneous allocations per thread

// Simple xorshift64 PRNG (per-thread, no shared state)
static uint64_t xorshift64(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

static void *worker(void *arg) {
  int id = (int)(intptr_t)arg;
  void *live[MAX_LIVE];
  int nlive = 0;
  uint64_t rng = (uint64_t)id * 6364136223846793005ULL + 1;

  for (int i = 0; i < OPS_PER_THREAD; i++) {
    uint64_t r = xorshift64(&rng);

    if (nlive < MAX_LIVE / 2 || (nlive < MAX_LIVE && (r & 1))) {
      // Allocate: pick a size from 1 to 40000 (spanning small + large)
      size_t sz = (size_t)(xorshift64(&rng) % 40000) + 1;
      void *p = recl_malloc(sz);
      if (!p) {
        fprintf(stderr, "thread %d: recl_malloc(%zu) failed at op %d\n", id, sz,
                i);
        continue;
      }
      // printf("buffer allocated at %p\n", p);
      // Write a canary pattern to detect corruption
      memset(p, (unsigned char)(id ^ i), sz < 64 ? sz : 64);
      live[nlive++] = p;
    } else {
      // Free a random live allocation
      int idx = (int)(xorshift64(&rng) % (uint64_t)nlive);
      recl_free(live[idx]);
      live[idx] = live[--nlive];
    }
  }

  // Free remaining
  for (int i = 0; i < nlive; i++)
    recl_free(live[i]);

  return NULL;
}

int main(void) {
  printf("reclaim stress test: %d threads x %d ops\n", NUM_THREADS,
         OPS_PER_THREAD);

  recl_alloc_main_heap();

  pthread_t threads[NUM_THREADS];
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < NUM_THREADS; i++)
    pthread_create(&threads[i], NULL, worker, (void *)(intptr_t)i);

  for (int i = 0; i < NUM_THREADS; i++)
    pthread_join(threads[i], NULL);

  clock_gettime(CLOCK_MONOTONIC, &t1);

  double elapsed =
      (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
  long total_ops = (long)NUM_THREADS * OPS_PER_THREAD;
  printf("completed %ld ops in %.3f s  (%.0f ops/s)\n", total_ops, elapsed,
         (double)total_ops / elapsed);

  return EXIT_SUCCESS;
}