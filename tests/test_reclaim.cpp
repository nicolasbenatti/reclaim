#include <gtest/gtest.h>

extern "C" {
#include "reclaim.h"
#include "config.h"
}

#include <cstring>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// All 15 size-class sizes: 16, 32, 64, ..., 256 KiB
static constexpr size_t kSizeClasses[NUM_SIZE_CLASSES] = {
    16, 32, 64, 128, 256, 512,
    1024, 2048, 4096, 8192, 16384, 32768,
    65536, 131072, 262144 // 256 KiB == LARGE_THRESHOLD
};

// ---------------------------------------------------------------------------
// 1. recl_malloc(1) returns a non-null pointer
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, MallocReturnsNonNull) {
    void *p = recl_malloc(1);
    ASSERT_NE(p, nullptr);
    std::cout << "  recl_malloc(1) -> " << p << "\n";
    recl_free(p);
}

// ---------------------------------------------------------------------------
// 2. recl_malloc(0) is treated as size 1 and still returns non-null
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, MallocZeroTreatedAsOne) {
    void *p = recl_malloc(0);
    ASSERT_NE(p, nullptr);
    std::cout << "  recl_malloc(0) -> " << p << " (treated as size 1)\n";
    recl_free(p);
}

// ---------------------------------------------------------------------------
// 3. recl_free(nullptr) must not crash
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, FreeNullIsNoop) {
    EXPECT_NO_FATAL_FAILURE(recl_free(nullptr));
    std::cout << "  recl_free(nullptr) returned without crash\n";
}

// ---------------------------------------------------------------------------
// 4. Every pointer returned by the small path is at least 16-byte aligned
//    (MIN_ALLOC == 16 guarantees this for all size classes)
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, AlignmentSmall) {
    for (size_t sc_size : kSizeClasses) {
        void *p = recl_malloc(sc_size);
        ASSERT_NE(p, nullptr) << "size=" << sc_size;
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        EXPECT_EQ(addr % MIN_ALLOC, 0u) << "pointer misaligned for size=" << sc_size;
        std::cout << "  size=" << sc_size << " -> " << p
                  << " (offset mod 16 = " << addr % MIN_ALLOC << ")\n";
        recl_free(p);
    }
}

// ---------------------------------------------------------------------------
// 5. Multiple allocations of the same size return distinct, non-overlapping
//    pointers (basic uniqueness / no alias)
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, MultipleAllocsDistinct) {
    constexpr int N = 32;
    void *ptrs[N];
    for (int i = 0; i < N; ++i) {
        ptrs[i] = recl_malloc(64);
        ASSERT_NE(ptrs[i], nullptr);
        std::cout << "  [" << i << "] recl_malloc(64) -> " << ptrs[i] << "\n";
    }
    // Check all pointers are unique
    int duplicates = 0;
    for (int i = 0; i < N; ++i)
        for (int j = i + 1; j < N; ++j) {
            if (ptrs[i] == ptrs[j]) {
                ++duplicates;
                std::cout << "  DUPLICATE: ptrs[" << i << "] == ptrs[" << j << "] = " << ptrs[i] << "\n";
            }
            EXPECT_NE(ptrs[i], ptrs[j]) << "duplicate at i=" << i << " j=" << j;
        }
    if (duplicates == 0)
        std::cout << "  all " << N << " pointers are distinct (no aliases found)\n";

    for (int i = 0; i < N; ++i)
        recl_free(ptrs[i]);
}

// ---------------------------------------------------------------------------
// 6. Data written to a small allocation is read back correctly (no corruption)
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, WriteReadSmall) {
    constexpr size_t SZ = 256;
    auto *p = static_cast<unsigned char *>(recl_malloc(SZ));
    ASSERT_NE(p, nullptr);
    std::cout << "  recl_malloc(" << SZ << ") -> " << static_cast<void *>(p) << "\n";

    for (size_t i = 0; i < SZ; ++i)
        p[i] = static_cast<unsigned char>(i & 0xFF);
    std::cout << "  wrote " << SZ << " bytes (pattern 0x00..0xFF)\n";

    for (size_t i = 0; i < SZ; ++i)
        EXPECT_EQ(p[i], static_cast<unsigned char>(i & 0xFF)) << "at i=" << i;
    std::cout << "  all " << SZ << " bytes read back correctly\n";

    recl_free(p);
}

// ---------------------------------------------------------------------------
// 7. Data written to a large allocation (> LARGE_THRESHOLD) is read back
//    correctly - exercises the separate large_alloc / large_free path
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, WriteReadLarge) {
    constexpr size_t SZ = LARGE_THRESHOLD + 4096; // clearly in large path
    auto *p = static_cast<unsigned char *>(recl_malloc(SZ));
    ASSERT_NE(p, nullptr);
    std::cout << "  recl_malloc(" << SZ << " = LARGE_THRESHOLD+4096) -> "
              << static_cast<void *>(p) << " (large path)\n";

    std::memset(p, 0xAB, SZ);
    std::cout << "  wrote " << SZ << " bytes (pattern 0xAB)\n";
    for (size_t i = 0; i < SZ; ++i)
        EXPECT_EQ(p[i], 0xAB) << "corruption at byte " << i;
    std::cout << "  all " << SZ << " bytes verified\n";

    recl_free(p);
}

// ---------------------------------------------------------------------------
// 8. Allocations at every exact size-class boundary succeed and free cleanly
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, SizeClassBoundaries) {
    int idx = 0;
    for (size_t sc_size : kSizeClasses) {
        void *p = recl_malloc(sc_size);
        ASSERT_NE(p, nullptr) << "failed at size=" << sc_size;
        std::cout << "  class[" << idx++ << "] size=" << sc_size
                  << " -> " << p << "\n";
        recl_free(p);
    }
}

// ---------------------------------------------------------------------------
// 9. The small/large boundary: 256 KiB takes the small path;
//    256 KiB + 1 takes the large path.  Both must succeed and free cleanly.
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, LargeThresholdBoundary) {
    // Exactly at the threshold -> small (size class 14)
    void *small = recl_malloc(LARGE_THRESHOLD);
    ASSERT_NE(small, nullptr);
    std::cout << "  recl_malloc(" << LARGE_THRESHOLD << " = LARGE_THRESHOLD) -> "
              << small << " (small path, class 14)\n";
    recl_free(small);

    // One byte over -> large path
    void *large = recl_malloc(LARGE_THRESHOLD + 1);
    ASSERT_NE(large, nullptr);
    std::cout << "  recl_malloc(" << LARGE_THRESHOLD + 1 << " = LARGE_THRESHOLD+1) -> "
              << large << " (large path)\n";
    recl_free(large);
}

// ---------------------------------------------------------------------------
// 10. Free-then-reallocate the same size returns the same pointer via the
//     tcache hot path (the freed object sits at the tcache bin head)
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, FreeAndReallocSamePointer) {
    void *p = recl_malloc(64);
    ASSERT_NE(p, nullptr);
    std::cout << "  first  alloc -> " << p << "\n";
    recl_free(p);
    std::cout << "  freed  " << p << ", tcache bin head should now hold it\n";

    // Tcache head for this size class should still hold p
    void *q = recl_malloc(64);
    ASSERT_NE(q, nullptr);
    std::cout << "  second alloc -> " << q
              << (p == q ? " (same - tcache hit)" : " (different - unexpected)") << "\n";
    EXPECT_EQ(p, q) << "expected tcache to return the just-freed pointer";
    recl_free(q);
}

// ---------------------------------------------------------------------------
// 11. Allocating MAX_CACHED + 2 objects then freeing them all exercises the
//     tcache overflow -> flush-to-ccache path (count > MAX_CACHED triggers a
//     half-flush)
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, TcacheFillAndFlush) {
    constexpr int N = MAX_CACHED + 2; // 66
    std::vector<void *> ptrs(N);
    std::cout << "  allocating " << N << " objects of 128 B "
              << "(MAX_CACHED=" << MAX_CACHED << ", flush triggers at count > MAX_CACHED)\n";

    for (int i = 0; i < N; ++i) {
        ptrs[i] = recl_malloc(128);
        ASSERT_NE(ptrs[i], nullptr) << "alloc failed at i=" << i;
    }
    std::cout << "  freeing all " << N << " objects; flush to ccache expected during free\n";
    // Free in allocation order - when count crosses MAX_CACHED the flush fires
    for (void *p : ptrs)
        recl_free(p);

    // Verify the allocator is still healthy afterwards
    void *sanity = recl_malloc(128);
    ASSERT_NE(sanity, nullptr);
    std::cout << "  post-flush sanity alloc -> " << sanity << " (allocator still healthy)\n";
    recl_free(sanity);
}

// ---------------------------------------------------------------------------
// 12. One allocation at each of the 15 size classes; all freed without crash
// ---------------------------------------------------------------------------
TEST(ReclaimAllocator, MixedSizeClassesAllFree) {
    void *ptrs[NUM_SIZE_CLASSES];
    for (int i = 0; i < NUM_SIZE_CLASSES; ++i) {
        ptrs[i] = recl_malloc(kSizeClasses[i]);
        ASSERT_NE(ptrs[i], nullptr) << "size class " << i;
        std::cout << "  class[" << i << "] size=" << kSizeClasses[i]
                  << " -> " << ptrs[i] << "\n";
    }
    for (int i = 0; i < NUM_SIZE_CLASSES; ++i)
        recl_free(ptrs[i]);
    std::cout << "  all " << NUM_SIZE_CLASSES << " allocations freed without crash\n";
}
