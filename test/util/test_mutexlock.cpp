#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "port/port.h"
#include "util/mutexlock.h"

namespace lldb {
namespace {

// ============================================================
// Basic Lock / Unlock
// ============================================================

TEST(MutexLockTest, LockAndUnlock) {
  port::Mutex mu;
  {
    MutexLock lock(&mu);
    // mutex is held; can't easily assert this directly since AssertHeld
    // is a no-op, but the destructor will unlock it.
  }
  // After MutexLock destruction, mutex should be released.
  // Verify by acquiring it again.
  {
    MutexLock lock(&mu);
  }
}

TEST(MutexLockTest, SequentialLocks) {
  port::Mutex mu;
  for (int i = 0; i < 100; ++i) {
    MutexLock lock(&mu);
  }
  // Verify the mutex is still usable and unlocked.
  MutexLock lock(&mu);
}

// ============================================================
// Scoped Behavior
// ============================================================

TEST(MutexLockTest, ScopedUnlockAtBlockEnd) {
  port::Mutex mu;
  // After the inner block, the mutex is released and can be re-locked.
  { MutexLock lock(&mu); }
  { MutexLock lock(&mu); }
}

TEST(MutexLockTest, NestedScopes) {
  port::Mutex mu1;
  port::Mutex mu2;
  {
    MutexLock lock1(&mu1);
    {
      MutexLock lock2(&mu2);
      // Both mutexes are held.
    }
    // Only mu1 is still held here (mu2 released).
  }
  // Both released.
  // Re-acquire both to confirm.
  { MutexLock lock1(&mu1); }
  { MutexLock lock2(&mu2); }
}

// ============================================================
// Thread Safety
// ============================================================

TEST(MutexLockTest, ConcurrentCounter) {
  constexpr int kThreads = 8;
  constexpr int kItersPerThread = 10000;

  port::Mutex mu;
  int counter = 0;
  std::atomic<bool> ok{true};

  auto worker = [&]() {
    for (int i = 0; i < kItersPerThread; ++i) {
      MutexLock lock(&mu);
      int v = counter;
      v = v + 1;
      counter = v;
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(worker);
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter, kThreads * kItersPerThread);
}

TEST(MutexLockTest, ConcurrentProtectedRegion) {
  constexpr int kThreads = 6;
  constexpr int kOps = 5000;

  port::Mutex mu;
  int value = 0;
  std::atomic<int> read_count{0};
  std::atomic<int> write_count{0};

  auto reader = [&]() {
    for (int i = 0; i < kOps; ++i) {
      MutexLock lock(&mu);
      int v = value;
      EXPECT_GE(v, 0);
      read_count.fetch_add(1, std::memory_order_relaxed);
    }
  };

  auto writer = [&]() {
    for (int i = 0; i < kOps; ++i) {
      MutexLock lock(&mu);
      value = i;
      write_count.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    if (t % 2 == 0) {
      threads.emplace_back(reader);
    } else {
      threads.emplace_back(writer);
    }
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(read_count.load(), (kThreads / 2) * kOps);
  EXPECT_EQ(write_count.load(), (kThreads / 2) * kOps);
}

// ============================================================
// Edge Cases
// ============================================================

TEST(MutexLockTest, MultipleMutexes) {
  port::Mutex mu1;
  port::Mutex mu2;
  port::Mutex mu3;

  {
    MutexLock l1(&mu1);
    MutexLock l2(&mu2);
    MutexLock l3(&mu3);
  }
  // All released.
  MutexLock l1(&mu1);
  MutexLock l2(&mu2);
  MutexLock l3(&mu3);
}

}  // namespace
}  // namespace lldb
