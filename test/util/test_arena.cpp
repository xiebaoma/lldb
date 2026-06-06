#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "util/arena.h"

namespace lldb {
namespace {

// ============================================================
// Basic Allocation
// ============================================================

TEST(ArenaTest, AllocateZeroBytesAsserts) {
  Arena arena;
  ASSERT_DEATH(arena.Allocate(0), "bytes > 0");
}

TEST(ArenaTest, AllocateSingleSmallBlock) {
  Arena arena;
  char* p = arena.Allocate(128);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0xAB, 128);
}

TEST(ArenaTest, AllocateMultipleBlocks) {
  Arena arena;
  std::vector<char*> ptrs;
  for (int i = 0; i < 100; ++i) {
    ptrs.push_back(arena.Allocate(256));
  }
  for (size_t i = 0; i < ptrs.size(); ++i) {
    std::memset(ptrs[i], static_cast<char>(i), 256);
  }
  for (size_t i = 0; i < ptrs.size(); ++i) {
    EXPECT_EQ(ptrs[i][0], static_cast<char>(i));
    EXPECT_EQ(ptrs[i][255], static_cast<char>(i));
  }
}

TEST(ArenaTest, AllocateMoreThanBlockSize) {
  Arena arena;
  char* p = arena.Allocate(8192);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0xCD, 8192);
  EXPECT_EQ(p[0], static_cast<char>(0xCD));
  EXPECT_EQ(p[8191], static_cast<char>(0xCD));
}

// ============================================================
// Aligned Allocation
// ============================================================

TEST(ArenaTest, AllocateAlignedReturnsAlignedPointer) {
  Arena arena;
  constexpr size_t kAlign = 8;
  for (int i = 0; i < 200; ++i) {
    char* p = arena.AllocateAligned((i % 37) + 1);
    ASSERT_NE(p, nullptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_EQ(addr & (kAlign - 1), 0u) << "unaligned at iteration " << i;
  }
}

TEST(ArenaTest, AllocateAlignedLargeObjectStillAligned) {
  Arena arena;
  char* p = arena.AllocateAligned(2048);
  ASSERT_NE(p, nullptr);
  uintptr_t addr = reinterpret_cast<uintptr_t>(p);
  EXPECT_EQ(addr & 7u, 0u);
}

// ============================================================
// Mixed Allocate / AllocateAligned
// ============================================================

TEST(ArenaTest, MixedAllocateAndAllocateAligned) {
  Arena arena;
  // After odd-sized unaligned allocations, alloc_ptr_ may be misaligned,
  // so the next AllocateAligned must insert padding.
  arena.Allocate(3);
  arena.Allocate(5);
  char* p = arena.AllocateAligned(16);
  ASSERT_NE(p, nullptr);
  uintptr_t addr = reinterpret_cast<uintptr_t>(p);
  EXPECT_EQ(addr & 7u, 0u);

  arena.Allocate(7);
  char* q = arena.AllocateAligned(32);
  ASSERT_NE(q, nullptr);
  uintptr_t qaddr = reinterpret_cast<uintptr_t>(q);
  EXPECT_EQ(qaddr & 7u, 0u);
}

// ============================================================
// Memory Usage
// ============================================================

TEST(ArenaTest, MemoryUsageGrowsWithAllocations) {
  Arena arena;
  size_t before = arena.MemoryUsage();
  arena.Allocate(100);
  arena.Allocate(200);
  size_t after = arena.MemoryUsage();
  EXPECT_GT(after, before);
}

TEST(ArenaTest, MemoryUsageMultipleBlocks) {
  Arena arena;
  constexpr int kN = 500;
  for (int i = 0; i < kN; ++i) {
    arena.Allocate(10);
  }
  EXPECT_GE(arena.MemoryUsage(), 2u * 4096);
}

// ============================================================
// Thread Safety
// ============================================================

TEST(ArenaTest, ConcurrentAllocateNoDataRace) {
  Arena arena;
  constexpr int kThreads = 8;
  constexpr int kAllocsPerThread = 1000;

  std::vector<std::thread> threads;
  std::atomic<bool> ok{true};

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&arena, &ok]() {
      for (int i = 0; i < kAllocsPerThread; ++i) {
        char* p = arena.Allocate(16);
        if (!p) {
          ok.store(false, std::memory_order_relaxed);
          return;
        }
        std::memset(p, 0x42, 16);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(ok.load());
}

TEST(ArenaTest, ConcurrentAllocateAlignedNoDataRace) {
  Arena arena;
  constexpr int kThreads = 8;
  constexpr int kAllocsPerThread = 1000;

  std::vector<std::thread> threads;
  std::atomic<bool> ok{true};

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&arena, &ok]() {
      for (int i = 0; i < kAllocsPerThread; ++i) {
        char* p = arena.AllocateAligned(24);
        if (!p) {
          ok.store(false, std::memory_order_relaxed);
          return;
        }
        std::memset(p, 0x7F, 24);
        if ((reinterpret_cast<uintptr_t>(p) & 7u) != 0) {
          ok.store(false, std::memory_order_relaxed);
          return;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(ok.load());
}

TEST(ArenaTest, ConcurrentMixedAllocate) {
  Arena arena;
  constexpr int kThreads = 6;
  constexpr int kIters = 500;
  std::atomic<int> counter{0};

  auto worker = [&arena, &counter](bool use_aligned) {
    for (int i = 0; i < kIters; ++i) {
      char* p;
      if (use_aligned) {
        p = arena.AllocateAligned((i % 64) + 1);
      } else {
        p = arena.Allocate((i % 64) + 1);
      }
      std::memset(p, 0x55, (i % 64) + 1);
      counter.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(worker, t % 2 == 0);
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter.load(), kThreads * kIters);
}

// ============================================================
// Edge Cases
// ============================================================

TEST(ArenaTest, FreshArenaMemoryUsageIsZero) {
  Arena arena;
  EXPECT_EQ(arena.MemoryUsage(), 0u);
}

TEST(ArenaTest, AllocateExactlyBlockSize) {
  Arena arena;
  char* p = arena.Allocate(4096);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0xEF, 4096);
}

TEST(ArenaTest, AllocateExactlyQuarterBlock) {
  Arena arena;
  // kBlockSize/4 = 1024 — boundary that determines fallback path.
  char* p1 = arena.Allocate(1024);  // == kBlockSize/4
  char* p2 = arena.Allocate(1025);  // >  kBlockSize/4
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  std::memset(p1, 0x11, 1024);
  std::memset(p2, 0x22, 1025);
}

}  // namespace
}  // namespace lldb
