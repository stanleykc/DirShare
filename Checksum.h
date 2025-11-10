#ifndef DIRSHARE_CHECKSUM_H
#define DIRSHARE_CHECKSUM_H

#include <cstddef>
#include <stdint.h>

namespace DirShare {

/**
 * CRC32 checksum calculation utilities
 * Provides fast integrity verification for file transfers
 */

/**
 * Calculate CRC32 checksum for a buffer
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return CRC32 checksum (32-bit)
 */
unsigned long calculate_crc32(const unsigned char* data, size_t length);

/**
 * Calculate CRC32 checksum incrementally
 * Allows computing checksum in chunks without loading entire file
 * @param data Pointer to data chunk
 * @param length Length of data chunk
 * @param previous_crc Previous CRC value (use 0xFFFFFFFF for first chunk)
 * @return Updated CRC32 value
 */
unsigned long calculate_crc32_incremental(const unsigned char* data,
                                         size_t length,
                                         unsigned long previous_crc);

/**
 * Finalize incremental CRC32 calculation
 * @param crc CRC value from incremental calculations
 * @return Final CRC32 checksum
 */
unsigned long finalize_crc32(unsigned long crc);

/**
 * Calculate CRC32 checksum for a file
 * @param file_path Path to file
 * @param checksum Output: calculated CRC32 checksum
 * @return true if successful, false on error (file not found, read error)
 */
bool calculate_file_crc32(const char* file_path, unsigned long& checksum);

/**
 * Convenience wrapper: Compute checksum for buffer
 * @param data Pointer to data buffer (as uint8_t*)
 * @param length Length of data in bytes
 * @return CRC32 checksum (32-bit as uint32_t)
 */
inline uint32_t compute_checksum(const uint8_t* data, size_t length) {
  return static_cast<uint32_t>(calculate_crc32(data, length));
}

} // namespace DirShare

#endif // DIRSHARE_CHECKSUM_H
