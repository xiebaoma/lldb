#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "db/dbformat.h"
#include "lldb/comparator.h"
#include "lldb/filter_policy.h"
#include "lldb/slice.h"

namespace lldb {
namespace {

// ============================================================
// Config constants
// ============================================================

TEST(DBFormatTest, ConfigConstants) {
  EXPECT_EQ(config::kNumLevels, 7);
  EXPECT_EQ(config::kL0_CompactionTrigger, 4);
  EXPECT_EQ(config::kL0_SlowdownWritesTrigger, 8);
  EXPECT_EQ(config::kL0_StopWritesTrigger, 12);
  EXPECT_EQ(config::kMaxMemCompactLevel, 2);
  EXPECT_EQ(config::kReadBytesPeriod, 1048576);
}

// ============================================================
// ValueType
// ============================================================

TEST(DBFormatTest, ValueTypeValues) {
  EXPECT_EQ(static_cast<int>(kTypeDeletion), 0);
  EXPECT_EQ(static_cast<int>(kTypeValue), 1);
}

TEST(DBFormatTest, KValueTypeForSeek) {
  // Must be the higher-numbered type so that seeks position correctly
  // when sequence numbers sort in decreasing order.
  EXPECT_EQ(kValueTypeForSeek, kTypeValue);
}

// ============================================================
// kMaxSequenceNumber
// ============================================================

TEST(DBFormatTest, MaxSequenceNumber) {
  EXPECT_EQ(kMaxSequenceNumber, (0x1ull << 56) - 1);
}

// ============================================================
// InternalKeyEncodingLength
// ============================================================

TEST(DBFormatTest, InternalKeyEncodingLength) {
  ParsedInternalKey key(Slice("hello"), 5, kTypeValue);
  // 5-byte user key + 8-byte tag = 13.
  EXPECT_EQ(InternalKeyEncodingLength(key), 13u);
}

TEST(DBFormatTest, InternalKeyEncodingLengthEmptyUserKey) {
  ParsedInternalKey key(Slice(""), 0, kTypeDeletion);
  EXPECT_EQ(InternalKeyEncodingLength(key), 8u);
}

// ============================================================
// AppendInternalKey / ParseInternalKey round-trip
// ============================================================

TEST(DBFormatTest, AppendAndParseRoundTrip) {
  ParsedInternalKey orig(Slice("mykey"), 42, kTypeValue);
  std::string encoded;
  AppendInternalKey(&encoded, orig);

  EXPECT_EQ(encoded.size(), 5u + 8u);

  ParsedInternalKey parsed;
  ASSERT_TRUE(ParseInternalKey(encoded, &parsed));
  EXPECT_EQ(parsed.user_key, Slice("mykey"));
  EXPECT_EQ(parsed.sequence, 42u);
  EXPECT_EQ(parsed.type, kTypeValue);
}

TEST(DBFormatTest, AppendAndParseDeletionKey) {
  ParsedInternalKey orig(Slice("del"), 100, kTypeDeletion);
  std::string encoded;
  AppendInternalKey(&encoded, orig);

  ParsedInternalKey parsed;
  ASSERT_TRUE(ParseInternalKey(encoded, &parsed));
  EXPECT_EQ(parsed.user_key, Slice("del"));
  EXPECT_EQ(parsed.sequence, 100u);
  EXPECT_EQ(parsed.type, kTypeDeletion);
}

TEST(DBFormatTest, AppendAndParseMaxSequence) {
  ParsedInternalKey orig(Slice("max"), kMaxSequenceNumber, kTypeValue);
  std::string encoded;
  AppendInternalKey(&encoded, orig);

  ParsedInternalKey parsed;
  ASSERT_TRUE(ParseInternalKey(encoded, &parsed));
  EXPECT_EQ(parsed.sequence, kMaxSequenceNumber);
}

TEST(DBFormatTest, AppendAndParseSequenceZero) {
  ParsedInternalKey orig(Slice("zero"), 0, kTypeValue);
  std::string encoded;
  AppendInternalKey(&encoded, orig);

  ParsedInternalKey parsed;
  ASSERT_TRUE(ParseInternalKey(encoded, &parsed));
  EXPECT_EQ(parsed.sequence, 0u);
  EXPECT_EQ(parsed.type, kTypeValue);
}

// ============================================================
// ParseInternalKey — error cases
// ============================================================

TEST(DBFormatTest, ParseInternalKeyTooShort) {
  // Less than 8 bytes is invalid.
  ParsedInternalKey parsed;
  EXPECT_FALSE(ParseInternalKey(Slice("short"), &parsed));
  EXPECT_FALSE(ParseInternalKey(Slice("1234567"), &parsed));
}

TEST(DBFormatTest, ParseInternalKeyExactlyEightBytes) {
  // 8 bytes with valid type — should succeed.
  std::string enc;
  AppendInternalKey(&enc, ParsedInternalKey(Slice(""), 1, kTypeValue));

  ParsedInternalKey parsed;
  EXPECT_TRUE(ParseInternalKey(Slice(enc), &parsed));
  EXPECT_EQ(parsed.sequence, 1u);
  EXPECT_EQ(parsed.type, kTypeValue);
}

TEST(DBFormatTest, ParseInternalKeyInvalidType) {
  // Manually craft a key with an invalid type byte (2).
  std::string bad;
  bad.append(1, 'x');  // 1-byte user key
  // Append tag: sequence=0, type=2 (invalid)
  PutFixed64(&bad, (0ull << 8) | 2);

  ParsedInternalKey parsed;
  EXPECT_FALSE(ParseInternalKey(Slice(bad), &parsed));
}

// ============================================================
// ExtractUserKey
// ============================================================

TEST(DBFormatTest, ExtractUserKey) {
  std::string enc;
  AppendInternalKey(&enc, ParsedInternalKey(Slice("data"), 7, kTypeValue));

  Slice user = ExtractUserKey(enc);
  EXPECT_EQ(user, Slice("data"));
  EXPECT_EQ(user.size(), 4u);
}

TEST(DBFormatTest, ExtractUserKeyEmpty) {
  std::string enc;
  AppendInternalKey(&enc, ParsedInternalKey(Slice(""), 0, kTypeValue));

  Slice user = ExtractUserKey(enc);
  EXPECT_EQ(user.size(), 0u);
  EXPECT_TRUE(user.empty());
}

// ============================================================
// InternalKey class
// ============================================================

TEST(InternalKeyTest, ConstructAndEncode) {
  InternalKey key(Slice("alpha"), 10, kTypeValue);
  Slice encoded = key.Encode();
  EXPECT_EQ(encoded.size(), 5u + 8u);
  EXPECT_EQ(key.user_key(), Slice("alpha"));
}

TEST(InternalKeyTest, DefaultConstructThenSetFrom) {
  InternalKey key;
  // Default-constructed key has empty rep_ (invalid state).
  // SetFrom should populate it.
  key.SetFrom(ParsedInternalKey(Slice("reinit"), 3, kTypeDeletion));
  EXPECT_FALSE(key.Encode().empty());
  EXPECT_EQ(key.user_key(), Slice("reinit"));
}

TEST(InternalKeyTest, DecodeFrom) {
  InternalKey key;
  std::string enc;
  AppendInternalKey(&enc, ParsedInternalKey(Slice("beta"), 3, kTypeDeletion));

  ASSERT_TRUE(key.DecodeFrom(enc));
  EXPECT_EQ(key.user_key(), Slice("beta"));
  // The encoded form should round-trip through Parse.
  ParsedInternalKey parsed;
  ASSERT_TRUE(ParseInternalKey(key.Encode(), &parsed));
  EXPECT_EQ(parsed.sequence, 3u);
  EXPECT_EQ(parsed.type, kTypeDeletion);
}

TEST(InternalKeyTest, DecodeFromEmpty) {
  InternalKey key;
  EXPECT_FALSE(key.DecodeFrom(Slice("")));
}

TEST(InternalKeyTest, SetFrom) {
  InternalKey key;
  ParsedInternalKey p(Slice("gamma"), 99, kTypeValue);
  key.SetFrom(p);

  EXPECT_EQ(key.user_key(), Slice("gamma"));
  ParsedInternalKey parsed;
  ASSERT_TRUE(ParseInternalKey(key.Encode(), &parsed));
  EXPECT_EQ(parsed.sequence, 99u);
  EXPECT_EQ(parsed.type, kTypeValue);
}

TEST(InternalKeyTest, Clear) {
  InternalKey key(Slice("delta"), 5, kTypeValue);
  EXPECT_FALSE(key.Encode().empty());
  key.Clear();
  // After Clear, DecodeFrom("") should return false (empty rep_).
  EXPECT_FALSE(key.DecodeFrom(Slice("")));
}

TEST(InternalKeyTest, DebugStringNotEmpty) {
  InternalKey key(Slice("debug"), 1, kTypeValue);
  EXPECT_FALSE(key.DebugString().empty());
}

// ============================================================
// ParsedInternalKey::DebugString
// ============================================================

TEST(DBFormatTest, ParsedInternalKeyDebugString) {
  ParsedInternalKey key(Slice("test"), 42, kTypeValue);
  std::string ds = key.DebugString();
  EXPECT_FALSE(ds.empty());
  // Should contain the user key and sequence number.
  EXPECT_NE(ds.find("test"), std::string::npos);
  EXPECT_NE(ds.find("42"), std::string::npos);
}

// ============================================================
// InternalKeyComparator
// ============================================================

class InternalKeyComparatorTest : public ::testing::Test {
 protected:
  InternalKeyComparatorTest()
      : cmp_(InternalKeyComparator(BytewiseComparator())) {}

  // Build an internal key string for testing comparisons.
  std::string MakeKey(const Slice& user_key, SequenceNumber seq,
                      ValueType type) {
    std::string result;
    AppendInternalKey(&result, ParsedInternalKey(user_key, seq, type));
    return result;
  }

  InternalKeyComparator cmp_;
};

TEST_F(InternalKeyComparatorTest, CompareByUserKey) {
  std::string a = MakeKey(Slice("aaa"), 10, kTypeValue);
  std::string b = MakeKey(Slice("bbb"), 10, kTypeValue);
  EXPECT_LT(cmp_.Compare(a, b), 0);
  EXPECT_GT(cmp_.Compare(b, a), 0);
}

TEST_F(InternalKeyComparatorTest, SameUserKeyDecreasingSequence) {
  // Same user key → higher sequence number sorts first (decreasing order).
  std::string a = MakeKey(Slice("key"), 100, kTypeValue);
  std::string b = MakeKey(Slice("key"), 50, kTypeValue);
  // a has higher sequence → a < b (comes first).
  EXPECT_LT(cmp_.Compare(a, b), 0);
  EXPECT_GT(cmp_.Compare(b, a), 0);
}

TEST_F(InternalKeyComparatorTest, SameUserKeySameSequenceDifferentType) {
  std::string a = MakeKey(Slice("key"), 10, kTypeValue);
  std::string b = MakeKey(Slice("key"), 10, kTypeDeletion);
  // kTypeValue (1) > kTypeDeletion (0), so 'a' encodes a larger tag.
  // Since we use decreasing order, larger tag = smaller key.
  EXPECT_LT(cmp_.Compare(a, b), 0);
}

TEST_F(InternalKeyComparatorTest, EqualKeys) {
  std::string a = MakeKey(Slice("key"), 42, kTypeValue);
  std::string b = MakeKey(Slice("key"), 42, kTypeValue);
  EXPECT_EQ(cmp_.Compare(a, b), 0);
}

TEST_F(InternalKeyComparatorTest, Name) {
  EXPECT_STREQ(cmp_.Name(), "lldb.InternalKeyComparator");
}

TEST_F(InternalKeyComparatorTest, CompareInternalKeyOverload) {
  InternalKey a(Slice("x"), 5, kTypeValue);
  InternalKey b(Slice("x"), 5, kTypeValue);
  EXPECT_EQ(cmp_.Compare(a, b), 0);

  InternalKey c(Slice("x"), 100, kTypeValue);
  EXPECT_LT(cmp_.Compare(c, a), 0);  // higher seq first
}

TEST_F(InternalKeyComparatorTest, UserComparatorAccessor) {
  // The user comparator should be the one we passed in.
  EXPECT_EQ(cmp_.user_comparator(), BytewiseComparator());
}

// ============================================================
// InternalFilterPolicy
// ============================================================

// Helper: build an internal key string from a user key.
static std::string MakeInternalKey(const Slice& user_key) {
  std::string result;
  AppendInternalKey(&result, ParsedInternalKey(user_key, 1, kTypeValue));
  return result;
}

class InternalFilterPolicyTest : public ::testing::Test {
 protected:
  static constexpr int kBitsPerKey = 10;

  void SetUp() override {
    bloom_.reset(const_cast<FilterPolicy*>(NewBloomFilterPolicy(kBitsPerKey)));
    internal_.reset(new InternalFilterPolicy(bloom_.get()));
  }

  std::unique_ptr<FilterPolicy> bloom_;
  std::unique_ptr<InternalFilterPolicy> internal_;
};

TEST_F(InternalFilterPolicyTest, NameDelegates) {
  EXPECT_STREQ(internal_->Name(), bloom_->Name());
}

TEST_F(InternalFilterPolicyTest, CreateFilterExtractsUserKeys) {
  // Create internal keys.
  std::string ik1 = MakeInternalKey(Slice("alpha"));
  std::string ik2 = MakeInternalKey(Slice("beta"));
  std::string ik3 = MakeInternalKey(Slice("gamma"));

  Slice keys[] = {Slice(ik1), Slice(ik2), Slice(ik3)};
  std::string filter;
  internal_->CreateFilter(keys, 3, &filter);
  EXPECT_FALSE(filter.empty());
}

TEST_F(InternalFilterPolicyTest, KeyMayMatch) {
  // Create a filter for a set of internal keys.
  std::string ik1 = MakeInternalKey(Slice("hello"));
  std::string ik2 = MakeInternalKey(Slice("world"));

  Slice keys[] = {Slice(ik1), Slice(ik2)};
  std::string filter;
  internal_->CreateFilter(keys, 2, &filter);

  // An internal key with user_key "hello" should match.
  std::string match_ik = MakeInternalKey(Slice("hello"));
  EXPECT_TRUE(internal_->KeyMayMatch(Slice(match_ik), Slice(filter)));

  // An internal key with user_key "missing" should not match
  // (with high probability — Bloom filter).
  std::string miss_ik = MakeInternalKey(Slice("missing"));
  EXPECT_FALSE(internal_->KeyMayMatch(Slice(miss_ik), Slice(filter)));
}

// ============================================================
// LookupKey
// ============================================================

TEST(LookupKeyTest, ShortKeyUsesInternalBuffer) {
  LookupKey lk(Slice("short"), 100);
  Slice mk = lk.memtable_key();
  EXPECT_FALSE(mk.empty());

  // memtable_key format: varint32(length) + user_key + tag(8 bytes)
  // Length of the memtable key should be > user_key + 8.
  EXPECT_GT(mk.size(), 5u + 8u);
}

TEST(LookupKeyTest, MemtableKeyAndInternalKeyConsistency) {
  LookupKey lk(Slice("test"), 42);

  Slice mk = lk.memtable_key();
  Slice ik = lk.internal_key();
  Slice uk = lk.user_key();

  EXPECT_EQ(uk, Slice("test"));

  // internal_key should be the suffix of memtable_key.
  EXPECT_EQ(ik.data(), mk.data() + (mk.size() - ik.size()));

  // user_key + 8-byte tag = internal_key.
  EXPECT_EQ(ik.size(), uk.size() + 8);
}

TEST(LookupKeyTest, LongKeyHeapAllocates) {
  // A 250-byte key exceeds the 200-byte internal space_ buffer.
  std::string big(250, 'x');
  LookupKey lk(Slice(big), 1);

  EXPECT_EQ(lk.user_key(), Slice(big));
  EXPECT_EQ(lk.internal_key().size(), big.size() + 8);

  // memtable_key should include the varint length prefix.
  EXPECT_GT(lk.memtable_key().size(), big.size() + 8);
}

TEST(LookupKeyTest, ZeroLengthUserKey) {
  LookupKey lk(Slice(""), 0);

  Slice ik = lk.internal_key();
  EXPECT_EQ(ik.size(), 8u);

  Slice uk = lk.user_key();
  EXPECT_EQ(uk.size(), 0u);
}

}  // namespace
}  // namespace lldb
