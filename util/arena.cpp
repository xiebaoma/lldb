//xiebaoma

#include "util/arena.h"

namespace lldb {

static const int kBlockSize = 4096;

Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() {
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
}

char* Arena::AllocateFallback(size_t bytes) {
  if (bytes > kBlockSize / 4) {
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;

  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

char* Arena::AllocateAligned(size_t bytes) {
  std::lock_guard<std::mutex> lock(mu_);
  const size_t align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  static_assert((align & (align - 1)) == 0);
  uintptr_t ptr = reinterpret_cast<uintptr_t>(alloc_ptr_);
  size_t misalign = ptr & (align - 1);
  size_t padding = misalign ? (align - misalign) : 0;

  if (bytes + padding > alloc_bytes_remaining_) {
    return AllocateFallback(bytes);
  }

  char* result = alloc_ptr_ + padding;
  alloc_ptr_ += padding + bytes;
  alloc_bytes_remaining_ -= padding + bytes;
  return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes) {
  char* result = new char[block_bytes];
  blocks_.push_back(result);
  memory_usage_.fetch_add(block_bytes + sizeof(char*),
                          std::memory_order_relaxed);
  return result;
}

}  // namespace lldb
