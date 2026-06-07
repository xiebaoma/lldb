#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "include/lldb/cache.h"

namespace lldb {
namespace {

// ============================================================
// Helper utilities
// ============================================================

struct DeleterRecord {
  std::string key;
  void* value;
};

static std::vector<DeleterRecord> g_deleter_records;

static void RecordingDeleter(const Slice& key, void* value) {
  g_deleter_records.push_back({key.ToString(), value});
}

static void DeletingDeleter(const Slice& key, void* value) {
  g_deleter_records.push_back({key.ToString(), value});
  delete static_cast<int*>(value);
}

static void ClearDeleterRecords() { g_deleter_records.clear(); }

// Thread-safe deleter: only deletes, no shared state (for concurrent tests).
static void ConcurrentDeleter(const Slice& key, void* value) {
  (void)key;
  delete static_cast<int*>(value);
}

// ============================================================
// Basic Insert / Lookup / Value
// ============================================================

TEST(CacheTest, InsertAndLookup) {
  Cache* cache = NewLRUCache(1024);
  int val = 42;

  Cache::Handle* h = cache->Insert(Slice("key1"), &val, 1, RecordingDeleter);
  ASSERT_NE(h, nullptr);
  cache->Release(h);

  Cache::Handle* h2 = cache->Lookup(Slice("key1"));
  ASSERT_NE(h2, nullptr);
  EXPECT_EQ(cache->Value(h2), &val);
  cache->Release(h2);

  delete cache;
}

TEST(CacheTest, LookupMissingKey) {
  Cache* cache = NewLRUCache(1024);

  Cache::Handle* h = cache->Lookup(Slice("nonexistent"));
  EXPECT_EQ(h, nullptr);

  delete cache;
}

TEST(CacheTest, ValueOnInsertHandle) {
  Cache* cache = NewLRUCache(1024);
  int val = 99;

  Cache::Handle* h = cache->Insert(Slice("k"), &val, 1, RecordingDeleter);
  ASSERT_NE(h, nullptr);
  EXPECT_EQ(cache->Value(h), &val);
  cache->Release(h);

  delete cache;
}

// ============================================================
// Erase
// ============================================================

TEST(CacheTest, Erase) {
  Cache* cache = NewLRUCache(1024);
  int val = 7;

  Cache::Handle* h = cache->Insert(Slice("eraseme"), &val, 1, RecordingDeleter);
  cache->Release(h);
  cache->Erase(Slice("eraseme"));

  Cache::Handle* h2 = cache->Lookup(Slice("eraseme"));
  EXPECT_EQ(h2, nullptr);

  delete cache;
}

TEST(CacheTest, EraseMissingKey) {
  Cache* cache = NewLRUCache(1024);
  cache->Erase(Slice("no_such_key"));
  delete cache;
}

// ============================================================
// LRU Eviction
// ============================================================

TEST(CacheTest, LRUEvictionBoundedCharge) {
  // 16 shards, per_shard = (16 + 15) / 16 = 1.
  // Total capacity is effectively 16.
  Cache* cache = NewLRUCache(16);
  int vals[200];

  for (int i = 0; i < 200; ++i) {
    vals[i] = i;
    char key[16];
    snprintf(key, sizeof(key), "evict_%d", i);
    Cache::Handle* h =
        cache->Insert(Slice(key), &vals[i], 1, RecordingDeleter);
    ASSERT_NE(h, nullptr);
    cache->Release(h);
  }

  // With 200 entries and per-shard capacity 1, most were evicted.
  EXPECT_LE(cache->TotalCharge(), 16u);

  delete cache;
}

TEST(CacheTest, RecentEntriesSurvive) {
  // Insert entries with large capacity; all should survive.
  Cache* cache = NewLRUCache(1024);
  int vals[50];

  for (int i = 0; i < 50; ++i) {
    vals[i] = i;
    char key[16];
    snprintf(key, sizeof(key), "keep_%d", i);
    Cache::Handle* h =
        cache->Insert(Slice(key), &vals[i], 1, RecordingDeleter);
    ASSERT_NE(h, nullptr);
    cache->Release(h);
  }

  EXPECT_EQ(cache->TotalCharge(), 50u);

  // All entries should be findable.
  for (int i = 0; i < 50; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "keep_%d", i);
    Cache::Handle* h = cache->Lookup(Slice(key));
    ASSERT_NE(h, nullptr) << "Missing key: " << key;
    EXPECT_EQ(cache->Value(h), &vals[i]);
    cache->Release(h);
  }

  delete cache;
}

// ============================================================
// Deleter invocation
// ============================================================

TEST(CacheTest, DeleterCalledOnEviction) {
  ClearDeleterRecords();

  // per_shard = (16 + 15) / 16 = 1. With 32 entries, roughly half the shards
  // will see a second insertion, causing the first entry in that shard to be
  // evicted and its deleter to fire.
  Cache* cache = NewLRUCache(16);
  int* vals[32];
  for (int i = 0; i < 32; ++i) {
    vals[i] = new int(i);
    char key[8];
    snprintf(key, sizeof(key), "dk%d", i);
    Cache::Handle* h = cache->Insert(Slice(key), vals[i], 1, DeletingDeleter);
    ASSERT_NE(h, nullptr);
    cache->Release(h);
  }

  // At least some deleters should have been called.
  EXPECT_GT(g_deleter_records.size(), 0u);

  // The deleter records should contain the correct keys and values.
  for (const auto& rec : g_deleter_records) {
    // The value pointer should be one of our allocated ints.
    bool found = false;
    for (int i = 0; i < 32; ++i) {
      if (rec.value == vals[i]) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Deleter called with unknown value: " << rec.value;
  }

  delete cache;

  // Clean up any vals that weren't deleted by the cache.
  for (int i = 0; i < 32; ++i) {
    bool was_deleted = false;
    for (const auto& rec : g_deleter_records) {
      if (rec.value == vals[i]) {
        was_deleted = true;
        break;
      }
    }
    if (!was_deleted) {
      delete vals[i];
    }
  }
}

TEST(CacheTest, DeleterCalledOnDestructor) {
  ClearDeleterRecords();
  int* v = new int(88);

  {
    Cache* cache = NewLRUCache(10);
    Cache::Handle* h = cache->Insert(Slice("x"), v, 1, DeletingDeleter);
    cache->Release(h);
    delete cache;
  }

  ASSERT_GE(g_deleter_records.size(), 1u);
  EXPECT_EQ(g_deleter_records[0].key, "x");
  EXPECT_EQ(g_deleter_records[0].value, v);
}

// ============================================================
// TotalCharge
// ============================================================

TEST(CacheTest, TotalCharge) {
  Cache* cache = NewLRUCache(1024);
  int v1 = 1, v2 = 2;

  EXPECT_EQ(cache->TotalCharge(), 0u);

  Cache::Handle* h1 = cache->Insert(Slice("a"), &v1, 100, RecordingDeleter);
  cache->Release(h1);
  EXPECT_EQ(cache->TotalCharge(), 100u);

  Cache::Handle* h2 = cache->Insert(Slice("b"), &v2, 200, RecordingDeleter);
  cache->Release(h2);
  EXPECT_EQ(cache->TotalCharge(), 300u);

  cache->Erase(Slice("a"));
  EXPECT_LE(cache->TotalCharge(), 200u);

  delete cache;
}

// ============================================================
// NewId
// ============================================================

TEST(CacheTest, NewIdMonotonic) {
  Cache* cache = NewLRUCache(1024);

  uint64_t id1 = cache->NewId();
  uint64_t id2 = cache->NewId();
  uint64_t id3 = cache->NewId();

  EXPECT_LT(id1, id2);
  EXPECT_LT(id2, id3);

  delete cache;
}

// ============================================================
// Prune
// ============================================================

TEST(CacheTest, PruneRemovesUnreferencedEntries) {
  Cache* cache = NewLRUCache(1024);
  int v1 = 1, v2 = 2;

  Cache::Handle* h1 = cache->Insert(Slice("p1"), &v1, 10, RecordingDeleter);
  cache->Release(h1);
  Cache::Handle* h2 = cache->Insert(Slice("p2"), &v2, 20, RecordingDeleter);
  cache->Release(h2);

  EXPECT_EQ(cache->TotalCharge(), 30u);

  cache->Prune();

  EXPECT_EQ(cache->TotalCharge(), 0u);
  EXPECT_EQ(cache->Lookup(Slice("p1")), nullptr);
  EXPECT_EQ(cache->Lookup(Slice("p2")), nullptr);

  delete cache;
}

TEST(CacheTest, PruneKeepsReferencedEntries) {
  Cache* cache = NewLRUCache(1024);
  int v1 = 1, v2 = 2;

  Cache::Handle* h = cache->Insert(Slice("keep"), &v1, 10, RecordingDeleter);
  // Do NOT release h — keeping it referenced.

  Cache::Handle* h2 = cache->Insert(Slice("drop"), &v2, 20, RecordingDeleter);
  cache->Release(h2);

  cache->Prune();

  // "drop" should be pruned (unreferenced).
  EXPECT_EQ(cache->Lookup(Slice("drop")), nullptr);

  // "keep" should remain (still referenced by h).
  Cache::Handle* hk = cache->Lookup(Slice("keep"));
  ASSERT_NE(hk, nullptr);
  cache->Release(hk);

  cache->Release(h);
  delete cache;
}

// ============================================================
// Zero capacity
// ============================================================

TEST(CacheTest, ZeroCapacityNoCaching) {
  Cache* cache = NewLRUCache(0);
  int val = 5;

  Cache::Handle* h = cache->Insert(Slice("key"), &val, 1, RecordingDeleter);
  ASSERT_NE(h, nullptr);
  EXPECT_EQ(cache->Value(h), &val);

  // With zero capacity, the entry is not cached.
  EXPECT_EQ(cache->Lookup(Slice("key")), nullptr);

  cache->Release(h);
  delete cache;
}

// ============================================================
// Update existing key
// ============================================================

TEST(CacheTest, ReInsertSameKey) {
  Cache* cache = NewLRUCache(1024);
  int v1 = 10, v2 = 20;

  Cache::Handle* h1 = cache->Insert(Slice("dup"), &v1, 1, RecordingDeleter);
  cache->Release(h1);

  Cache::Handle* h2 = cache->Insert(Slice("dup"), &v2, 1, RecordingDeleter);
  cache->Release(h2);

  Cache::Handle* h = cache->Lookup(Slice("dup"));
  ASSERT_NE(h, nullptr);
  EXPECT_EQ(cache->Value(h), &v2);
  cache->Release(h);

  delete cache;
}

// ============================================================
// Multiple concurrent handles
// ============================================================

TEST(CacheTest, MultipleHandlesSameKey) {
  Cache* cache = NewLRUCache(1024);
  int val = 42;

  Cache::Handle* h1 = cache->Insert(Slice("shared"), &val, 1, RecordingDeleter);
  cache->Release(h1);

  Cache::Handle* hA = cache->Lookup(Slice("shared"));
  ASSERT_NE(hA, nullptr);
  Cache::Handle* hB = cache->Lookup(Slice("shared"));
  ASSERT_NE(hB, nullptr);

  EXPECT_EQ(cache->Value(hA), &val);
  EXPECT_EQ(cache->Value(hB), &val);

  // Release one; the entry should not be deleted yet.
  cache->Release(hA);
  Cache::Handle* stillThere = cache->Lookup(Slice("shared"));
  ASSERT_NE(stillThere, nullptr);
  cache->Release(stillThere);

  cache->Release(hB);
  delete cache;
}

// ============================================================
// Stress: large number of entries
// ============================================================

TEST(CacheTest, ManyEntries) {
  constexpr int kN = 1000;
  // Use 2x capacity to absorb uneven shard distribution.
  Cache* cache = NewLRUCache(kN * 2);
  int* vals = new int[kN];

  for (int i = 0; i < kN; ++i) {
    vals[i] = i;
    char key[32];
    snprintf(key, sizeof(key), "stress_key_%d", i);
    Cache::Handle* h =
        cache->Insert(Slice(key), &vals[i], 1, RecordingDeleter);
    ASSERT_NE(h, nullptr);
    cache->Release(h);
  }

  for (int i = 0; i < 100; ++i) {
    int idx = (i * 73 + 19) % kN;
    char key[32];
    snprintf(key, sizeof(key), "stress_key_%d", idx);
    Cache::Handle* h = cache->Lookup(Slice(key));
    ASSERT_NE(h, nullptr) << "Missing key: " << key;
    EXPECT_EQ(static_cast<int*>(cache->Value(h)), &vals[idx]);
    cache->Release(h);
  }

  delete[] vals;
  delete cache;
}

// ============================================================
// Thread safety
// ============================================================

TEST(CacheTest, ConcurrentInsertLookup) {
  constexpr int kThreads = 8;
  constexpr int kOpsPerThread = 2000;

  Cache* cache = NewLRUCache(4096);
  std::atomic<bool> ok{true};

  auto worker = [&](int tid) {
    for (int i = 0; i < kOpsPerThread; ++i) {
      char key[32];
      snprintf(key, sizeof(key), "t%d_op%d", tid, i);

      int* val = new int(tid * 10000 + i);
      Cache::Handle* h =
          cache->Insert(Slice(key), val, 1, ConcurrentDeleter);
      if (h == nullptr) {
        ok.store(false);
        delete val;
        return;
      }
      cache->Release(h);

      if (i > 0) {
        snprintf(key, sizeof(key), "t%d_op%d", tid, i - 1);
        Cache::Handle* lh = cache->Lookup(Slice(key));
        if (lh != nullptr) {
          cache->Release(lh);
        }
      }
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(worker, t);
  }
  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(ok.load());
  delete cache;
}

TEST(CacheTest, ConcurrentNewId) {
  constexpr int kThreads = 4;
  constexpr int kIdsPerThread = 500;

  Cache* cache = NewLRUCache(1024);
  std::vector<uint64_t> ids[kThreads];

  auto worker = [&](int tid) {
    for (int i = 0; i < kIdsPerThread; ++i) {
      ids[tid].push_back(cache->NewId());
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(worker, t);
  }
  for (auto& t : threads) {
    t.join();
  }

  std::set<uint64_t> all_ids;
  for (int t = 0; t < kThreads; ++t) {
    for (uint64_t id : ids[t]) {
      EXPECT_TRUE(all_ids.insert(id).second) << "Duplicate id: " << id;
    }
  }
  EXPECT_EQ(all_ids.size(), kThreads * kIdsPerThread);

  delete cache;
}

// ============================================================
// Mixed operations
// ============================================================

TEST(CacheTest, MixedInsertEraseLookup) {
  Cache* cache = NewLRUCache(1024);
  int vals[10];

  for (int i = 0; i < 10; ++i) {
    vals[i] = i;
    char key[16];
    snprintf(key, sizeof(key), "mix_%d", i);
    Cache::Handle* h =
        cache->Insert(Slice(key), &vals[i], 1, RecordingDeleter);
    ASSERT_NE(h, nullptr);
    cache->Release(h);
  }

  // Erase half.
  for (int i = 0; i < 5; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mix_%d", i);
    cache->Erase(Slice(key));
  }

  // Erased entries should be gone.
  for (int i = 0; i < 5; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mix_%d", i);
    EXPECT_EQ(cache->Lookup(Slice(key)), nullptr);
  }

  // Non-erased entries should still be present.
  for (int i = 5; i < 10; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "mix_%d", i);
    Cache::Handle* h = cache->Lookup(Slice(key));
    ASSERT_NE(h, nullptr) << "Key " << key << " unexpectedly missing";
    cache->Release(h);
  }

  delete cache;
}

// ============================================================
// Empty key
// ============================================================

TEST(CacheTest, EmptyKey) {
  Cache* cache = NewLRUCache(1024);
  int val = 1;

  Cache::Handle* h = cache->Insert(Slice(""), &val, 1, RecordingDeleter);
  ASSERT_NE(h, nullptr);
  cache->Release(h);

  Cache::Handle* h2 = cache->Lookup(Slice(""));
  ASSERT_NE(h2, nullptr);
  EXPECT_EQ(cache->Value(h2), &val);
  cache->Release(h2);

  cache->Erase(Slice(""));
  EXPECT_EQ(cache->Lookup(Slice("")), nullptr);

  delete cache;
}

}  // namespace
}  // namespace lldb
