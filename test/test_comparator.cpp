#include <gtest/gtest.h>

#include <string>

#include "lldb/comparator.h"
#include "lldb/slice.h"

namespace lldb {
namespace {

const Comparator* cmp() {
  static const Comparator* c = BytewiseComparator();
  return c;
}

// ============================================================
// Name
// ============================================================

TEST(ComparatorTest, Name) {
  EXPECT_STREQ(cmp()->Name(), "lldb.BytewiseComparator");
}

// ============================================================
// Compare
// ============================================================

TEST(ComparatorTest, CompareEqual) {
  EXPECT_EQ(cmp()->Compare(Slice("abc"), Slice("abc")), 0);
}

TEST(ComparatorTest, CompareLess) {
  EXPECT_LT(cmp()->Compare(Slice("abc"), Slice("abd")), 0);
  EXPECT_LT(cmp()->Compare(Slice("a"), Slice("aa")), 0);
  EXPECT_LT(cmp()->Compare(Slice(""), Slice("a")), 0);
}

TEST(ComparatorTest, CompareGreater) {
  EXPECT_GT(cmp()->Compare(Slice("abd"), Slice("abc")), 0);
  EXPECT_GT(cmp()->Compare(Slice("aa"), Slice("a")), 0);
  EXPECT_GT(cmp()->Compare(Slice("a"), Slice("")), 0);
}

TEST(ComparatorTest, CompareEmpty) {
  EXPECT_EQ(cmp()->Compare(Slice(), Slice()), 0);
}

// ============================================================
// FindShortestSeparator
// ============================================================

TEST(ComparatorTest, FindShortestSeparatorDifferentFirstByte) {
  std::string start = "hello";
  Slice limit("world");
  cmp()->FindShortestSeparator(&start, limit);
  // After shortening, Compare(start, limit) should be < 0.
  EXPECT_LT(cmp()->Compare(start, limit), 0);
}

TEST(ComparatorTest, FindShortestSeparatorPrefixCase) {
  // When one string is a prefix of the other, start should not change.
  std::string start = "hello";
  std::string orig = start;
  Slice limit("hello world");
  cmp()->FindShortestSeparator(&start, limit);
  EXPECT_EQ(start, orig);
}

TEST(ComparatorTest, FindShortestSeparatorUnchangedIfCantShorten) {
  // "abc" and "abd" differ at index 2: 'c' < 'd' but 'c'+1 == 'd', so
  // the byte cannot be incremented (must stay < limit byte).
  std::string start = "abc";
  std::string orig = start;
  Slice limit("abd");
  cmp()->FindShortestSeparator(&start, limit);
  // Should remain unchanged because 'c' + 1 == 'd'.
  EXPECT_EQ(start, orig);
}

TEST(ComparatorTest, FindShortestSeparatorShortensWhenPossible) {
  // "abc" and "abx" differ at index 2: 'c' < 'x', and 'c'+1='d' < 'x'.
  // So start should become "abd".
  std::string start = "abc";
  Slice limit("abx");
  cmp()->FindShortestSeparator(&start, limit);
  EXPECT_EQ(start, "abd");
  EXPECT_LT(cmp()->Compare(start, limit), 0);
}

// ============================================================
// FindShortSuccessor
// ============================================================

TEST(ComparatorTest, FindShortSuccessorBasic) {
  std::string key = "hello";
  std::string orig = key;
  cmp()->FindShortSuccessor(&key);
  // The result must be > original.
  EXPECT_GT(cmp()->Compare(key, orig), 0);
}

TEST(ComparatorTest, FindShortSuccessorSingleByte) {
  std::string key = "a";
  cmp()->FindShortSuccessor(&key);
  EXPECT_GT(cmp()->Compare(key, Slice("a")), 0);
  EXPECT_EQ(key, "b");
}

TEST(ComparatorTest, FindShortSuccessorAllFF) {
  // A key of all 0xFF bytes cannot have any byte incremented.
  std::string key = "\xFF\xFF\xFF";
  std::string orig = key;
  cmp()->FindShortSuccessor(&key);
  // Should remain unchanged since all bytes are 0xFF.
  EXPECT_EQ(key, orig);
}

TEST(ComparatorTest, FindShortSuccessorSingleFF) {
  // First byte is 0xFF, second byte < 0xFF → skip first, increment second.
  std::string key("\xFF\x42", 2);
  cmp()->FindShortSuccessor(&key);
  EXPECT_EQ(key.size(), 2u);
  EXPECT_EQ(key[0], '\xFF');
  EXPECT_EQ(key[1], '\x43');
  EXPECT_GT(cmp()->Compare(key, Slice("\xFF\x42", 2)), 0);
}

TEST(ComparatorTest, FindShortSuccessorTruncates) {
  // After incrementing, the key is truncated to the incremented position + 1.
  std::string key = "abc";
  cmp()->FindShortSuccessor(&key);
  // First byte 'a' can be incremented to 'b', truncated to 1 byte.
  EXPECT_EQ(key, "b");
}

}  // namespace
}  // namespace lldb
