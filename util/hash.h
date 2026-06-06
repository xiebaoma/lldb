// xiebaoma

#ifndef STORAGE_LLDB_UTIL_HASH_H_
#define STORAGE_LLDB_UTIL_HASH_H_

#include <cstddef>
#include <cstdint>

namespace lldb {

uint32_t Hash(const char* data, size_t n, uint32_t seed);

}  // namespace lldb

#endif  // STORAGE_LLDB_UTIL_HASH_H_
