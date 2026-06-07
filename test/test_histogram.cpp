#include <gtest/gtest.h>

#include <string>

#include "util/histogram.h"

namespace lldb {
namespace {

// Histogram's default constructor does not initialize members.  Each test
// must call Clear() before using the histogram.
class HistogramTest : public ::testing::Test {
 protected:
  void SetUp() override { h_.Clear(); }
  Histogram h_;
};

// ============================================================
// Empty histogram
// ============================================================

TEST_F(HistogramTest, EmptyHistogram) {
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 0"), std::string::npos);
  EXPECT_NE(s.find("Average: 0.0000"), std::string::npos);
}

// ============================================================
// Add
// ============================================================

TEST_F(HistogramTest, AddSingleValue) {
  h_.Add(5.0);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 1"), std::string::npos);
}

TEST_F(HistogramTest, AddMultipleValues) {
  for (int i = 0; i < 100; i++) {
    h_.Add(10.0);
  }
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 100"), std::string::npos);
}

TEST_F(HistogramTest, ZeroValue) {
  h_.Add(0);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 1"), std::string::npos);
  EXPECT_NE(s.find("Min: 0.0000"), std::string::npos);
}

TEST_F(HistogramTest, NegativeValue) {
  h_.Add(-5.0);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Min: -5.0000"), std::string::npos);
}

TEST_F(HistogramTest, LargeValue) {
  h_.Add(1e100);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 1"), std::string::npos);
}

// ============================================================
// Average
// ============================================================

TEST_F(HistogramTest, Average) {
  h_.Add(10);
  h_.Add(20);
  h_.Add(30);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Average: 20.0000"), std::string::npos);
}

// ============================================================
// Min / Max
// ============================================================

TEST_F(HistogramTest, MinAndMax) {
  h_.Add(1);
  h_.Add(100);
  h_.Add(50);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Min: 1.0000"), std::string::npos);
  EXPECT_NE(s.find("Max: 100.0000"), std::string::npos);
}

// ============================================================
// Median / Percentile
// ============================================================

TEST_F(HistogramTest, MedianOdd) {
  h_.Add(1);
  h_.Add(5);
  h_.Add(10);
  // Bucket for value 5 is [5,6). With 1 value below and 1 in this bucket,
  // linear interpolation yields median = 5.5.
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Median: 5.5000"), std::string::npos);
}

TEST_F(HistogramTest, MedianEven) {
  h_.Add(1);
  h_.Add(2);
  h_.Add(3);
  h_.Add(4);
  // Bucket for value 2 is [2,3). threshold=2.0, left_sum=1, right_sum=2.
  // pos=1.0, so median = 2 + 1*1.0 = 3.0.
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Median: 3.0000"), std::string::npos);
}

// ============================================================
// Standard deviation
// ============================================================

TEST_F(HistogramTest, StdDevZero) {
  h_.Add(42);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("StdDev: 0.00"), std::string::npos);
}

TEST_F(HistogramTest, StdDevNonZero) {
  h_.Add(0);
  h_.Add(10);
  // variance = (100*2 - 100)/4 = 25, stddev = 5.
  std::string s = h_.ToString();
  EXPECT_NE(s.find("StdDev: 5.00"), std::string::npos);
}

// ============================================================
// Clear
// ============================================================

TEST_F(HistogramTest, ClearResetsCount) {
  h_.Add(42);
  h_.Add(99);
  h_.Clear();
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 0"), std::string::npos);
}

// ============================================================
// Merge
// ============================================================

TEST_F(HistogramTest, MergeCombinesCounts) {
  h_.Add(1);
  h_.Add(2);

  Histogram other;
  other.Clear();
  other.Add(100);
  other.Add(200);

  h_.Merge(other);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 4"), std::string::npos);
  EXPECT_NE(s.find("Min: 1.0000"), std::string::npos);
  EXPECT_NE(s.find("Max: 200.0000"), std::string::npos);
}

TEST_F(HistogramTest, MergeEmptyIntoNonEmpty) {
  h_.Add(5);

  Histogram empty;
  empty.Clear();
  h_.Merge(empty);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 1"), std::string::npos);
}

TEST_F(HistogramTest, MergeNonEmptyIntoEmpty) {
  // h_ is empty (just cleared). Merge a non-empty one into it.
  Histogram other;
  other.Clear();
  other.Add(7);

  h_.Merge(other);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 1"), std::string::npos);
  EXPECT_NE(s.find("Min: 7.0000"), std::string::npos);
  EXPECT_NE(s.find("Max: 7.0000"), std::string::npos);
}

// ============================================================
// Bucket boundaries
// ============================================================

TEST_F(HistogramTest, BucketBoundaries) {
  h_.Add(0.5);   // < 1 → bucket 0
  h_.Add(1.0);   // <= 1 → bucket 1
  h_.Add(1.5);   // <= 2 → bucket 2
  h_.Add(9.0);   // <= 9  → bucket 9
  h_.Add(10.0);  // <= 10 → bucket 10
  h_.Add(11.0);  // <= 12 → bucket 11
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count: 6"), std::string::npos);
}

// ============================================================
// ToString format
// ============================================================

TEST_F(HistogramTest, ToStringHasHeader) {
  h_.Add(3);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("Count:"), std::string::npos);
  EXPECT_NE(s.find("Average:"), std::string::npos);
  EXPECT_NE(s.find("StdDev:"), std::string::npos);
  EXPECT_NE(s.find("Min:"), std::string::npos);
  EXPECT_NE(s.find("Median:"), std::string::npos);
  EXPECT_NE(s.find("Max:"), std::string::npos);
}

TEST_F(HistogramTest, ToStringHasHashMarks) {
  h_.Add(42);
  std::string s = h_.ToString();
  EXPECT_NE(s.find("###"), std::string::npos);
}

}  // namespace
}  // namespace lldb
