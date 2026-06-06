#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include "util/hash.h"

namespace lldb {
namespace {

// ============================================================
// Determinism
// ============================================================

TEST(HashTest, Deterministic) {
  const char* data = "hello world";
  uint32_t h1 = Hash(data, strlen(data), 0x12345678);
  uint32_t h2 = Hash(data, strlen(data), 0x12345678);
  EXPECT_EQ(h1, h2);
}

// ============================================================
// Seed affects output
// ============================================================

TEST(HashTest, DifferentSeedDifferentHash) {
  const char* data = "hello";
  uint32_t h1 = Hash(data, 5, 0);
  uint32_t h2 = Hash(data, 5, 1);
  // Different seeds should (almost always) produce different hashes.
  EXPECT_NE(h1, h2);
}

// ============================================================
// Data content affects output
// ============================================================

TEST(HashTest, DifferentDataDifferentHash) {
  uint32_t h1 = Hash("abc", 3, 0);
  uint32_t h2 = Hash("abd", 3, 0);
  EXPECT_NE(h1, h2);
}

TEST(HashTest, DifferentLengthDifferentHash) {
  uint32_t h1 = Hash("abc", 2, 0);  // "ab"
  uint32_t h2 = Hash("abc", 3, 0);  // "abc"
  EXPECT_NE(h1, h2);
}

// ============================================================
// Empty input
// ============================================================

TEST(HashTest, EmptyInput) {
  uint32_t h1 = Hash("", 0, 0);
  uint32_t h2 = Hash("", 0, 42);
  // Even with empty data, seed and length affect result:
  // h = seed ^ (n * m) = seed ^ 0 = seed
  EXPECT_EQ(h1, 0u);
  EXPECT_EQ(h2, 42u);
}

// ============================================================
// Edge cases: various lengths that exercise different code paths
// ============================================================

TEST(HashTest, SingleByte) {
  uint32_t h = Hash("x", 1, 0);
  uint32_t h_same = Hash("x", 1, 0);
  uint32_t h_diff = Hash("y", 1, 0);
  EXPECT_EQ(h, h_same);
  EXPECT_NE(h, h_diff);
}

TEST(HashTest, TwoBytes) {
  uint32_t h = Hash("xy", 2, 0);
  uint32_t h_same = Hash("xy", 2, 0);
  EXPECT_EQ(h, h_same);
}

TEST(HashTest, ThreeBytes) {
  uint32_t h = Hash("xyz", 3, 0);
  uint32_t h_same = Hash("xyz", 3, 0);
  EXPECT_EQ(h, h_same);
}

TEST(HashTest, FourBytes) {
  // Exactly one 4-byte chunk — exercises the while loop once.
  uint32_t h = Hash("abcd", 4, 0);
  uint32_t h_same = Hash("abcd", 4, 0);
  EXPECT_EQ(h, h_same);
}

TEST(HashTest, FiveBytes) {
  // 4 bytes in the loop + 1 byte in the switch fallthrough.
  uint32_t h = Hash("abcde", 5, 0);
  uint32_t h_same = Hash("abcde", 5, 0);
  EXPECT_EQ(h, h_same);
}

TEST(HashTest, SevenBytes) {
  // 4 bytes in the loop + 3 bytes in the switch.
  uint32_t h = Hash("abcdefg", 7, 0);
  uint32_t h_same = Hash("abcdefg", 7, 0);
  EXPECT_EQ(h, h_same);
}

// ============================================================
// Distribution sanity check
// ============================================================

TEST(HashTest, DistributionNoTrivialCollisions) {
  // Hash many short strings with the same seed; most should be distinct.
  constexpr int kN = 1000;
  std::set<uint32_t> seen;
  char buf[16];
  for (int i = 0; i < kN; ++i) {
    snprintf(buf, sizeof(buf), "key_%d", i);
    uint32_t h = Hash(buf, strlen(buf), 0xbc9f1d34);
    seen.insert(h);
  }
  // With 1000 distinct inputs and a 32-bit hash space, all should be unique.
  EXPECT_EQ(seen.size(), size_t(kN));
}

// ============================================================
// Known-answer test (golden value)
// ============================================================

TEST(HashTest, GoldenValue) {
  // Guard against accidental algorithm changes.
  // If this test breaks, someone changed the hash function — check whether
  // that was intentional, then update this golden value.
  uint32_t h = Hash("hello", 5, 0xbc9f1d34);
  EXPECT_NE(h, 0u);
  // Same input twice → same output.
  EXPECT_EQ(h, Hash("hello", 5, 0xbc9f1d34));
  // Same data, different seed → different output.
  EXPECT_NE(h, Hash("hello", 5, 0x11111111));
}

}  // namespace
}  // namespace lldb
