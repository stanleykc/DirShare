#include "FileUtils.h"
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Dirent.h>
#include <fstream>
#include <sys/types.h>
#include <utime.h>

namespace DirShare {

bool read_file(const std::string& file_path, std::vector<unsigned char>& data)
{
  std::ifstream file(file_path.c_str(), std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  data.resize(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
    return false;
  }

  return true;
}

bool write_file(const std::string& file_path,
                const unsigned char* data,
                size_t size)
{
  std::ofstream file(file_path.c_str(), std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  if (!file.write(reinterpret_cast<const char*>(data), size)) {
    return false;
  }

  return true;
}

bool get_file_size(const std::string& file_path, unsigned long long& size)
{
  ACE_stat st;
  if (ACE_OS::stat(file_path.c_str(), &st) != 0) {
    return false;
  }

  size = static_cast<unsigned long long>(st.st_size);
  return true;
}

bool get_file_mtime(const std::string& file_path,
                    unsigned long long& sec,
                    unsigned long& nsec)
{
  ACE_stat st;
  if (ACE_OS::stat(file_path.c_str(), &st) != 0) {
    return false;
  }

  sec = static_cast<unsigned long long>(st.st_mtime);
  // Note: ACE_stat may not have nanosecond precision on all platforms
  // For now, set nsec to 0 (second-level precision is sufficient)
  nsec = 0;
  return true;
}

bool set_file_mtime(const std::string& file_path,
                    unsigned long long sec,
                    unsigned long nsec)
{
  // Get current access time
  ACE_stat st;
  if (ACE_OS::stat(file_path.c_str(), &st) != 0) {
    return false;
  }

  // Set modification time using standard utime
  struct utimbuf times;
  times.actime = st.st_atime;
  times.modtime = static_cast<time_t>(sec);

  return ::utime(file_path.c_str(), &times) == 0;
}

bool file_exists(const std::string& file_path)
{
  ACE_stat st;
  if (ACE_OS::stat(file_path.c_str(), &st) != 0) {
    return false;
  }
  // Check if it's a regular file (not directory or special file)
  return (st.st_mode & S_IFREG) != 0;
}

bool is_directory(const std::string& path)
{
  ACE_stat st;
  if (ACE_OS::stat(path.c_str(), &st) != 0) {
    return false;
  }
  return (st.st_mode & S_IFDIR) != 0;
}

bool delete_file(const std::string& file_path)
{
  return ACE_OS::unlink(file_path.c_str()) == 0;
}

bool list_directory_files(const std::string& directory_path,
                         std::vector<std::string>& files)
{
  files.clear();

  // Check if directory exists
  if (!is_directory(directory_path)) {
    return false;
  }

  ACE_Dirent dir(directory_path.c_str());

  for (ACE_DIRENT* entry = dir.read(); entry != 0; entry = dir.read()) {
    std::string filename = entry->d_name;

    // Skip . and ..
    if (filename == "." || filename == "..") {
      continue;
    }

    // Build full path
    std::string full_path = directory_path;
    if (!full_path.empty() && full_path[full_path.length() - 1] != '/' &&
        full_path[full_path.length() - 1] != '\\') {
      full_path += '/';
    }
    full_path += filename;

    // Check if it's a regular file (skip directories, symlinks, special files)
    ACE_stat st;
    if (ACE_OS::lstat(full_path.c_str(), &st) == 0) {
      // Only include regular files (not directories or symlinks per spec)
      if ((st.st_mode & S_IFREG) != 0) {
        // Validate filename for security
        if (is_valid_filename(filename)) {
          files.push_back(filename);
        }
      }
    }
  }

  return true;
}

bool is_valid_filename(const std::string& filename)
{
  // Reject empty filenames
  if (filename.empty()) {
    return false;
  }

  // Reject path traversal attempts
  if (filename.find("../") != std::string::npos ||
      filename.find("..\\") != std::string::npos ||
      filename.find("/..") != std::string::npos ||
      filename.find("\\..") != std::string::npos) {
    return false;
  }

  // Reject absolute paths
  if (!filename.empty() && (filename[0] == '/' || filename[0] == '\\')) {
    return false;
  }

  // Reject Windows drive letters (C:, D:, etc.)
  if (filename.length() >= 2 && filename[1] == ':') {
    return false;
  }

  // Reject filenames with path separators (must be single-level)
  if (filename.find('/') != std::string::npos ||
      filename.find('\\') != std::string::npos) {
    return false;
  }

  return true;
}

} // namespace DirShare
