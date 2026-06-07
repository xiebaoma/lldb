#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "util/coding.h"

namespace lldb {
namespace {

// ============================================================
// Fixed32
// ============================================================

TEST(CodingTest, Fixed32Zero) {
  char buf[4];
  EncodeFixed32(buf, 0);
  EXPECT_EQ(DecodeFixed32(buf), 0u);
}

TEST(CodingTest, Fixed32Max) {
  char buf[4];
  EncodeFixed32(buf, 0xFFFFFFFF);
  EXPECT_EQ(DecodeFixed32(buf), 0xFFFFFFFFu);
}

TEST(CodingTest, Fixed32RoundTrip) {
  uint32_t values[] = {0, 1, 127, 128, 255, 256, 0xDEADBEEF, 0x7FFFFFFF,
                       0x80000000, 0xFFFFFFFF};
  for (uint32_t v : values) {
    char buf[4];
    EncodeFixed32(buf, v);
    EXPECT_EQ(DecodeFixed32(buf), v) << "failed for " << v;
  }
}

TEST(CodingTest, PutFixed32) {
  std::string s;
  PutFixed32(&s, 0xABCD1234);
  ASSERT_EQ(s.size(), 4u);
  EXPECT_EQ(DecodeFixed32(s.data()), 0xABCD1234u);
}

// ============================================================
// Fixed64
// ============================================================

TEST(CodingTest, Fixed64Zero) {
  char buf[8];
  EncodeFixed64(buf, 0);
  EXPECT_EQ(DecodeFixed64(buf), 0u);
}

TEST(CodingTest, Fixed64Max) {
  char buf[8];
  EncodeFixed64(buf, 0xFFFFFFFFFFFFFFFFULL);
  EXPECT_EQ(DecodeFixed64(buf), 0xFFFFFFFFFFFFFFFFULL);
}

TEST(CodingTest, Fixed64RoundTrip) {
  uint64_t values[] = {0, 1, 127, 128, 255, 256, 0xDEADBEEFCAFE,
                       0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL,
                       0xFFFFFFFFFFFFFFFFULL};
  for (uint64_t v : values) {
    char buf[8];
    EncodeFixed64(buf, v);
    EXPECT_EQ(DecodeFixed64(buf), v) << "failed for " << v;
  }
}

// ============================================================
// Varint32
// ============================================================

TEST(CodingTest, Varint32Zero) {
  std::string s;
  PutVarint32(&s, 0);
  Slice input(s);
  uint32_t v = 0xDEAD;
  ASSERT_TRUE(GetVarint32(&input, &v));
  EXPECT_EQ(v, 0u);
}

TEST(CodingTest, Varint32OneByteBoundary) {
  // 127 fits in 1 byte (no continuation bit).
  std::string s;
  PutVarint32(&s, 127);
  EXPECT_EQ(s.size(), 1u);
  Slice input(s);
  uint32_t v = 0;
  ASSERT_TRUE(GetVarint32(&input, &v));
  EXPECT_EQ(v, 127u);

  // 128 needs 2 bytes.
  std::string s2;
  PutVarint32(&s2, 128);
  EXPECT_EQ(s2.size(), 2u);
  Slice input2(s2);
  ASSERT_TRUE(GetVarint32(&input2, &v));
  EXPECT_EQ(v, 128u);
}

TEST(CodingTest, Varint32BoundaryValues) {
  uint32_t values[] = {0, 1, 127, 128, 16383, 16384, 2097151, 2097152,
                       268435455, 268435456, 0xFFFFFFFF};
  for (uint32_t v : values) {
    std::string s;
    PutVarint32(&s, v);
    Slice input(s);
    uint32_t decoded = 0;
    ASSERT_TRUE(GetVarint32(&input, &decoded)) << "failed for " << v;
    EXPECT_EQ(decoded, v) << "failed for " << v;
  }
}

TEST(CodingTest, Varint32RoundTrip) {
  uint32_t test_vals[] = {0, 1, 9, 42, 99, 127, 128, 200, 255, 256,
                          1000, 16383, 16384, 65535, 65536, 1000000,
                          1000000000, 2000000000, 4000000000u};
  for (uint32_t v : test_vals) {
    std::string s;
    PutVarint32(&s, v);
    Slice input(s);
    uint32_t decoded = 0;
    ASSERT_TRUE(GetVarint32(&input, &decoded)) << "failed for " << v;
    EXPECT_EQ(decoded, v) << "failed for " << v;
  }
}

TEST(CodingTest, GetVarint32AdvancesSlice) {
  std::string s;
  PutVarint32(&s, 42);
  PutVarint32(&s, 99);
  Slice input(s);
  uint32_t v1 = 0, v2 = 0;
  ASSERT_TRUE(GetVarint32(&input, &v1));
  EXPECT_EQ(v1, 42u);
  ASSERT_TRUE(GetVarint32(&input, &v2));
  EXPECT_EQ(v2, 99u);
  EXPECT_TRUE(input.empty());
}

TEST(CodingTest, GetVarint32TruncatedInput) {
  // A single byte with continuation bit set — needs more bytes.
  std::string s;
  PutVarint32(&s, 300);  // multi-byte
  Slice input(s.data(), 1);
  uint32_t v = 0;
  EXPECT_FALSE(GetVarint32(&input, &v));
}

// ============================================================
// Varint64
// ============================================================

TEST(CodingTest, Varint64Zero) {
  std::string s;
  PutVarint64(&s, 0);
  Slice input(s);
  uint64_t v = 1;
  ASSERT_TRUE(GetVarint64(&input, &v));
  EXPECT_EQ(v, 0u);
}

TEST(CodingTest, Varint64BoundaryValues) {
  uint64_t values[] = {0, 1, 127, 128, 16383, 16384, 2097151, 2097152,
                       268435455, 268435456, 1ULL << 34, 1ULL << 42,
                       1ULL << 49, 1ULL << 56, 1ULL << 63,
                       0xFFFFFFFFFFFFFFFFULL};
  for (uint64_t v : values) {
    std::string s;
    PutVarint64(&s, v);
    Slice input(s);
    uint64_t decoded = 0;
    ASSERT_TRUE(GetVarint64(&input, &decoded)) << "failed for " << v;
    EXPECT_EQ(decoded, v) << "failed for " << v;
  }
}

TEST(CodingTest, GetVarint64AdvancesSlice) {
  std::string s;
  PutVarint64(&s, UINT64_C(9999999999));
  PutVarint64(&s, UINT64_C(42));
  Slice input(s);
  uint64_t v1 = 0, v2 = 0;
  ASSERT_TRUE(GetVarint64(&input, &v1));
  EXPECT_EQ(v1, UINT64_C(9999999999));
  ASSERT_TRUE(GetVarint64(&input, &v2));
  EXPECT_EQ(v2, 42u);
}

TEST(CodingTest, GetVarint64TruncatedInput) {
  std::string s;
  PutVarint64(&s, 300);  // multi-byte
  Slice input(s.data(), 1);
  uint64_t v = 0;
  EXPECT_FALSE(GetVarint64(&input, &v));
}

// ============================================================
// VarintLength
// ============================================================

TEST(CodingTest, VarintLength) {
  EXPECT_EQ(VarintLength(0), 1);
  EXPECT_EQ(VarintLength(1), 1);
  EXPECT_EQ(VarintLength(127), 1);
  EXPECT_EQ(VarintLength(128), 2);
  EXPECT_EQ(VarintLength(16383), 2);
  EXPECT_EQ(VarintLength(16384), 3);
  EXPECT_EQ(VarintLength(2097151), 3);
  EXPECT_EQ(VarintLength(2097152), 4);
  EXPECT_EQ(VarintLength(268435455), 4);
  EXPECT_EQ(VarintLength(268435456), 5);
}

// ============================================================
// EncodeVarint32 / EncodeVarint64 (pointer-based)
// ============================================================

TEST(CodingTest, EncodeVarint32Pointer) {
  char buf[5];
  char* end = EncodeVarint32(buf, 300);
  EXPECT_EQ(end - buf, 2);
  uint32_t result = 0;
  const char* q = GetVarint32Ptr(buf, buf + 5, &result);
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(result, 300u);
  EXPECT_EQ(q, end);
}

TEST(CodingTest, EncodeVarint64Pointer) {
  char buf[10];
  char* end = EncodeVarint64(buf, 300);
  EXPECT_EQ(end - buf, 2);
  uint64_t result = 0;
  const char* q = GetVarint64Ptr(buf, buf + 10, &result);
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(result, 300u);
  EXPECT_EQ(q, end);
}

// ============================================================
// GetVarint32Ptr / GetVarint64Ptr
// ============================================================

TEST(CodingTest, GetVarint32PtrNullOnInvalid) {
  char buf[] = {static_cast<char>(0xFF)};  // continuation bit set, no more data
  uint32_t v = 0;
  const char* result = GetVarint32Ptr(buf, buf + 1, &v);
  EXPECT_EQ(result, nullptr);
}

TEST(CodingTest, GetVarint64PtrNullOnInvalid) {
  char buf[] = {static_cast<char>(0xFF)};  // continuation bit set, no more data
  uint64_t v = 0;
  const char* result = GetVarint64Ptr(buf, buf + 1, &v);
  EXPECT_EQ(result, nullptr);
}

TEST(CodingTest, GetVarint32PtrFastPath) {
  // Value < 128 → single byte, no continuation bit.
  char buf[] = {static_cast<char>(65)};
  uint32_t v = 0;
  const char* result = GetVarint32Ptr(buf, buf + 1, &v);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(v, 65u);
  EXPECT_EQ(result, buf + 1);
}

// ============================================================
// LengthPrefixedSlice
// ============================================================

TEST(CodingTest, PutGetLengthPrefixedSlice) {
  std::string s;
  PutLengthPrefixedSlice(&s, Slice("hello"));
  PutLengthPrefixedSlice(&s, Slice("world"));

  Slice input(s);
  Slice result;
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &result));
  EXPECT_EQ(result, Slice("hello"));

  Slice result2;
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &result2));
  EXPECT_EQ(result2, Slice("world"));

  EXPECT_TRUE(input.empty());
}

TEST(CodingTest, PutGetLengthPrefixedSliceEmpty) {
  std::string s;
  PutLengthPrefixedSlice(&s, Slice());
  Slice input(s);
  Slice result;
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.size(), 0u);
}

TEST(CodingTest, GetLengthPrefixedSliceTruncated) {
  std::string s;
  PutLengthPrefixedSlice(&s, Slice("data"));
  Slice input(s.data(), s.size() - 2);
  Slice result;
  EXPECT_FALSE(GetLengthPrefixedSlice(&input, &result));
}

// ============================================================
// Multiple appends to std::string (mixed encoding)
// ============================================================

TEST(CodingTest, MixedEncodeDecode) {
  std::string s;
  PutFixed32(&s, 0x12345678);
  PutVarint32(&s, 1024);
  PutLengthPrefixedSlice(&s, Slice("key"));
  PutVarint64(&s, UINT64_C(9876543210));

  Slice input(s);
  // Read fixed32
  EXPECT_EQ(DecodeFixed32(input.data()), 0x12345678u);
  input.remove_prefix(4);

  // Read varint32
  uint32_t v32 = 0;
  ASSERT_TRUE(GetVarint32(&input, &v32));
  EXPECT_EQ(v32, 1024u);

  // Read length-prefixed slice
  Slice key;
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &key));
  EXPECT_EQ(key, Slice("key"));

  // Read varint64
  uint64_t v64 = 0;
  ASSERT_TRUE(GetVarint64(&input, &v64));
  EXPECT_EQ(v64, UINT64_C(9876543210));
}

}  // namespace
}  // namespace lldb
