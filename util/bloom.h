// xiebaoma

#ifndef STORAGE_LLDB_UTIL_BLOOM_H_
#define STORAGE_LLDB_UTIL_BLOOM_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "lldb/filter_policy.h"
#include "lldb/slice.h"

namespace lldb {

class BloomFilterPolicy : public FilterPolicy {
 public:
  explicit BloomFilterPolicy(int bits_per_key);

  const char* Name() const override;
  void CreateFilter(const Slice* keys, int n, std::string* dst) const override;
  bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) const override;

 private:
  size_t bits_per_key_;
  size_t k_;
};

}  // namespace lldb

#endif  // STORAGE_LLDB_UTIL_BLOOM_H_
