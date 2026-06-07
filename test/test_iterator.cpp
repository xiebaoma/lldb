#include <gtest/gtest.h>

#include <vector>

#include "include/lldb/iterator.h"
#include "include/lldb/status.h"

namespace lldb {
namespace {

// Minimal concrete Iterator for testing the non-abstract members
// (RegisterCleanup, destructor) and factory functions.
class TestIterator : public Iterator {
 public:
  bool Valid() const override { return false; }
  void SeekToFirst() override {}
  void SeekToLast() override {}
  void Seek(const Slice& /*target*/) override {}
  void Next() override { assert(false); }
  void Prev() override { assert(false); }
  Slice key() const override {
    assert(false);
    return Slice();
  }
  Slice value() const override {
    assert(false);
    return Slice();
  }
  Status status() const override { return Status::OK(); }
};

// ============================================================
// RegisterCleanup & Destructor
// ============================================================

TEST(IteratorTest, SingleCleanupCalledOnDestruction) {
  bool called = false;
  {
    TestIterator iter;
    iter.RegisterCleanup(
        [](void* arg1, void* arg2) {
          *static_cast<bool*>(arg1) = true;
          EXPECT_EQ(arg2, reinterpret_cast<void*>(42));
        },
        &called, reinterpret_cast<void*>(42));
    EXPECT_FALSE(called);
  }
  EXPECT_TRUE(called);
}

TEST(IteratorTest, MultipleCleanupsCalledOnDestruction) {
  std::vector<int> order;
  {
    TestIterator iter;
    iter.RegisterCleanup(
        [](void* arg1, void* /*arg2*/) {
          static_cast<std::vector<int>*>(arg1)->push_back(1);
        },
        &order, nullptr);
    iter.RegisterCleanup(
        [](void* arg1, void* /*arg2*/) {
          static_cast<std::vector<int>*>(arg1)->push_back(2);
        },
        &order, nullptr);
    iter.RegisterCleanup(
        [](void* arg1, void* /*arg2*/) {
          static_cast<std::vector<int>*>(arg1)->push_back(3);
        },
        &order, nullptr);
    EXPECT_TRUE(order.empty());
  }
  // LIFO order after the head: 1st → head, 2nd → inserted after head,
  // 3rd → inserted after head (before 2nd). Destructor runs head then
  // traverses: head(1st), then 3rd, then 2nd.
  EXPECT_EQ(order, std::vector<int>({1, 3, 2}));
}

TEST(IteratorTest, CleanupReceivesBothArgs) {
  int int_val = 0;
  std::string str_val;
  {
    TestIterator iter;
    iter.RegisterCleanup(
        [](void* arg1, void* arg2) {
          *static_cast<int*>(arg1) = 99;
          *static_cast<std::string*>(arg2) = "cleaned";
        },
        &int_val, &str_val);
  }
  EXPECT_EQ(int_val, 99);
  EXPECT_EQ(str_val, "cleaned");
}

TEST(IteratorTest, NoCleanupDoesNotCrash) {
  // Destroying an iterator with no registered cleanups should be safe.
  { TestIterator iter; }
  SUCCEED();
}

// ============================================================
// Factory Functions
// ============================================================

TEST(IteratorTest, NewEmptyIteratorIsNotValid) {
  Iterator* iter = NewEmptyIterator();
  EXPECT_FALSE(iter->Valid());
  EXPECT_TRUE(iter->status().ok());
  delete iter;
}

TEST(IteratorTest, NewErrorIteratorIsNotValid) {
  Status s = Status::NotFound(Slice("missing key"));
  Iterator* iter = NewErrorIterator(s);
  EXPECT_FALSE(iter->Valid());
  EXPECT_TRUE(iter->status().IsNotFound());
  delete iter;
}

TEST(IteratorTest, NewErrorIteratorPreservesStatus) {
  Status s = Status::Corruption(Slice("data corruption"));
  Iterator* iter = NewErrorIterator(s);
  EXPECT_FALSE(iter->Valid());
  EXPECT_TRUE(iter->status().IsCorruption());
  EXPECT_EQ(iter->status().ToString(), s.ToString());
  delete iter;
}

}  // namespace
}  // namespace lldb
