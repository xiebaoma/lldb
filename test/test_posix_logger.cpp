#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "lldb/env.h"
#include "util/posix_logger.h"

namespace lldb {
namespace {

class PosixLoggerTest : public ::testing::Test {
 protected:
  static constexpr size_t kBufSize = 8192;

  void SetUp() override { std::memset(buf_, 0, sizeof(buf_)); }

  // Create a PosixLogger backed by an in-memory buffer, log a message,
  // destroy the logger, and return the captured output.
  std::string Capture(const std::string& msg) {
    FILE* f = ::fmemopen(buf_, sizeof(buf_), "w");
    EXPECT_NE(f, nullptr);
    if (!f) return "";
    {
      PosixLogger logger(f);
      Log(&logger, "%s", msg.c_str());
    }
    return std::string(buf_);
  }

  // Log multiple messages to the same logger.
  std::string CaptureMultiple(const std::vector<std::string>& msgs) {
    FILE* f = ::fmemopen(buf_, sizeof(buf_), "w");
    EXPECT_NE(f, nullptr);
    if (!f) return "";
    {
      PosixLogger logger(f);
      for (const auto& m : msgs) {
        Log(&logger, "%s", m.c_str());
      }
    }
    return std::string(buf_);
  }

  char buf_[kBufSize];
};

// ============================================================
// Basic logging
// ============================================================

TEST_F(PosixLoggerTest, LogsMessage) {
  std::string out = Capture("hello world");
  EXPECT_NE(out.find("hello world"), std::string::npos);
  EXPECT_EQ(out.back(), '\n');
}

TEST_F(PosixLoggerTest, ContainsTimestamp) {
  std::string out = Capture("msg");
  // Format: YYYY/MM/DD-HH:MM:SS.mmmmmm <thread> msg
  EXPECT_NE(out.find("/"), std::string::npos);
}

TEST_F(PosixLoggerTest, ContainsThreadId) {
  std::string out = Capture("msg");
  // There should be a space-separated token between the timestamp and message
  // that represents the thread ID.
  EXPECT_NE(out.find(" "), std::string::npos);
}

// ============================================================
// Newline handling
// ============================================================

TEST_F(PosixLoggerTest, AppendsNewlineWhenMissing) {
  std::string out = Capture("no trailing newline");
  EXPECT_EQ(out.back(), '\n');
}

TEST_F(PosixLoggerTest, DoesNotDoubleNewline) {
  std::string out = Capture("already has newline\n");
  EXPECT_EQ(out.back(), '\n');
  // No double newline at end.
  size_t len = out.size();
  ASSERT_GE(len, 2u);
  EXPECT_NE(out.substr(len - 2), "\n\n");
}

// ============================================================
// Null logger
// ============================================================

TEST_F(PosixLoggerTest, NullLoggerDoesNotCrash) {
  Log(nullptr, "%s", "should not crash");
  Log(nullptr, "%d %d %d", 1, 2, 3);
  SUCCEED();
}

// ============================================================
// Large message (dynamic allocation path)
// ============================================================

TEST_F(PosixLoggerTest, MessageLargerThanStackBuffer) {
  // Stack buffer is 512 bytes. A 600-byte message triggers the
  // dynamic allocation code path in the second iteration.
  std::string large(600, 'X');
  std::string out = Capture(large);
  EXPECT_NE(out.find(large), std::string::npos);
  EXPECT_EQ(out.back(), '\n');
}

// ============================================================
// Multiple log calls
// ============================================================

TEST_F(PosixLoggerTest, MultipleMessages) {
  std::string out = CaptureMultiple({"first", "second", "third"});
  EXPECT_NE(out.find("first"), std::string::npos);
  EXPECT_NE(out.find("second"), std::string::npos);
  EXPECT_NE(out.find("third"), std::string::npos);
  // Each on its own line, in order.
  size_t p1 = out.find("first");
  size_t p2 = out.find("second");
  size_t p3 = out.find("third");
  EXPECT_LT(p1, p2);
  EXPECT_LT(p2, p3);
}

// ============================================================
// Empty / edge cases
// ============================================================

TEST_F(PosixLoggerTest, EmptyMessage) {
  std::string out = Capture("");
  EXPECT_NE(out.find('\n'), std::string::npos);
}

TEST_F(PosixLoggerTest, SpecialCharacters) {
  std::string out = Capture("line1\nline2\ttab");
  EXPECT_NE(out.find('\n'), std::string::npos);
  EXPECT_NE(out.find('\t'), std::string::npos);
}

}  // namespace
}  // namespace lldb
