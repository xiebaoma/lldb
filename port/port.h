// xiebaoma

#ifndef STORAGE_LLDB_PORT_PORT_H_
#define STORAGE_LLDB_PORT_PORT_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <condition_variable>

namespace lldb {
namespace port {

// Returns the CRC32C checksum of buf[0,size-1] appended to crc.
// Returns 0 if hardware-accelerated CRC32C is unavailable on this platform,
// which causes the caller to fall back to the software implementation.
inline uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size) {
  (void)crc;
  (void)buf;
  (void)size;
  return 0;  // No hardware acceleration on this platform.
}

class Mutex{
  public:
    Mutex() = default;
    ~Mutex() = default;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void Lock(){
      mu_.lock();
    }

    void Unlock(){
      mu_.unlock();
    }

    // TODO 
    void AssertHeld(){
      return;
    }

  private:
    friend class CondVar;
    std::mutex mu_;
};

class CondVar {
  public:
    explicit CondVar(Mutex* mu) : mu_(mu) { assert(mu != nullptr); }
    ~CondVar() = default;

    CondVar(const CondVar&) = delete;
    CondVar& operator=(const CondVar&) = delete;

    void Wait() {
      std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
      cv_.wait(lock);
      lock.release();
    }
    void Signal() { cv_.notify_one(); }
    void SignalAll() { cv_.notify_all(); }

 private:
  std::condition_variable cv_;
  Mutex* const mu_;
};

}  // namespace port
}  // namespace lldb

#endif  // STORAGE_LLDB_PORT_PORT_H_
