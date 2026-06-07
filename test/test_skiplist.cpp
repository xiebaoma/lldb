#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "memtable/skiplist.h"
#include "util/arena.h"

namespace lldb {
namespace {

// Simple three-way comparator for integer keys.
struct IntComparator {
  int operator()(int a, int b) const {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
};

class SkipListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    arena_.reset(new Arena());
    list_.reset(new SkipList<int, IntComparator>(IntComparator(), arena_.get()));
  }

  void TearDown() override {
    list_.reset();
    arena_.reset();
  }

  std::unique_ptr<Arena> arena_;
  std::unique_ptr<SkipList<int, IntComparator>> list_;
};

// ============================================================
// Empty list
// ============================================================

TEST_F(SkipListTest, EmptyListContains) {
  EXPECT_FALSE(list_->Contains(0));
  EXPECT_FALSE(list_->Contains(42));
}

TEST_F(SkipListTest, EmptyListIterator) {
  typename SkipList<int, IntComparator>::Iterator it(list_.get());

  it.SeekToFirst();
  EXPECT_FALSE(it.Valid());

  it.SeekToLast();
  EXPECT_FALSE(it.Valid());

  it.Seek(0);
  EXPECT_FALSE(it.Valid());
}

// ============================================================
// Basic Insert / Contains
// ============================================================

TEST_F(SkipListTest, InsertAndContains) {
  list_->Insert(5);
  EXPECT_TRUE(list_->Contains(5));
  EXPECT_FALSE(list_->Contains(3));
  EXPECT_FALSE(list_->Contains(7));
}

TEST_F(SkipListTest, InsertMultiple) {
  for (int i = 0; i < 100; ++i) {
    list_->Insert(i);
  }
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(list_->Contains(i)) << "key=" << i;
  }
  EXPECT_FALSE(list_->Contains(-1));
  EXPECT_FALSE(list_->Contains(100));
}

// ============================================================
// Iterator: SeekToFirst / SeekToLast
// ============================================================

TEST_F(SkipListTest, IteratorSeekToFirst) {
  list_->Insert(30);
  list_->Insert(10);
  list_->Insert(20);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 10);
}

TEST_F(SkipListTest, IteratorSeekToLast) {
  list_->Insert(10);
  list_->Insert(30);
  list_->Insert(20);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToLast();
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 30);
}

TEST_F(SkipListTest, IteratorSeekToFirstSingleElement) {
  list_->Insert(99);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 99);

  it.SeekToLast();
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 99);
}

// ============================================================
// Iterator: Seek
// ============================================================

TEST_F(SkipListTest, IteratorSeekExactMatch) {
  for (int i = 0; i < 10; ++i) list_->Insert(i * 10);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.Seek(50);
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 50);
}

TEST_F(SkipListTest, IteratorSeekGreaterOrEqual) {
  for (int i = 0; i < 10; ++i) list_->Insert(i * 10);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.Seek(55);  // No exact match; 60 is the next >= key.
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 60);
}

TEST_F(SkipListTest, IteratorSeekBeyondLargest) {
  for (int i = 0; i < 10; ++i) list_->Insert(i * 10);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.Seek(100);  // Larger than all keys.
  EXPECT_FALSE(it.Valid());
}

TEST_F(SkipListTest, IteratorSeekBeforeSmallest) {
  for (int i = 0; i < 10; ++i) list_->Insert(i * 10);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.Seek(-5);
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 0);  // First key >= -5.
}

// ============================================================
// Iterator: Next / Prev
// ============================================================

TEST_F(SkipListTest, IteratorNextTraversal) {
  for (int i = 0; i < 10; ++i) list_->Insert(i);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();

  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(it.Valid()) << "i=" << i;
    EXPECT_EQ(it.key(), i);
    it.Next();
  }
  EXPECT_FALSE(it.Valid());
}

TEST_F(SkipListTest, IteratorPrevTraversal) {
  for (int i = 0; i < 10; ++i) list_->Insert(i);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToLast();

  for (int i = 9; i >= 0; --i) {
    ASSERT_TRUE(it.Valid()) << "i=" << i;
    EXPECT_EQ(it.key(), i);
    it.Prev();
  }
  EXPECT_FALSE(it.Valid());
}

TEST_F(SkipListTest, IteratorNextPrevRoundTrip) {
  list_->Insert(1);
  list_->Insert(2);
  list_->Insert(3);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  EXPECT_EQ(it.key(), 1);

  it.Next();
  EXPECT_EQ(it.key(), 2);

  it.Prev();
  EXPECT_EQ(it.key(), 1);

  // Prev from the first element should make iterator invalid.
  it.Prev();
  EXPECT_FALSE(it.Valid());
}

// ============================================================
// Forward / backward full iteration
// ============================================================

TEST_F(SkipListTest, FullForwardIteration) {
  std::set<int> keys;
  for (int v : {5, 3, 8, 1, 9, 2, 7, 4, 6, 0}) {
    keys.insert(v);
    list_->Insert(v);
  }

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();

  std::vector<int> result;
  while (it.Valid()) {
    result.push_back(it.key());
    it.Next();
  }
  EXPECT_EQ(result, std::vector<int>(keys.begin(), keys.end()));
}

TEST_F(SkipListTest, FullBackwardIteration) {
  std::set<int> keys;
  for (int v : {5, 3, 8, 1, 9, 2, 7, 4, 6, 0}) {
    keys.insert(v);
    list_->Insert(v);
  }

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToLast();

  std::vector<int> result;
  while (it.Valid()) {
    result.push_back(it.key());
    it.Prev();
  }
  EXPECT_EQ(result, std::vector<int>(keys.rbegin(), keys.rend()));
}

// ============================================================
// Insert in sorted (ascending) order
// ============================================================

TEST_F(SkipListTest, InsertSortedAscending) {
  for (int i = 0; i < 100; ++i) list_->Insert(i);

  // Verify all are present and in order.
  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(it.Valid());
    EXPECT_EQ(it.key(), i);
    it.Next();
  }
  EXPECT_FALSE(it.Valid());
}

// ============================================================
// Insert in reverse (descending) order
// ============================================================

TEST_F(SkipListTest, InsertSortedDescending) {
  for (int i = 99; i >= 0; --i) list_->Insert(i);

  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(list_->Contains(i)) << "key=" << i;
  }

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(it.Valid());
    EXPECT_EQ(it.key(), i);
    it.Next();
  }
}

// ============================================================
// Large dataset — exercises multiple levels
// ============================================================

TEST_F(SkipListTest, LargeDataset) {
  const int N = 10000;
  for (int i = 0; i < N; ++i) list_->Insert(i);

  // Random access via Contains.
  EXPECT_TRUE(list_->Contains(0));
  EXPECT_TRUE(list_->Contains(N / 2));
  EXPECT_TRUE(list_->Contains(N - 1));
  EXPECT_FALSE(list_->Contains(N));

  // Iterator traversal.
  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  int count = 0;
  while (it.Valid()) {
    EXPECT_EQ(it.key(), count);
    count++;
    it.Next();
  }
  EXPECT_EQ(count, N);
}

TEST_F(SkipListTest, LargeDatasetRandomInsertOrder) {
  const int N = 5000;
  std::vector<int> values(N);
  for (int i = 0; i < N; ++i) values[i] = i;

  // Simple shuffle-like reordering: reverse every other pair.
  for (int i = 0; i < N - 1; i += 2) {
    std::swap(values[i], values[i + 1]);
  }

  for (int v : values) list_->Insert(v);

  for (int i = 0; i < N; ++i) {
    EXPECT_TRUE(list_->Contains(i)) << "key=" << i;
  }
}

// ============================================================
// Negative keys
// ============================================================

TEST_F(SkipListTest, NegativeKeys) {
  for (int i = -50; i <= 50; ++i) list_->Insert(i);

  EXPECT_TRUE(list_->Contains(-50));
  EXPECT_TRUE(list_->Contains(0));
  EXPECT_TRUE(list_->Contains(50));
  EXPECT_FALSE(list_->Contains(-51));
  EXPECT_FALSE(list_->Contains(51));

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), -50);

  it.SeekToLast();
  ASSERT_TRUE(it.Valid());
  EXPECT_EQ(it.key(), 50);
}

// ============================================================
// Seek after mutation is NOT tested since Insert requires
// external synchronization and we only test single-threaded.
// ============================================================

// ============================================================
// Iterator copy semantics
// ============================================================

TEST_F(SkipListTest, IteratorCopy) {
  list_->Insert(1);
  list_->Insert(2);

  typename SkipList<int, IntComparator>::Iterator it(list_.get());
  it.SeekToFirst();
  EXPECT_EQ(it.key(), 1);

  typename SkipList<int, IntComparator>::Iterator it2(it);
  EXPECT_TRUE(it2.Valid());
  EXPECT_EQ(it2.key(), 1);

  it2.Next();
  EXPECT_EQ(it2.key(), 2);
  // Original iterator should be unaffected (they point independently).
  EXPECT_EQ(it.key(), 1);
}

}  // namespace
}  // namespace lldb
