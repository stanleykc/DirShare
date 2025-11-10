#ifndef DIRSHARE_FILEUTILS_H
#define DIRSHARE_FILEUTILS_H

#include <string>
#include <vector>

namespace DirShare {

/**
 * File I/O utility functions
 * Provides cross-platform file operations with timestamp preservation
 */

/**
 * Read entire file into buffer
 * @param file_path Path to file
 * @param data Output: file contents
 * @return true if successful, false on error
 */
bool read_file(const std::string& file_path, std::vector<unsigned char>& data);

/**
 * Write buffer to file
 * @param file_path Path to file
 * @param data File contents to write
 * @param size Size of data in bytes
 * @return true if successful, false on error
 */
bool write_file(const std::string& file_path,
                const unsigned char* data,
                size_t size);

/**
 * Get file size
 * @param file_path Path to file
 * @param size Output: file size in bytes
 * @return true if successful, false if file doesn't exist or error
 */
bool get_file_size(const std::string& file_path, unsigned long long& size);

/**
 * Get file modification time
 * @param file_path Path to file
 * @param sec Output: seconds since epoch
 * @param nsec Output: nanoseconds part
 * @return true if successful, false on error
 */
bool get_file_mtime(const std::string& file_path,
                    unsigned long long& sec,
                    unsigned long& nsec);

/**
 * Set file modification time
 * Preserves timestamps when transferring files
 * @param file_path Path to file
 * @param sec Seconds since epoch
 * @param nsec Nanoseconds part
 * @return true if successful, false on error
 */
bool set_file_mtime(const std::string& file_path,
                    unsigned long long sec,
                    unsigned long nsec);

/**
 * Check if file exists
 * @param file_path Path to file
 * @return true if file exists, false otherwise
 */
bool file_exists(const std::string& file_path);

/**
 * Check if path is a directory
 * @param path Path to check
 * @return true if directory exists, false otherwise
 */
bool is_directory(const std::string& path);

/**
 * Delete file
 * @param file_path Path to file
 * @return true if successful, false on error
 */
bool delete_file(const std::string& file_path);

/**
 * List all regular files in directory (non-recursive)
 * Ignores subdirectories, symbolic links, and special files
 * @param directory_path Path to directory
 * @param files Output: list of filenames (relative to directory)
 * @return true if successful, false on error
 */
bool list_directory_files(const std::string& directory_path,
                         std::vector<std::string>& files);

/**
 * Validate filename for security
 * Rejects path traversal attempts (../, ..\), absolute paths
 * @param filename Filename to validate
 * @return true if safe, false if potentially dangerous
 */
bool is_valid_filename(const std::string& filename);

} // namespace DirShare

#endif // DIRSHARE_FILEUTILS_H
