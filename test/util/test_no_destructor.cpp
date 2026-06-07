#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "util/no_destructor.h"

namespace lldb {
namespace {

// Helper: tracks whether its destructor was called.
struct DestructionTracker {
  inline static bool destructor_called = false;

  DestructionTracker() = default;
  ~DestructionTracker() { destructor_called = true; }

  DestructionTracker(const DestructionTracker&) = delete;
  DestructionTracker& operator=(const DestructionTracker&) = delete;
};

// Helper: over-aligned struct for alignment testing.
struct alignas(64) OverAligned {
  char data[128] = {};
};

// Helper: type with multi-arg constructor.
struct MultiArg {
  int a;
  std::string b;
  double c;

  MultiArg(int a_, std::string b_, double c_) : a(a_), b(std::move(b_)), c(c_) {}
};

// ============================================================
// NoDestructor
// ============================================================

TEST(NoDestructorTest, BasicInt) {
  NoDestructor<int> nd(42);
  EXPECT_EQ(*nd.get(), 42);
}

TEST(NoDestructorTest, StringType) {
  NoDestructor<std::string> nd("hello");
  EXPECT_EQ(*nd.get(), "hello");
}

TEST(NoDestructorTest, DefaultConstructor) {
  NoDestructor<int> nd(0);
  EXPECT_EQ(*nd.get(), 0);
}

TEST(NoDestructorTest, MultiArgConstructor) {
  NoDestructor<MultiArg> nd(10, "world", 3.14);
  MultiArg* p = nd.get();
  EXPECT_EQ(p->a, 10);
  EXPECT_EQ(p->b, "world");
  EXPECT_DOUBLE_EQ(p->c, 3.14);
}

TEST(NoDestructorTest, MutableAccess) {
  NoDestructor<int> nd(7);
  *nd.get() = 99;
  EXPECT_EQ(*nd.get(), 99);
}

TEST(NoDestructorTest, DestructorNotCalled) {
  DestructionTracker::destructor_called = false;
  {
    NoDestructor<DestructionTracker> nd;
    EXPECT_FALSE(DestructionTracker::destructor_called);
  }
  // After NoDestructor goes out of scope, the managed object's destructor
  // should NOT have been called.
  EXPECT_FALSE(DestructionTracker::destructor_called);
}

TEST(NoDestructorTest, OverAlignedType) {
  NoDestructor<OverAligned> nd;
  OverAligned* p = nd.get();
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(OverAligned), 0u);
  EXPECT_NE(p, nullptr);
}

}  // namespace
}  // namespace lldb
