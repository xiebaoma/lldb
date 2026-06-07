#include <gtest/gtest.h>

#include "include/lldb/iterator.h"
#include "include/lldb/status.h"
#include "table/iterator_wrapper.h"

namespace lldb {
namespace {

// A controllable concrete Iterator for exercising IteratorWrapper.
class MockIterator : public Iterator {
 public:
  MockIterator() = default;

  bool Valid() const override { return valid_; }
  void SeekToFirst() override {
    valid_ = true;
    key_slice_ = Slice("first_key");
    val_slice_ = Slice("first_val");
  }
  void SeekToLast() override {
    valid_ = true;
    key_slice_ = Slice("last_key");
    val_slice_ = Slice("last_val");
  }
  void Seek(const Slice& /*target*/) override {
    valid_ = true;
    key_slice_ = Slice("sought_key");
    val_slice_ = Slice("sought_val");
  }
  void Next() override { valid_ = has_next_; }
  void Prev() override { valid_ = has_prev_; }
  Slice key() const override { return key_slice_; }
  Slice value() const override { return val_slice_; }
  Status status() const override { return status_; }

  void set_valid(bool v) { valid_ = v; }
  void set_key(Slice s) { key_slice_ = s; }
  void set_value(Slice s) { val_slice_ = s; }
  void set_status(Status s) { status_ = s; }
  void set_has_next(bool v) { has_next_ = v; }
  void set_has_prev(bool v) { has_prev_ = v; }

 private:
  bool valid_ = false;
  Slice key_slice_;
  Slice val_slice_;
  Status status_;
  bool has_next_ = false;
  bool has_prev_ = false;
};

// ============================================================
// Construction & Destruction
// ============================================================

TEST(IteratorWrapperTest, DefaultConstructor) {
  IteratorWrapper w;
  EXPECT_EQ(w.iter(), nullptr);
  EXPECT_FALSE(w.Valid());
}

TEST(IteratorWrapperTest, ConstructorFromIterator) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_key(Slice("hello"));
  m->set_value(Slice("world"));

  IteratorWrapper w(m);
  EXPECT_EQ(w.iter(), m);
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("hello"));
  EXPECT_EQ(w.value(), Slice("world"));
}

TEST(IteratorWrapperTest, DestructorDeletesIterator) {
  bool deleted = false;
  auto* iter = new MockIterator();
  iter->RegisterCleanup(
      [](void* arg1, void* /*arg2*/) { *static_cast<bool*>(arg1) = true; },
      &deleted, nullptr);
  { IteratorWrapper w(iter); }
  EXPECT_TRUE(deleted);
}

TEST(IteratorWrapperTest, DestructorHandlesNullIterator) {
  // Default-constructed IteratorWrapper has null iter_; destruction is safe.
  { IteratorWrapper w; }
  SUCCEED();
}

// ============================================================
// Set
// ============================================================

TEST(IteratorWrapperTest, SetReplacesIterator) {
  bool first_deleted = false;
  auto* first = new MockIterator();
  first->RegisterCleanup(
      [](void* arg1, void* /*arg2*/) { *static_cast<bool*>(arg1) = true; },
      &first_deleted, nullptr);

  IteratorWrapper w(first);
  EXPECT_FALSE(first_deleted);

  auto* second = new MockIterator();
  w.Set(second);
  EXPECT_TRUE(first_deleted);
  EXPECT_EQ(w.iter(), second);
}

TEST(IteratorWrapperTest, SetNull) {
  bool deleted = false;
  auto* iter = new MockIterator();
  iter->RegisterCleanup(
      [](void* arg1, void* /*arg2*/) { *static_cast<bool*>(arg1) = true; },
      &deleted, nullptr);

  IteratorWrapper w(iter);
  w.Set(nullptr);
  EXPECT_TRUE(deleted);
  EXPECT_EQ(w.iter(), nullptr);
  EXPECT_FALSE(w.Valid());
}

TEST(IteratorWrapperTest, SetNullOnDefaultWrapper) {
  IteratorWrapper w;
  w.Set(nullptr);
  EXPECT_EQ(w.iter(), nullptr);
  EXPECT_FALSE(w.Valid());
}

TEST(IteratorWrapperTest, SetCallsUpdate) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_key(Slice("key"));
  m->set_value(Slice("val"));

  IteratorWrapper w;
  w.Set(m);
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("key"));
  EXPECT_EQ(w.value(), Slice("val"));
}

// ============================================================
// Delegation — Valid / key / value / status
// ============================================================

TEST(IteratorWrapperTest, KeyAndValueDelegation) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_key(Slice("k"));
  m->set_value(Slice("v"));

  IteratorWrapper w(m);
  EXPECT_EQ(w.key(), Slice("k"));
  EXPECT_EQ(w.value(), Slice("v"));
}

TEST(IteratorWrapperTest, StatusDelegation) {
  auto* m = new MockIterator();
  Status s = Status::IOError(Slice("disk full"));
  m->set_status(s);

  IteratorWrapper w(m);
  EXPECT_TRUE(w.status().IsIOError());
}

// ============================================================
// Delegation — navigation methods
// ============================================================

TEST(IteratorWrapperTest, SeekToFirst) {
  auto* m = new MockIterator();
  IteratorWrapper w(m);
  w.SeekToFirst();
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("first_key"));
  EXPECT_EQ(w.value(), Slice("first_val"));
}

TEST(IteratorWrapperTest, SeekToLast) {
  auto* m = new MockIterator();
  IteratorWrapper w(m);
  w.SeekToLast();
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("last_key"));
  EXPECT_EQ(w.value(), Slice("last_val"));
}

TEST(IteratorWrapperTest, Seek) {
  auto* m = new MockIterator();
  IteratorWrapper w(m);
  w.Seek(Slice("target"));
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("sought_key"));
  EXPECT_EQ(w.value(), Slice("sought_val"));
}

TEST(IteratorWrapperTest, Next) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_key(Slice("before"));
  IteratorWrapper w(m);
  EXPECT_TRUE(w.Valid());

  m->set_has_next(true);
  m->set_key(Slice("after"));
  w.Next();
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("after"));
}

TEST(IteratorWrapperTest, NextAtEnd) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_has_next(false);
  IteratorWrapper w(m);
  EXPECT_TRUE(w.Valid());

  w.Next();
  EXPECT_FALSE(w.Valid());
}

TEST(IteratorWrapperTest, Prev) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_key(Slice("before"));
  IteratorWrapper w(m);
  EXPECT_TRUE(w.Valid());

  m->set_has_prev(true);
  m->set_key(Slice("prev_key"));
  w.Prev();
  EXPECT_TRUE(w.Valid());
  EXPECT_EQ(w.key(), Slice("prev_key"));
}

TEST(IteratorWrapperTest, PrevAtBeginning) {
  auto* m = new MockIterator();
  m->set_valid(true);
  m->set_has_prev(false);
  IteratorWrapper w(m);
  EXPECT_TRUE(w.Valid());

  w.Prev();
  EXPECT_FALSE(w.Valid());
}

// ============================================================
// Valid reflects iterator state after each operation
// ============================================================

TEST(IteratorWrapperTest, ValidTracksIteratorState) {
  auto* m = new MockIterator();
  IteratorWrapper w(m);

  EXPECT_FALSE(w.Valid());
  m->set_valid(true);
  m->set_key(Slice("k"));
  w.SeekToFirst();
  EXPECT_TRUE(w.Valid());
}

// ============================================================
// Key caching
// ============================================================

TEST(IteratorWrapperTest, KeyCachedAfterSeek) {
  auto* m = new MockIterator();
  IteratorWrapper w(m);
  w.SeekToFirst();

  // Change the iterator's key after the wrapper has cached it.
  m->set_key(Slice("stale"));
  // Wrapper should still return the cached key.
  EXPECT_EQ(w.key(), Slice("first_key"));
}

TEST(IteratorWrapperTest, ValueNotCached) {
  auto* m = new MockIterator();
  IteratorWrapper w(m);
  w.SeekToFirst();

  // Value is not cached — it delegates to iter_->value() each time.
  m->set_value(Slice("new_value"));
  EXPECT_EQ(w.value(), Slice("new_value"));
}

}  // namespace
}  // namespace lldb
