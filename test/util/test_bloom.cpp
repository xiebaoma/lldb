#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "include/lldb/filter_policy.h"
#include "lldb/slice.h"
#include "util/bloom.h"

namespace lldb {
namespace {

// ============================================================
// Construction & Name
// ============================================================

TEST(BloomTest, Name) {
  BloomFilterPolicy policy(10);
  EXPECT_STREQ(policy.Name(), "lldb.BuiltinBloomFilter2");
}

TEST(BloomTest, NewBloomFilterPolicy) {
  const FilterPolicy* policy = NewBloomFilterPolicy(10);
  ASSERT_NE(policy, nullptr);
  EXPECT_STREQ(policy->Name(), "lldb.BuiltinBloomFilter2");
  delete policy;
}

// ============================================================
// Basic CreateFilter / KeyMayMatch
// ============================================================

TEST(BloomTest, EmptyKeysProducesEmptyFilter) {
  BloomFilterPolicy policy(10);
  std::string filter;
  policy.CreateFilter(nullptr, 0, &filter);
  // Should append k_ as the last byte (k_ > 0 so non-empty).
  EXPECT_FALSE(filter.empty());
}

TEST(BloomTest, SingleKeyMatches) {
  BloomFilterPolicy policy(10);
  std::string filter;
  Slice keys[] = {Slice("hello")};
  policy.CreateFilter(keys, 1, &filter);
  EXPECT_TRUE(policy.KeyMayMatch(Slice("hello"), Slice(filter)));
}

TEST(BloomTest, SingleKeyDoesNotMatchOther) {
  BloomFilterPolicy policy(10);
  std::string filter;
  Slice keys[] = {Slice("hello")};
  policy.CreateFilter(keys, 1, &filter);
  // "world" was not inserted — should (most likely) return false.
  EXPECT_FALSE(policy.KeyMayMatch(Slice("world"), Slice(filter)));
}

TEST(BloomTest, MultipleKeysAllMatch) {
  BloomFilterPolicy policy(10);
  Slice keys[] = {Slice("alpha"), Slice("beta"), Slice("gamma"),
                  Slice("delta"), Slice("epsilon")};
  std::string filter;
  policy.CreateFilter(keys, 5, &filter);

  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(policy.KeyMayMatch(keys[i], Slice(filter)))
        << "key \"" << keys[i].data() << "\" should match";
  }
}

TEST(BloomTest, MultipleKeysNoFalseNegative) {
  // Every inserted key must be found — bloom filters have no false negatives.
  constexpr int kN = 200;
  BloomFilterPolicy policy(10);
  std::string filter;
  std::vector<std::string> key_strings(kN);
  std::vector<Slice> keys(kN);
  for (int i = 0; i < kN; ++i) {
    key_strings[i] = "key_" + std::to_string(i);
    keys[i] = Slice(key_strings[i]);
  }
  policy.CreateFilter(keys.data(), kN, &filter);

  for (int i = 0; i < kN; ++i) {
    EXPECT_TRUE(policy.KeyMayMatch(keys[i], Slice(filter)))
        << "false negative for key " << i;
  }
}

// ============================================================
// False positive rate sanity check
// ============================================================

TEST(BloomTest, FalsePositiveRateRoughlyBound) {
  // With bits_per_key=10, the false positive rate should be ~1%.
  // 1000 inserted keys, 1000 non-inserted keys → expect < ~10% FP.
  constexpr int kN = 1000;
  BloomFilterPolicy policy(10);
  std::string filter;
  std::vector<Slice> keys(kN);
  std::vector<std::string> key_data(kN);
  for (int i = 0; i < kN; ++i) {
    key_data[i] = "inserted_" + std::to_string(i);
    keys[i] = Slice(key_data[i]);
  }
  policy.CreateFilter(keys.data(), kN, &filter);

  int fp = 0;
  for (int i = 0; i < kN; ++i) {
    std::string nk = "not_inserted_" + std::to_string(i);
    if (policy.KeyMayMatch(Slice(nk), Slice(filter))) {
      ++fp;
    }
  }
  // With bits_per_key=10 and 1000 keys, FP rate should be well under 10%.
  // (True rate ~1%, but be generous to avoid flaky tests.)
  double fp_rate = static_cast<double>(fp) / kN;
  EXPECT_LT(fp_rate, 0.10) << "false positive rate too high: " << fp_rate;
}

// ============================================================
// bits_per_key / k clamping
// ============================================================

TEST(BloomTest, MinBitsPerKeyClampsToOne) {
  // bits_per_key <= 1 → k_ = max(1, 0) = 1
  BloomFilterPolicy policy(1);
  std::string filter;
  Slice keys[] = {Slice("a")};
  policy.CreateFilter(keys, 1, &filter);
  // k_ is stored as the last byte of the filter.
  uint8_t k = static_cast<uint8_t>(filter.back());
  EXPECT_EQ(k, 1u);
  EXPECT_TRUE(policy.KeyMayMatch(Slice("a"), Slice(filter)));
}

TEST(BloomTest, MaxKClampsToThirty) {
  // bits_per_key large → k_ = 30
  BloomFilterPolicy policy(100);
  std::string filter;
  Slice keys[] = {Slice("a")};
  policy.CreateFilter(keys, 1, &filter);
  uint8_t k = static_cast<uint8_t>(filter.back());
  EXPECT_LE(k, 30u);
}

TEST(BloomTest, MinimumFilterSize) {
  // With n=0, bits = max(0, 64) = 64 bits = 8 bytes, plus 1 byte for k_ = 9 bytes.
  BloomFilterPolicy policy(10);
  std::string filter;
  policy.CreateFilter(nullptr, 0, &filter);
  // 8 bytes (64 bits) + 1 byte (k) = 9 bytes.
  EXPECT_EQ(filter.size(), 9u);
}

// ============================================================
// KeyMayMatch edge cases
// ============================================================

TEST(BloomTest, KeyMayMatchEmptyFilter) {
  BloomFilterPolicy policy(10);
  EXPECT_FALSE(policy.KeyMayMatch(Slice("x"), Slice()));
}

TEST(BloomTest, KeyMayMatchTooShortFilter) {
  BloomFilterPolicy policy(10);
  // len < 2 should return false.
  const char single_byte[] = {10};
  EXPECT_FALSE(policy.KeyMayMatch(Slice("x"), Slice(single_byte, 1)));
}

TEST(BloomTest, KeyMayMatchLargeEncodedK) {
  // If the k byte (last byte) is > 30, should return true
  // (reserved for future encodings).
  BloomFilterPolicy policy(10);
  char filter_data[] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
                        static_cast<char>(31)};  // k=31 > 30
  EXPECT_TRUE(policy.KeyMayMatch(Slice("x"), Slice(filter_data, 9)));
}

// ============================================================
// Append behavior
// ============================================================

TEST(BloomTest, AppendToExistingString) {
  BloomFilterPolicy policy(10);
  std::string dst = "prefix_";
  size_t original_size = dst.size();
  Slice keys[] = {Slice("hello"), Slice("world")};
  policy.CreateFilter(keys, 2, &dst);
  // dst should contain original prefix + the filter bits + k byte.
  EXPECT_GT(dst.size(), original_size);
  EXPECT_EQ(dst.substr(0, original_size), "prefix_");
}

// ============================================================
// Determinism
// ============================================================

TEST(BloomTest, Deterministic) {
  BloomFilterPolicy policy(10);
  Slice keys[] = {Slice("a"), Slice("b"), Slice("c")};

  std::string f1, f2;
  policy.CreateFilter(keys, 3, &f1);
  policy.CreateFilter(keys, 3, &f2);

  EXPECT_EQ(f1, f2);
  // KeyMayMatch results should also be identical.
  EXPECT_EQ(policy.KeyMayMatch(Slice("a"), Slice(f1)),
            policy.KeyMayMatch(Slice("a"), Slice(f2)));
}

// ============================================================
// Duplicate keys
// ============================================================

TEST(BloomTest, DuplicateKeysDontBreak) {
  BloomFilterPolicy policy(10);
  Slice keys[] = {Slice("dup"), Slice("dup"), Slice("dup")};
  std::string filter;
  policy.CreateFilter(keys, 3, &filter);
  EXPECT_TRUE(policy.KeyMayMatch(Slice("dup"), Slice(filter)));
}

}  // namespace
}  // namespace lldb
