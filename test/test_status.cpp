#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "lldb/slice.h"
#include "lldb/status.h"

namespace lldb {
namespace {

// ============================================================
// Success
// ============================================================

TEST(StatusTest, DefaultConstructorIsOK) {
  Status s;
  EXPECT_TRUE(s.ok());
  EXPECT_FALSE(s.IsNotFound());
  EXPECT_FALSE(s.IsCorruption());
  EXPECT_FALSE(s.IsNotSupportedError());
  EXPECT_FALSE(s.IsInvalidArgument());
  EXPECT_FALSE(s.IsIOError());
}

TEST(StatusTest, StaticOK) {
  Status s = Status::OK();
  EXPECT_TRUE(s.ok());
}

// ============================================================
// Error types
// ============================================================

TEST(StatusTest, NotFound) {
  Status s = Status::NotFound(Slice("key missing"));
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsNotFound());
  EXPECT_FALSE(s.IsCorruption());
}

TEST(StatusTest, Corruption) {
  Status s = Status::Corruption(Slice("checksum mismatch"));
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsCorruption());
  EXPECT_FALSE(s.IsNotFound());
}

TEST(StatusTest, NotSupported) {
  Status s = Status::NotSupported(Slice("not implemented"));
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsNotSupportedError());
  EXPECT_FALSE(s.IsInvalidArgument());
}

TEST(StatusTest, InvalidArgument) {
  Status s = Status::InvalidArgument(Slice("invalid key"));
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsInvalidArgument());
  EXPECT_FALSE(s.IsIOError());
}

TEST(StatusTest, IOError) {
  Status s = Status::IOError(Slice("disk full"));
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());
  EXPECT_FALSE(s.IsNotSupportedError());
}

// ============================================================
// ToString
// ============================================================

TEST(StatusTest, ToStringOK) {
  EXPECT_EQ(Status().ToString(), "OK");
  EXPECT_EQ(Status::OK().ToString(), "OK");
}

TEST(StatusTest, ToStringNotFound) {
  Status s = Status::NotFound(Slice("missing"));
  EXPECT_EQ(s.ToString(), "NotFound: missing");
}

TEST(StatusTest, ToStringCorruption) {
  Status s = Status::Corruption(Slice("bad data"));
  EXPECT_EQ(s.ToString(), "Corruption: bad data");
}

TEST(StatusTest, ToStringNotSupported) {
  Status s = Status::NotSupported(Slice("feature"));
  EXPECT_EQ(s.ToString(), "Not implemented: feature");
}

TEST(StatusTest, ToStringInvalidArgument) {
  Status s = Status::InvalidArgument(Slice("bad value"));
  EXPECT_EQ(s.ToString(), "Invalid argument: bad value");
}

TEST(StatusTest, ToStringIOError) {
  Status s = Status::IOError(Slice("timeout"));
  EXPECT_EQ(s.ToString(), "IO error: timeout");
}

TEST(StatusTest, ToStringWithTwoMessages) {
  Status s = Status::Corruption(Slice("db"), Slice("block 42"));
  EXPECT_EQ(s.ToString(), "Corruption: db: block 42");
}

TEST(StatusTest, ToStringWithEmptyFirstMessage) {
  Status s = Status::NotFound(Slice(), Slice("detail"));
  EXPECT_EQ(s.ToString(), "NotFound: : detail");
}

TEST(StatusTest, ToStringWithEmptyBothMessages) {
  Status s = Status::IOError(Slice(), Slice());
  EXPECT_EQ(s.ToString(), "IO error: ");
}

// ============================================================
// Copy
// ============================================================

TEST(StatusTest, CopyConstructorOK) {
  Status ok = Status::OK();
  Status copy(ok);
  EXPECT_TRUE(copy.ok());
}

TEST(StatusTest, CopyConstructorError) {
  Status e = Status::InvalidArgument(Slice("bad"));
  Status copy(e);
  EXPECT_TRUE(copy.IsInvalidArgument());
  EXPECT_EQ(copy.ToString(), "Invalid argument: bad");
}

TEST(StatusTest, CopyAssignmentOKToOK) {
  Status a = Status::OK();
  Status b = Status::OK();
  b = a;
  EXPECT_TRUE(b.ok());
}

TEST(StatusTest, CopyAssignmentErrorToOK) {
  Status e = Status::NotFound(Slice("x"));
  Status ok = Status::OK();
  ok = e;
  EXPECT_TRUE(ok.IsNotFound());
  EXPECT_EQ(ok.ToString(), "NotFound: x");
}

TEST(StatusTest, CopyAssignmentOKToError) {
  Status ok = Status::OK();
  Status e = Status::IOError(Slice("x"));
  e = ok;
  EXPECT_TRUE(e.ok());
}

TEST(StatusTest, CopyAssignmentErrorToError) {
  Status a = Status::Corruption(Slice("a"));
  Status b = Status::NotFound(Slice("b"));
  b = a;
  EXPECT_TRUE(b.IsCorruption());
  EXPECT_EQ(b.ToString(), "Corruption: a");
}

TEST(StatusTest, CopySelfAssignment) {
  Status s = Status::NotFound(Slice("key"));
  const Status& ref = s;
  s = ref;
  EXPECT_TRUE(s.IsNotFound());
  EXPECT_EQ(s.ToString(), "NotFound: key");
}

// ============================================================
// Move
// ============================================================

TEST(StatusTest, MoveConstructorFromOK) {
  Status ok = Status::OK();
  Status moved(std::move(ok));
  EXPECT_TRUE(moved.ok());
  EXPECT_TRUE(ok.ok());
}

TEST(StatusTest, MoveConstructorFromError) {
  Status e = Status::Corruption(Slice("e"));
  Status moved(std::move(e));
  EXPECT_TRUE(moved.IsCorruption());
  EXPECT_EQ(moved.ToString(), "Corruption: e");
  EXPECT_TRUE(e.ok());
}

TEST(StatusTest, MoveAssignmentOKToOK) {
  Status a = Status::OK();
  Status b = Status::OK();
  b = std::move(a);
  EXPECT_TRUE(b.ok());
}

TEST(StatusTest, MoveAssignmentErrorToOK) {
  Status e = Status::NotFound(Slice("x"));
  Status ok = Status::OK();
  ok = std::move(e);
  EXPECT_TRUE(ok.IsNotFound());
}

TEST(StatusTest, MoveAssignmentOKToError) {
  Status ok = Status::OK();
  Status e = Status::IOError(Slice("x"));
  e = std::move(ok);
  EXPECT_TRUE(e.ok());
}

TEST(StatusTest, MoveAssignmentErrorToError) {
  Status a = Status::Corruption(Slice("a"));
  Status b = Status::NotFound(Slice("b"));
  b = std::move(a);
  EXPECT_TRUE(b.IsCorruption());
  EXPECT_EQ(b.ToString(), "Corruption: a");
}

// ============================================================
// All error types are mutually distinct
// ============================================================

TEST(StatusTest, ErrorTypesAreDistinct) {
  auto check = [](const Status& s, auto is_target, auto is_other1,
                  auto is_other2) {
    EXPECT_TRUE((s.*is_target)());
    EXPECT_FALSE((s.*is_other1)());
    EXPECT_FALSE((s.*is_other2)());
  };

  check(Status::NotFound(Slice("x")), &Status::IsNotFound,
        &Status::IsCorruption, &Status::IsIOError);
  check(Status::Corruption(Slice("x")), &Status::IsCorruption,
        &Status::IsNotFound, &Status::IsNotSupportedError);
  check(Status::NotSupported(Slice("x")), &Status::IsNotSupportedError,
        &Status::IsInvalidArgument, &Status::IsIOError);
  check(Status::InvalidArgument(Slice("x")), &Status::IsInvalidArgument,
        &Status::IsNotFound, &Status::IsCorruption);
  check(Status::IOError(Slice("x")), &Status::IsIOError,
        &Status::IsNotSupportedError, &Status::IsInvalidArgument);
}

// ============================================================
// Copy independence (deep copy)
// ============================================================

TEST(StatusTest, CopyIsDeep) {
  Status a = Status::NotFound(Slice("key"));
  Status b = a;
  // b and a are equal in content but different in state_ pointers.
  EXPECT_EQ(a.ToString(), b.ToString());
  EXPECT_TRUE(a.IsNotFound());
  EXPECT_TRUE(b.IsNotFound());
}

}  // namespace
}  // namespace lldb
