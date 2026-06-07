#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>

#include "util/random.cpp"

namespace lldb {
namespace {

static constexpr uint32_t kM = 2147483647L;  // 2^31-1

// ============================================================
// Constructor
// ============================================================

TEST(RandomTest, ConstructorMasksSeed) {
  // Seed is masked to 31 bits.
  Random r(0xFFFFFFFFu);
  // First call to Next() should produce a valid value.
  uint32_t v = r.Next();
  EXPECT_GE(v, 1u);
  EXPECT_LE(v, kM);
}

TEST(RandomTest, ConstructorAvoidsBadSeedZero) {
  // Seed 0 is bumped to 1.
  Random r(0);
  uint32_t v = r.Next();
  EXPECT_GE(v, 1u);
  EXPECT_LE(v, kM);
}

TEST(RandomTest, ConstructorAvoidsBadSeedM) {
  // Seed equal to M (2147483647) is bumped to 1.
  Random r(kM);
  uint32_t v = r.Next();
  EXPECT_GE(v, 1u);
  EXPECT_LE(v, kM);
}

// ============================================================
// Next()
// ============================================================

TEST(RandomTest, NextIsDeterministic) {
  Random a(42);
  Random b(42);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(a.Next(), b.Next());
  }
}

TEST(RandomTest, NextRange) {
  Random r(12345);
  for (int i = 0; i < 10000; ++i) {
    uint32_t v = r.Next();
    EXPECT_GE(v, 1u) << "at iteration " << i;
    EXPECT_LE(v, kM) << "at iteration " << i;
  }
}

TEST(RandomTest, NextNeverZeroOrM) {
  Random r(17);
  for (int i = 0; i < 10000; ++i) {
    uint32_t v = r.Next();
    EXPECT_NE(v, 0u);
    EXPECT_NE(v, kM);
  }
}

TEST(RandomTest, NextProducesVariousMagnitudes) {
  // The Lehmer RNG produces values uniformly in [1, M-1].  With enough
  // iterations we should see values across the full range.
  Random r(99);
  bool has_small = false;   // bottom third
  bool has_mid = false;     // middle third
  bool has_large = false;   // top third
  for (int i = 0; i < 10000 && !(has_small && has_mid && has_large); ++i) {
    uint32_t v = r.Next();
    if (v < kM / 3) has_small = true;
    else if (v > 2 * kM / 3) has_large = true;
    else has_mid = true;
  }
  EXPECT_TRUE(has_small);
  EXPECT_TRUE(has_mid);
  EXPECT_TRUE(has_large);
}

TEST(RandomTest, DifferentSeedsGiveDifferentSequences) {
  Random a(1);
  Random b(999999);
  bool differs = false;
  for (int i = 0; i < 100; ++i) {
    if (a.Next() != b.Next()) {
      differs = true;
      break;
    }
  }
  EXPECT_TRUE(differs);
}

// ============================================================
// Uniform
// ============================================================

TEST(RandomTest, UniformRange) {
  Random r(42);
  for (int n = 1; n <= 100; ++n) {
    for (int i = 0; i < 100; ++i) {
      uint32_t v = r.Uniform(n);
      EXPECT_LT(v, static_cast<uint32_t>(n));
    }
  }
}

TEST(RandomTest, UniformDistribution) {
  Random r(7);
  const int n = 10;
  const int samples = 100000;
  int counts[n] = {};
  for (int i = 0; i < samples; ++i) {
    ++counts[r.Uniform(n)];
  }
  // Each bucket should have roughly samples/n = 10000.
  // Allow a generous tolerance of +/- 15%.
  int expected = samples / n;
  for (int i = 0; i < n; ++i) {
    EXPECT_GT(counts[i], expected * 0.70);
    EXPECT_LT(counts[i], expected * 1.30);
  }
}

// ============================================================
// OneIn
// ============================================================

TEST(RandomTest, OneInReturnsBool) {
  Random r(42);
  bool saw_true = false;
  bool saw_false = false;
  for (int i = 0; i < 1000 && !(saw_true && saw_false); ++i) {
    if (r.OneIn(2)) {
      saw_true = true;
    } else {
      saw_false = true;
    }
  }
  EXPECT_TRUE(saw_true);
  EXPECT_TRUE(saw_false);
}

TEST(RandomTest, OneInFrequency) {
  Random r(13);
  const int n = 5;
  const int samples = 100000;
  int true_count = 0;
  for (int i = 0; i < samples; ++i) {
    if (r.OneIn(n)) ++true_count;
  }
  // Expected: ~samples/n = 20000. Allow +/- 15%.
  double expected = static_cast<double>(samples) / n;
  EXPECT_GT(true_count, expected * 0.70);
  EXPECT_LT(true_count, expected * 1.30);
}

// ============================================================
// Skewed
// ============================================================

TEST(RandomTest, SkewedRange) {
  Random r(42);
  for (int max_log = 0; max_log <= 10; ++max_log) {
    uint32_t limit = (1u << (max_log + 1)) - 1;  // upper bound via Uniform
    // Actually the range is [0, 2^max_log - 1]
    uint32_t max_val = (1u << max_log) - 1;
    for (int i = 0; i < 500; ++i) {
      uint32_t v = r.Skewed(max_log);
      EXPECT_LE(v, max_val) << "max_log=" << max_log;
    }
  }
}

TEST(RandomTest, SkewedBiasTowardsSmallerValues) {
  Random r(99);
  const int max_log = 4;  // range [0, 15]
  const int samples = 100000;
  int counts[16] = {};
  for (int i = 0; i < samples; ++i) {
    ++counts[r.Skewed(max_log)];
  }
  // Smaller values should appear more frequently.
  // Values 0-7 should have more hits than values 8-15 collectively.
  int small_sum = 0;
  int large_sum = 0;
  for (int i = 0; i < 8; ++i) small_sum += counts[i];
  for (int i = 8; i < 16; ++i) large_sum += counts[i];
  EXPECT_GT(small_sum, large_sum);
}

}  // namespace
}  // namespace lldb
