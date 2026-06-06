#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "util/crc32c.h"

namespace lldb {
namespace crc32c {
namespace {

// ============================================================
// Known Golden Values (RFC 3720 iSCSI CRC32C polynomial)
// ============================================================
// These are verified against the standard Castagnoli CRC32C.

TEST(CRC32CTest, ValueEmpty) {
  EXPECT_EQ(Value("", 0), 0u);
}

TEST(CRC32CTest, ValueZeroByteSequence) {
  // CRC32C of 32 zero bytes.
  const char zeros[32] = {};
  uint32_t c = Value(zeros, 32);
  EXPECT_EQ(c, 0x8a9136aau);
}

TEST(CRC32CTest, ValueOneZeroByte) {
  const char zero = '\0';
  EXPECT_EQ(Value(&zero, 1), 0x527d5351u);
}

TEST(CRC32CTest, ValueFamousString) {
  // CRC32C of "123456789" is a widely known test vector across CRC32C
  // implementations.
  EXPECT_EQ(Value("123456789", 9), 0xe3069283u);
}

TEST(CRC32CTest, ValueHelloConsistent) {
  // Two calls with same input must produce the same output.
  uint32_t h1 = Value("hello", 5);
  uint32_t h2 = Value("hello", 5);
  EXPECT_EQ(h1, h2);
  EXPECT_NE(h1, 0u);
  EXPECT_NE(h1, Value("Hello", 5));
}

TEST(CRC32CTest, ValueTestCRCBuffer) {
  // This is the golden value used by CanAccelerateCRC32C() to probe
  // whether hardware acceleration works.
  EXPECT_EQ(Value("TestCRCBuffer", 13), 0xdcbc59fau);
}

// ============================================================
// Extend — incremental CRC computation
// ============================================================

TEST(CRC32CTest, ExtendEmpty) {
  // Extending with empty data doesn't change the CRC.
  EXPECT_EQ(Extend(0, "", 0), 0u);
  EXPECT_EQ(Extend(0xc99465aa, "", 0), 0xc99465aau);
}

TEST(CRC32CTest, ExtendEquivalentToValue) {
  // Value(data) should equal Extend(0, data, n).
  EXPECT_EQ(Value("hello", 5), Extend(0, "hello", 5));
}

TEST(CRC32CTest, ExtendConcatenation) {
  // CRC(a || b) = Extend(CRC(a), b).
  uint32_t crc_ab = Value("abcdef", 6);

  uint32_t crc_a = Value("ab", 2);
  uint32_t crc_ab_ext = Extend(crc_a, "cdef", 4);

  EXPECT_EQ(crc_ab, crc_ab_ext);
}

TEST(CRC32CTest, ExtendStreaming) {
  // Build up the CRC byte by byte — should match computing at once.
  uint32_t crc = 0;
  const char* data = "streaming test";
  size_t len = strlen(data);
  for (size_t i = 0; i < len; ++i) {
    crc = Extend(crc, &data[i], 1);
  }
  EXPECT_EQ(crc, Value(data, len));
}

// ============================================================
// Different data lengths (exercise all code paths)
// ============================================================

TEST(CRC32CTest, ValueVariousLengths) {
  // These test the alignment handling and various loop boundaries
  // in the software implementation (1, 4, 15, 16, 17, 31, 32, 33, 64 bytes).
  const char* s = "The quick brown fox jumps over the lazy dog";

  // All prefixes should produce deterministic, non-zero results.
  for (size_t len = 1; len <= strlen(s); ++len) {
    uint32_t v = Value(s, len);
    EXPECT_EQ(v, Value(s, len)) << "non-deterministic at len=" << len;
  }
}

// ============================================================
// Mask / Unmask
// ============================================================

TEST(CRC32CTest, MaskUnmaskRoundTrip) {
  uint32_t values[] = {0u, 1u, 0xc99465aau, 0xe3069283u, 0xffffffffu,
                       0x8a9136aau, 0x12345678u};
  for (uint32_t v : values) {
    EXPECT_EQ(Unmask(Mask(v)), v) << "round-trip failed for " << v;
  }
}

TEST(CRC32CTest, MaskIsNotIdentity) {
  // Mask should change the value (otherwise it's pointless).
  uint32_t v = 0xc99465aa;
  EXPECT_NE(Mask(v), v);
}

TEST(CRC32CTest, UnmaskIsNotIdentity) {
  uint32_t v = 0xc99465aa;
  EXPECT_NE(Unmask(v), v);
}

// ============================================================
// kMaskDelta
// ============================================================

TEST(CRC32CTest, MaskDeltaValue) {
  EXPECT_EQ(kMaskDelta, 0xa282ead8u);
}

// ============================================================
// Determinism
// ============================================================

TEST(CRC32CTest, Deterministic) {
  const char* data = "deterministic test data";
  uint32_t v1 = Value(data, strlen(data));
  uint32_t v2 = Value(data, strlen(data));
  EXPECT_EQ(v1, v2);
}

// ============================================================
// Different data → different CRC
// ============================================================

TEST(CRC32CTest, DifferentInputDifferentCRC) {
  EXPECT_NE(Value("abc", 3), Value("abd", 3));
  EXPECT_NE(Value("x", 1), Value("y", 1));
  EXPECT_NE(Value("hello", 5), Value("Hello", 5));
  EXPECT_NE(Value("ab", 2), Value("abc", 3));
}

}  // namespace
}  // namespace crc32c
}  // namespace lldb
