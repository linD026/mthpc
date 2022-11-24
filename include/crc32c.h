#ifndef __MTHPC_CRC32C_H__
#define __MTHPC_CRC32C_H__

#include <stdint.h>

inline uint32_t crc32c_u32(const uint32_t crc, const uint32_t v)
{
#if defined(__x86_64__)
  return _mm_crc32_u32(crc, v);
#elif defined(__aarch64__)
  return __crc32cw(crc, v);
#endif
}

inline uint32_t crc32c_u64(const uint32_t crc, const uint64_t v)
{
#if defined(__x86_64__)
  return (uint32_t)_mm_crc32_u64(crc, v);
#elif defined(__aarch64__)
  return (uint32_t)__crc32cd(crc, v);
#endif
}

#endif /* __MTHPC_CRC32C_H__ */
