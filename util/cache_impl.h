// xiebaoma
//
// Internal implementation of the LRU cache.  These types are not part of the
// public API; they are declared here so that cache.cpp can implement them.

#ifndef STORAGE_LLDB_UTIL_CACHE_IMPL_H_
#define STORAGE_LLDB_UTIL_CACHE_IMPL_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "lldb/cache.h"
#include "port/port.h"
#include "util/mutexlock.h"

namespace lldb {

// An entry in the LRU cache.  Inherits from Cache::Handle so that
// static_cast (rather than reinterpret_cast) can be used for safe
// pointer conversion between the public and internal handle types.
struct LRUHandle : public Cache::Handle {
  void* value;
  void (*deleter)(const Slice&, void* value);
  LRUHandle* next_hash;
  LRUHandle* next;
  LRUHandle* prev;
  size_t charge;
  size_t key_length;
  bool in_cache;
  uint32_t refs;
  uint32_t hash;
  char key_data[1];  // Beginning of key

  Slice key() const {
    assert(next != this);
    return Slice(key_data, key_length);
  }
};

// Hash table for fast cache lookups.  Uses chaining for collision resolution.
class HandleTable {
 public:
  HandleTable();
  ~HandleTable();

  LRUHandle* Lookup(const Slice& key, uint32_t hash);
  LRUHandle* Insert(LRUHandle* h);
  LRUHandle* Remove(const Slice& key, uint32_t hash);

 private:
  uint32_t length_;
  uint32_t elems_;
  LRUHandle** list_;

  LRUHandle** FindPointer(const Slice& key, uint32_t hash);
  void Resize();
};

// A single shard of the sharded LRU cache.
class LRUCache {
 public:
  LRUCache();
  ~LRUCache();

  void SetCapacity(size_t capacity) { capacity_ = capacity; }

  Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
                        size_t charge,
                        void (*deleter)(const Slice& key, void* value));
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);
  void Release(Cache::Handle* handle);
  void Erase(const Slice& key, uint32_t hash);
  void Prune();

  size_t TotalCharge() const;

 private:
  void LRU_Remove(LRUHandle* e);
  void LRU_Append(LRUHandle* list, LRUHandle* e);
  void Ref(LRUHandle* e);
  void Unref(LRUHandle* e);
  bool FinishErase(LRUHandle* e);

  size_t capacity_;
  mutable port::Mutex mutex_;
  size_t usage_;
  LRUHandle lru_;
  LRUHandle in_use_;
  HandleTable table_;
};

// The top-level cache implementation.  Shards keys across multiple LRUCache
// instances to reduce lock contention.
class ShardedLRUCache : public Cache {
 public:
  explicit ShardedLRUCache(size_t capacity);
  ~ShardedLRUCache() override;

  Handle* Insert(const Slice& key, void* value, size_t charge,
                 void (*deleter)(const Slice& key, void* value)) override;
  Handle* Lookup(const Slice& key) override;
  void Release(Handle* handle) override;
  void Erase(const Slice& key) override;
  void* Value(Handle* handle) override;
  uint64_t NewId() override;
  void Prune() override;
  size_t TotalCharge() const override;

 private:
  static constexpr int kNumShardBits = 4;
  static constexpr int kNumShards = 1 << kNumShardBits;

  static uint32_t HashSlice(const Slice& s);
  static uint32_t Shard(uint32_t hash);

  LRUCache shard_[kNumShards];
  port::Mutex id_mutex_;
  uint64_t last_id_;
};

}  // namespace lldb

#endif  // STORAGE_LLDB_UTIL_CACHE_IMPL_H_
