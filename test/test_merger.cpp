#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "lldb/comparator.h"
#include "lldb/iterator.h"
#include "lldb/slice.h"
#include "lldb/status.h"
#include "table/merger.h"

namespace lldb {
namespace {

// A concrete Iterator backed by a sorted vector of (key, value) pairs.
class VectorIterator : public Iterator {
 public:
  VectorIterator(const std::vector<std::string>& keys,
                 const std::vector<std::string>& vals)
      : keys_(keys), vals_(vals), pos_(-1) {
    assert(keys_.size() == vals_.size());
  }

  bool Valid() const override {
    return pos_ >= 0 && pos_ < static_cast<int>(keys_.size());
  }

  void SeekToFirst() override { pos_ = keys_.empty() ? -1 : 0; }
  void SeekToLast() override {
    pos_ = keys_.empty() ? -1 : static_cast<int>(keys_.size()) - 1;
  }

  void Seek(const Slice& target) override {
    pos_ = -1;
    for (size_t i = 0; i < keys_.size(); i++) {
      if (keys_[i] >= target.ToString()) {
        pos_ = static_cast<int>(i);
        break;
      }
    }
  }

  void Next() override {
    assert(Valid());
    pos_++;
    if (pos_ >= static_cast<int>(keys_.size())) pos_ = -1;
  }

  void Prev() override {
    assert(Valid());
    pos_--;
  }

  Slice key() const override {
    assert(Valid());
    return Slice(keys_[pos_]);
  }

  Slice value() const override {
    assert(Valid());
    return Slice(vals_[pos_]);
  }

  Status status() const override { return status_; }
  void set_status(Status s) { status_ = s; }

 private:
  std::vector<std::string> keys_;
  std::vector<std::string> vals_;
  int pos_;
  Status status_;
};

// Helper: create a VectorIterator and return it as Iterator*.
// The caller transfers ownership to the MergingIterator.
static Iterator* MakeVI(const std::vector<std::string>& keys,
                        const std::vector<std::string>& vals) {
  return new VectorIterator(keys, vals);
}

// ============================================================
// NewMergingIterator: n = 0, 1
// ============================================================

TEST(MergerTest, ZeroChildrenReturnsEmptyIterator) {
  Iterator* iter = NewMergingIterator(BytewiseComparator(), nullptr, 0);
  ASSERT_NE(iter, nullptr);
  EXPECT_FALSE(iter->Valid());
  EXPECT_TRUE(iter->status().ok());
  delete iter;
}

TEST(MergerTest, OneChildReturnsSameIterator) {
  // For n=1, the child is returned directly (no wrapping, no ownership transfer).
  std::vector<std::string> keys = {"a", "b", "c"};
  std::vector<std::string> vals = {"va", "vb", "vc"};
  VectorIterator v(keys, vals);
  v.SeekToFirst();

  Iterator* child = &v;
  Iterator* iter = NewMergingIterator(BytewiseComparator(), &child, 1);
  ASSERT_NE(iter, nullptr);
  EXPECT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("a"));
  // Must NOT delete iter — it aliases the stack-allocated VectorIterator.
}

// ============================================================
// SeekToFirst / SeekToLast — basic merge
// ============================================================

TEST(MergerTest, SeekToFirstFindsSmallest) {
  Iterator* children[] = {
      MakeVI({"b", "d", "f"}, {"vb", "vd", "vf"}),
      MakeVI({"a", "c", "e"}, {"va", "vc", "ve"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToFirst();
  ASSERT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("a"));
  EXPECT_EQ(iter->value(), Slice("va"));
}

TEST(MergerTest, SeekToFirstWithEmptyChild) {
  Iterator* children[] = {
      MakeVI({"x", "y"}, {"vx", "vy"}),
      MakeVI({}, {}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToFirst();
  ASSERT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("x"));
}

TEST(MergerTest, SeekToFirstAllEmpty) {
  Iterator* children[] = {
      MakeVI({}, {}),
      MakeVI({}, {}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToFirst();
  EXPECT_FALSE(iter->Valid());
}

TEST(MergerTest, SeekToLastFindsLargest) {
  Iterator* children[] = {
      MakeVI({"a", "c", "e"}, {"va", "vc", "ve"}),
      MakeVI({"b", "d", "f"}, {"vb", "vd", "vf"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToLast();
  ASSERT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("f"));
}

// ============================================================
// Seek(target)
// ============================================================

TEST(MergerTest, SeekExactMatch) {
  Iterator* children[] = {
      MakeVI({"a", "c", "e"}, {"va", "vc", "ve"}),
      MakeVI({"b", "d", "f"}, {"vb", "vd", "vf"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->Seek(Slice("c"));
  ASSERT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("c"));
}

TEST(MergerTest, SeekBetweenKeys) {
  Iterator* children[] = {
      MakeVI({"a", "e"}, {"va", "ve"}),
      MakeVI({"c", "g"}, {"vc", "vg"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->Seek(Slice("d"));
  ASSERT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("e"));
}

TEST(MergerTest, SeekBeyondAllKeys) {
  Iterator* children[] = {
      MakeVI({"a", "b"}, {"va", "vb"}),
      MakeVI({"c", "d"}, {"vc", "vd"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->Seek(Slice("z"));
  EXPECT_FALSE(iter->Valid());
}

// ============================================================
// Forward traversal (Next)
// ============================================================

TEST(MergerTest, ForwardTraversalThreeChildren) {
  Iterator* children[] = {
      MakeVI({"a", "d", "g"}, {"va", "vd", "vg"}),
      MakeVI({"b", "e", "h"}, {"vb", "ve", "vh"}),
      MakeVI({"c", "f", "i"}, {"vc", "vf", "vi"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 3));

  iter->SeekToFirst();
  std::string expected = "abcdefghi";
  for (size_t i = 0; i < expected.size(); i++) {
    ASSERT_TRUE(iter->Valid()) << "i=" << i;
    EXPECT_EQ(iter->key().ToString(), std::string(1, expected[i]));
    iter->Next();
  }
  EXPECT_FALSE(iter->Valid());
}

TEST(MergerTest, ForwardTraversalWithEmptyChild) {
  Iterator* children[] = {
      MakeVI({"a", "c", "e"}, {"va", "vc", "ve"}),
      MakeVI({}, {}),
      MakeVI({"b", "d", "f"}, {"vb", "vd", "vf"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 3));

  iter->SeekToFirst();
  std::string result;
  while (iter->Valid()) {
    result += iter->key().ToString();
    iter->Next();
  }
  EXPECT_EQ(result, "abcdef");
}

// ============================================================
// Reverse traversal (Prev)
// ============================================================

TEST(MergerTest, ReverseTraversalThreeChildren) {
  Iterator* children[] = {
      MakeVI({"a", "d", "g"}, {"va", "vd", "vg"}),
      MakeVI({"b", "e", "h"}, {"vb", "ve", "vh"}),
      MakeVI({"c", "f", "i"}, {"vc", "vf", "vi"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 3));

  iter->SeekToLast();
  std::string expected = "ihgfedcba";
  for (size_t i = 0; i < expected.size(); i++) {
    ASSERT_TRUE(iter->Valid()) << "i=" << i;
    EXPECT_EQ(iter->key().ToString(), std::string(1, expected[i]));
    iter->Prev();
  }
  EXPECT_FALSE(iter->Valid());
}

// ============================================================
// Direction switching: Next <-> Prev
// ============================================================

TEST(MergerTest, NextThenPrev) {
  Iterator* children[] = {
      MakeVI({"a", "c", "e"}, {"va", "vc", "ve"}),
      MakeVI({"b", "d", "f"}, {"vb", "vd", "vf"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToFirst();
  EXPECT_EQ(iter->key(), Slice("a"));
  iter->Next();
  EXPECT_EQ(iter->key(), Slice("b"));
  iter->Next();
  EXPECT_EQ(iter->key(), Slice("c"));
  iter->Prev();
  EXPECT_EQ(iter->key(), Slice("b"));
}

TEST(MergerTest, PrevThenNext) {
  Iterator* children[] = {
      MakeVI({"a", "c", "e"}, {"va", "vc", "ve"}),
      MakeVI({"b", "d", "f"}, {"vb", "vd", "vf"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToLast();
  EXPECT_EQ(iter->key(), Slice("f"));
  iter->Prev();
  EXPECT_EQ(iter->key(), Slice("e"));
  iter->Prev();
  EXPECT_EQ(iter->key(), Slice("d"));
  iter->Next();
  EXPECT_EQ(iter->key(), Slice("e"));
}

// ============================================================
// Duplicate keys across children
// ============================================================

TEST(MergerTest, DuplicateKeysAcrossChildren) {
  Iterator* children[] = {
      MakeVI({"a", "b", "c"}, {"v1", "v2", "v3"}),
      MakeVI({"a", "b", "c"}, {"w1", "w2", "w3"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  iter->SeekToFirst();
  // Both children have "a"; the merge produces both in some order.
  EXPECT_EQ(iter->key(), Slice("a"));
  iter->Next();
  EXPECT_EQ(iter->key(), Slice("a"));
  iter->Next();
  EXPECT_EQ(iter->key(), Slice("b"));
}

// ============================================================
// status()
// ============================================================

TEST(MergerTest, StatusReturnsFirstError) {
  VectorIterator* good = new VectorIterator({"a"}, {"va"});
  VectorIterator* bad = new VectorIterator({"b"}, {"vb"});
  bad->set_status(Status::Corruption(Slice("bad data")));

  Iterator* children[] = {good, bad};
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  EXPECT_TRUE(iter->status().IsCorruption());
}

TEST(MergerTest, StatusAllOK) {
  Iterator* children[] = {
      MakeVI({"a"}, {"va"}),
      MakeVI({"b"}, {"vb"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  EXPECT_TRUE(iter->status().ok());
}

// ============================================================
// 5 children, 100 keys total
// ============================================================

TEST(MergerTest, FiveChildrenForwardMerge) {
  // child i gets keys at positions i, i+5, i+10, ... (20 each).
  Iterator* children[5];
  for (int i = 0; i < 5; i++) {
    std::vector<std::string> keys, vals;
    for (int j = 0; j < 20; j++) {
      char c = static_cast<char>('a' + i + j * 5);
      keys.push_back(std::string(1, c));
      vals.push_back("val_" + std::string(1, c));
    }
    children[i] = new VectorIterator(keys, vals);
  }

  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 5));

  iter->SeekToFirst();
  int count = 0;
  std::string prev_key;
  while (iter->Valid()) {
    Slice k = iter->key();
    if (count > 0) {
      EXPECT_LE(prev_key, k.ToString());
    }
    prev_key = k.ToString();
    iter->Next();
    count++;
  }
  EXPECT_EQ(count, 100);
}

// ============================================================
// SeekToFirst after Next exhaustion
// ============================================================

TEST(MergerTest, SeekToFirstAfterExhaustion) {
  Iterator* children[] = {
      MakeVI({"x", "y", "z"}, {"vx", "vy", "vz"}),
      MakeVI({"p", "q"}, {"vp", "vq"}),
  };
  std::unique_ptr<Iterator> iter(
      NewMergingIterator(BytewiseComparator(), children, 2));

  // Walk to exhaustion.
  iter->SeekToFirst();
  while (iter->Valid()) iter->Next();
  EXPECT_FALSE(iter->Valid());

  // Seek again.
  iter->SeekToFirst();
  ASSERT_TRUE(iter->Valid());
  EXPECT_EQ(iter->key(), Slice("p"));
}

}  // namespace
}  // namespace lldb
