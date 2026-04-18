// Benchmark suite.
// Build: make bench
// Run: ./build/bench [--benchmark_filter=<regex>]
// Run: ./build/bench --benchmark_out=results.json --benchmark_out_format=json

#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

extern "C" {
#include "reclaim.h"
}

static inline uint64_t xorshift64(uint64_t *state) {
  // glibc's rand() has shared state, therefore it is not thread-safe.
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

// === BM_MixedAllocFree ===
// Each benchmark iteration is one alloc or free decision.
static void BM_MixedAllocFree(benchmark::State &state) {
  // Benchmark parameters
  static constexpr int MAX_LIVE = 256;

  const int id = state.thread_index();
  uint64_t rng = (uint64_t)id * 6364136223846793005ULL + 1;
  void *live[MAX_LIVE];
  int nlive = 0;

  for (auto _ : state) {
    uint64_t r = xorshift64(&rng);

    if (nlive < MAX_LIVE / 2 || (nlive < MAX_LIVE && (r & 1))) {
      // Allocate
      size_t sz = (size_t)(xorshift64(&rng) % 40000) + 1;
      void *p = recl_malloc(sz);
      if (p) {
        // Touch first bytes to ensure the page is faulted in
        static_cast<char *>(p)[0] = (char)(id ^ nlive);
        live[nlive++] = p;
      }
    } else {
      // Free a previous allocation at random
      int idx = (int)(xorshift64(&rng) % (uint64_t)nlive);
      recl_free(live[idx]);
      live[idx] = live[--nlive];
    }
  }

  // Free all remaining memory
  for (int i = 0; i < nlive; i++)
    recl_free(live[i]);

  // Report ops per second
  state.SetItemsProcessed(state.iterations());
}

// Register benchmark for multi and single-thread workloads.
BENCHMARK(BM_MixedAllocFree)
    ->Threads(1)
    ->Threads(16)
    ->Iterations(1000000)
    ->Unit(benchmark::kMicrosecond);

// === malloc_large ===
// This benchmark has been reimplemented from the well-assessed mimalloc-bench
// suite. Tests allocation of large blocks (5 to 25 MiB) with up to 20 live at
// any time.
static void BM_Malloc_Large(benchmark::State &state) {
  // Benchmark parameters
  static const int MALLOC_LARGE_MAX_LIVE = 20;
  static const int MALLOC_LARGE_MIN_BUFSIZE = 5 * 1024 * 1024;
  static const int MALLOC_LARGE_MAX_BUFSIZE = 25 * 1024 * 1024;

  const int id = state.thread_index();
  uint64_t rng = (uint64_t)id * 6364136223846793005ULL + 1;

  //   std::mt19937 rng(42); // rd());
  //   std::uniform_int_distribution<>
  //   size_distribution(MALLOC_LARGE_MIN_BUFSIZE,
  //                                                     MALLOC_LARGE_MAX_BUFSIZE);
  //   std::uniform_int_distribution<> buf_number_distribution(
  //       0, MALLOC_LARGE_NUM_BUFFERS - 1);
  char *live[MALLOC_LARGE_MAX_LIVE] = {NULL};

  for (auto _ : state) {
    size_t buf_idx = xorshift64(&rng) % MALLOC_LARGE_MAX_LIVE;
    size_t rnd_size = MALLOC_LARGE_MIN_BUFSIZE +
                      xorshift64(&rng) %
                          (MALLOC_LARGE_MAX_BUFSIZE - MALLOC_LARGE_MIN_BUFSIZE);
    if (live[buf_idx] != NULL) {
      recl_free(live[buf_idx]);
    }
    live[buf_idx] = (char *)recl_malloc(rnd_size);
  }

  // Free all remaining memory
  for (int i = 0; i < MALLOC_LARGE_MAX_LIVE; i++)
    if (live[i] != NULL)
      recl_free(live[i]);

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Malloc_Large)
    ->Threads(1)
    ->Threads(16)
    ->Iterations(2000)
    ->Unit(benchmark::kMicrosecond);

int main(int argc, char **argv) {
  recl_alloc_main_heap();

  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  recl_free_main_heap();

  return 0;
}
