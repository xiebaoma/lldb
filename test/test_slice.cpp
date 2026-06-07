#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "include/lldb/slice.h"

namespace lldb {
namespace {

// ============================================================
// Construction
// ============================================================

TEST(SliceTest, DefaultConstructor) {
  Slice s;
  EXPECT_EQ(s.data(), "");  // points to static empty string
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.empty());
}

TEST(SliceTest, ConstructFromCString) {
  Slice s("hello");
  EXPECT_EQ(s.size(), 5u);
  EXPECT_EQ(std::memcmp(s.data(), "hello", 5), 0);
}

TEST(SliceTest, ConstructFromDataAndSize) {
  const char* data = "world";
  Slice s(data, 3);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::memcmp(s.data(), "wor", 3), 0);
}

TEST(SliceTest, ConstructFromStdString) {
  std::string str = "abc";
  Slice s(str);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::memcmp(s.data(), "abc", 3), 0);
}

TEST(SliceTest, ConstructFromEmptyStdString) {
  std::string str;
  Slice s(str);
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.empty());
}

// ============================================================
// Accessors
// ============================================================

TEST(SliceTest, OperatorBracket) {
  Slice s("abc");
  EXPECT_EQ(s[0], 'a');
  EXPECT_EQ(s[1], 'b');
  EXPECT_EQ(s[2], 'c');
}

TEST(SliceTest, BeginEnd) {
  Slice s("xy");
  EXPECT_EQ(*s.begin(), 'x');
  EXPECT_EQ(*(s.end() - 1), 'y');
  EXPECT_EQ(s.end() - s.begin(), 2);
}

TEST(SliceTest, Clear) {
  Slice s("hello");
  EXPECT_FALSE(s.empty());
  s.clear();
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

// ============================================================
// remove_prefix
// ============================================================

TEST(SliceTest, RemovePrefix) {
  Slice s("hello");
  s.remove_prefix(2);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::memcmp(s.data(), "llo", 3), 0);
}

TEST(SliceTest, RemovePrefixEntireSlice) {
  Slice s("hi");
  s.remove_prefix(2);
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.empty());
}

TEST(SliceTest, RemovePrefixDoesNotCopy) {
  const char* original = "hello";
  Slice s(original);
  s.remove_prefix(3);
  // data_ should point into the original buffer, offset by 3.
  EXPECT_EQ(s.data(), original + 3);
}

// ============================================================
// starts_with
// ============================================================

TEST(SliceTest, StartsWith) {
  Slice s("hello world");
  EXPECT_TRUE(s.starts_with(Slice("hello")));
  EXPECT_TRUE(s.starts_with(Slice("h")));
  EXPECT_TRUE(s.starts_with(Slice("")));
  EXPECT_TRUE(s.starts_with(Slice("hello world")));
}

TEST(SliceTest, StartsWithFalse) {
  Slice s("hello");
  EXPECT_FALSE(s.starts_with(Slice("world")));
  EXPECT_FALSE(s.starts_with(Slice("hellox")));   // longer
  EXPECT_FALSE(s.starts_with(Slice("help")));      // same length, diff char
}

// ============================================================
// compare — three-way comparison
// ============================================================

TEST(SliceTest, CompareEqual) {
  Slice a("abc");
  Slice b("abc");
  EXPECT_EQ(a.compare(b), 0);
}

TEST(SliceTest, CompareLessByContent) {
  Slice a("abc");
  Slice b("abd");
  EXPECT_LT(a.compare(b), 0);
  EXPECT_GT(b.compare(a), 0);
}

TEST(SliceTest, CompareLessByLength) {
  // Common prefix equal, but one is shorter → shorter < longer.
  Slice a("ab");
  Slice b("abc");
  EXPECT_LT(a.compare(b), 0);
  EXPECT_GT(b.compare(a), 0);
}

TEST(SliceTest, CompareEmpty) {
  Slice a("");
  Slice b("");
  EXPECT_EQ(a.compare(b), 0);

  Slice c("a");
  EXPECT_LT(Slice().compare(c), 0);
  EXPECT_GT(c.compare(Slice()), 0);
}

// ============================================================
// operator== / operator!=
// ============================================================

TEST(SliceTest, Equality) {
  EXPECT_TRUE(Slice("abc") == Slice("abc"));
  EXPECT_TRUE(Slice("abc") == Slice(std::string("abc")));
  EXPECT_FALSE(Slice("abc") == Slice("abd"));
  EXPECT_FALSE(Slice("ab") == Slice("abc"));
}

TEST(SliceTest, Inequality) {
  EXPECT_TRUE(Slice("abc") != Slice("abd"));
  EXPECT_TRUE(Slice("ab") != Slice("abc"));
  EXPECT_FALSE(Slice("abc") != Slice("abc"));
}

// ============================================================
// ToString
// ============================================================

TEST(SliceTest, ToString) {
  Slice s("hello");
  std::string str = s.ToString();
  EXPECT_EQ(str, "hello");
  // Must be a deep copy — modifying the string should not affect the slice.
  str[0] = 'H';
  EXPECT_EQ(s[0], 'h');
}

TEST(SliceTest, ToStringEmpty) {
  Slice s;
  EXPECT_EQ(s.ToString(), "");
}

// ============================================================
// Copy Semantics
// ============================================================

TEST(SliceTest, CopyConstructor) {
  Slice a("hello");
  Slice b(a);
  EXPECT_EQ(b.data(), a.data());
  EXPECT_EQ(b.size(), a.size());
}

TEST(SliceTest, CopyAssignment) {
  Slice a("hello");
  Slice b;
  b = a;
  EXPECT_EQ(b.data(), a.data());
  EXPECT_EQ(b.size(), a.size());
}

}  // namespace
}  // namespace lldb
