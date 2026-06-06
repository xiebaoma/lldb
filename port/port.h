// xiebaoma

#ifndef STORAGE_LLDB_PORT_PORT_H_
#define STORAGE_LLDB_PORT_PORT_H_

#include <cstddef>
#include <cstdint>

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

}  // namespace port
}  // namespace lldb

#endif  // STORAGE_LLDB_PORT_PORT_H_
