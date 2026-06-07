#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "include/lldb/slice.h"
#include "util/logging.h"

namespace lldb {
namespace {

// ============================================================
// NumberToString
// ============================================================

TEST(LoggingTest, NumberToStringZero) {
  EXPECT_EQ(NumberToString(0), "0");
}

TEST(LoggingTest, NumberToStringSmall) {
  EXPECT_EQ(NumberToString(1), "1");
  EXPECT_EQ(NumberToString(9), "9");
  EXPECT_EQ(NumberToString(10), "10");
  EXPECT_EQ(NumberToString(42), "42");
  EXPECT_EQ(NumberToString(99), "99");
  EXPECT_EQ(NumberToString(100), "100");
  EXPECT_EQ(NumberToString(999), "999");
}

TEST(LoggingTest, NumberToStringLarge) {
  EXPECT_EQ(NumberToString(1000000), "1000000");
  EXPECT_EQ(NumberToString(1000000000), "1000000000");
  EXPECT_EQ(NumberToString(1000000000000ULL), "1000000000000");
}

TEST(LoggingTest, NumberToStringMaxUint64) {
  EXPECT_EQ(NumberToString(std::numeric_limits<uint64_t>::max()),
            "18446744073709551615");
}

TEST(LoggingTest, NumberToStringRoundTrip) {
  uint64_t values[] = {0, 1, 9, 10, 42, 99, 100, 999, 1000, 1024,
                       1000000, 123456789, 1000000000000ULL,
                       0xFFFFFFFFFFFFFFFFULL};
  for (uint64_t v : values) {
    std::string s = NumberToString(v);
    uint64_t parsed = std::stoull(s);
    EXPECT_EQ(parsed, v) << "failed for " << v;
  }
}

// ============================================================
// AppendNumberTo
// ============================================================

TEST(LoggingTest, AppendNumberToBasic) {
  std::string s;
  AppendNumberTo(&s, 42);
  EXPECT_EQ(s, "42");
}

TEST(LoggingTest, AppendNumberToAppends) {
  std::string s = "prefix_";
  AppendNumberTo(&s, 12345);
  EXPECT_EQ(s, "prefix_12345");
}

TEST(LoggingTest, AppendNumberToMultiple) {
  std::string s;
  AppendNumberTo(&s, 1);
  AppendNumberTo(&s, 2);
  AppendNumberTo(&s, 3);
  EXPECT_EQ(s, "123");
}

// ============================================================
// EscapeString
// ============================================================

TEST(LoggingTest, EscapeStringEmpty) {
  EXPECT_EQ(EscapeString(Slice()), "");
}

TEST(LoggingTest, EscapeStringAllPrintable) {
  std::string s = EscapeString(Slice("hello world"));
  EXPECT_EQ(s, "hello world");
}

TEST(LoggingTest, EscapeStringPrintableRange) {
  // Space (0x20) through '~' (0x7E) are all printable.
  std::string input;
  for (char c = ' '; c <= '~'; ++c) {
    input.push_back(c);
  }
  std::string result = EscapeString(Slice(input));
  EXPECT_EQ(result, input);
}

TEST(LoggingTest, EscapeStringNullByte) {
  char data[] = {'a', '\x00', 'b'};
  std::string result = EscapeString(Slice(data, sizeof(data)));
  EXPECT_EQ(result, "a\\x00b");
}

TEST(LoggingTest, EscapeStringControlChars) {
  char data[] = {'\x01', '\x02', '\x1F'};
  std::string result = EscapeString(Slice(data, sizeof(data)));
  EXPECT_EQ(result, "\\x01\\x02\\x1f");
}

TEST(LoggingTest, EscapeStringHighBytes) {
  char data[] = {'\x80', '\xFF'};
  std::string result = EscapeString(Slice(data, sizeof(data)));
  EXPECT_EQ(result, "\\x80\\xff");
}

TEST(LoggingTest, EscapeStringMixedContent) {
  const char data[] = "AB\x00\x01" "CD\xFF\x7F" "EF";
  std::string result = EscapeString(Slice(data, 10));
  EXPECT_EQ(result, "AB\\x00\\x01" "CD\\xff\\x7f" "EF");
}

TEST(LoggingTest, EscapeStringDeleteChar) {
  // DEL (0x7F) is not in the printable range.
  std::string result = EscapeString(Slice("\x7F", 1));
  EXPECT_EQ(result, "\\x7f");
}

// ============================================================
// AppendEscapedStringTo
// ============================================================

TEST(LoggingTest, AppendEscapedStringToAppends) {
  std::string s = "prefix_";
  char null_byte = '\x00';
  AppendEscapedStringTo(&s, Slice(&null_byte, 1));
  EXPECT_EQ(s, "prefix_\\x00");
}

TEST(LoggingTest, AppendEscapedStringToMultiple) {
  std::string s;
  AppendEscapedStringTo(&s, Slice("hello"));
  AppendEscapedStringTo(&s, Slice("\x01", 1));
  EXPECT_EQ(s, "hello\\x01");
}

// ============================================================
// ConsumeDecimalNumber
// ============================================================

TEST(LoggingTest, ConsumeDecimalNumberBasic) {
  Slice in("123");
  uint64_t val = 0;
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &val));
  EXPECT_EQ(val, 123u);
  EXPECT_TRUE(in.empty());
}

TEST(LoggingTest, ConsumeDecimalNumberZero) {
  Slice in("0");
  uint64_t val = 42;
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &val));
  EXPECT_EQ(val, 0u);
  EXPECT_TRUE(in.empty());
}

TEST(LoggingTest, ConsumeDecimalNumberLeadingZeros) {
  Slice in("007");
  uint64_t val = 0;
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &val));
  EXPECT_EQ(val, 7u);
  EXPECT_TRUE(in.empty());
}

TEST(LoggingTest, ConsumeDecimalNumberTrailingChars) {
  Slice in("42abc");
  uint64_t val = 0;
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &val));
  EXPECT_EQ(val, 42u);
  EXPECT_EQ(in, Slice("abc"));
}

TEST(LoggingTest, ConsumeDecimalNumberMaxUint64) {
  Slice in("18446744073709551615");
  uint64_t val = 0;
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &val));
  EXPECT_EQ(val, std::numeric_limits<uint64_t>::max());
  EXPECT_TRUE(in.empty());
}

TEST(LoggingTest, ConsumeDecimalNumberOverflowByOne) {
  Slice in("18446744073709551616");
  uint64_t val = 0;
  EXPECT_FALSE(ConsumeDecimalNumber(&in, &val));
}

TEST(LoggingTest, ConsumeDecimalNumberOverflowLarge) {
  Slice in("99999999999999999999");
  uint64_t val = 0;
  EXPECT_FALSE(ConsumeDecimalNumber(&in, &val));
}

TEST(LoggingTest, ConsumeDecimalNumberEmptyInput) {
  Slice in("");
  uint64_t val = 0;
  EXPECT_FALSE(ConsumeDecimalNumber(&in, &val));
}

TEST(LoggingTest, ConsumeDecimalNumberNoDigits) {
  Slice in("abc");
  uint64_t val = 0;
  EXPECT_FALSE(ConsumeDecimalNumber(&in, &val));
}

TEST(LoggingTest, ConsumeDecimalNumberMultipleFromOneSlice) {
  Slice in("123 456 789");
  uint64_t v1 = 0, v2 = 0, v3 = 0;
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &v1));
  EXPECT_EQ(v1, 123u);
  EXPECT_EQ(in, Slice(" 456 789"));

  // Skip the space manually.
  in.remove_prefix(1);
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &v2));
  EXPECT_EQ(v2, 456u);
  EXPECT_EQ(in, Slice(" 789"));

  in.remove_prefix(1);
  ASSERT_TRUE(ConsumeDecimalNumber(&in, &v3));
  EXPECT_EQ(v3, 789u);
  EXPECT_TRUE(in.empty());
}

TEST(LoggingTest, ConsumeDecimalNumberRoundTrip) {
  uint64_t values[] = {0, 1, 9, 10, 42, 99, 100, 999, 1000, 1024,
                       1000000, 123456789, 1000000000000ULL,
                       std::numeric_limits<uint64_t>::max()};
  for (uint64_t v : values) {
    std::string s = NumberToString(v);
    Slice in(s);
    uint64_t parsed = 0;
    ASSERT_TRUE(ConsumeDecimalNumber(&in, &parsed)) << "failed for " << v;
    EXPECT_EQ(parsed, v) << "failed for " << v;
    EXPECT_TRUE(in.empty()) << "remaining data for " << v;
  }
}

TEST(LoggingTest, ConsumeDecimalNumberSingleDigit) {
  for (uint64_t d = 0; d <= 9; ++d) {
    char c = '0' + static_cast<char>(d);
    Slice in(&c, 1);
    uint64_t val = 999;
    ASSERT_TRUE(ConsumeDecimalNumber(&in, &val)) << "failed for digit " << d;
    EXPECT_EQ(val, d);
    EXPECT_TRUE(in.empty());
  }
}

}  // namespace
}  // namespace lldb
