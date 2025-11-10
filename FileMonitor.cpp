#include "FileMonitor.h"
#include "Checksum.h"
#include "FileUtils.h"
#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>

namespace DirShare {

FileMonitor::FileMonitor(const std::string& directory_path,
                         FileChangeTracker& change_tracker,
                         bool fail_silently)
  : directory_path_(directory_path)
  , fail_silently_(fail_silently)
  , change_tracker_(change_tracker)
{
  // Verify directory exists
  if (!is_directory(directory_path_)) {
    if (!fail_silently) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("ERROR: %N:%l: Directory does not exist: %C\n"),
                 directory_path_.c_str()));
    }
  }
}

FileMonitor::~FileMonitor()
{
}

bool FileMonitor::scan_for_changes(
  std::vector<std::string>& created_files,
  std::vector<std::string>& modified_files,
  std::vector<std::string>& deleted_files)
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);

  created_files.clear();
  modified_files.clear();
  deleted_files.clear();

  // Get current list of files
  std::vector<std::string> current_files;
  if (!list_directory_files(directory_path_, current_files)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Failed to list directory: %C\n"),
               directory_path_.c_str()));
    return false;
  }

  // Build current state map
  std::map<std::string, FileState> current_state;
  for (size_t i = 0; i < current_files.size(); ++i) {
    const std::string& filename = current_files[i];
    std::string full_path = build_path(filename);

    FileState state;
    if (!get_file_size(full_path, state.size)) {
      continue; // Skip files we can't read
    }

    if (!get_modification_time(full_path, state.timestamp_sec, state.timestamp_nsec)) {
      continue;
    }

    if (!calculate_file_checksum(full_path, state.checksum)) {
      continue;
    }

    current_state[filename] = state;
  }

  // Detect created and modified files
  for (std::map<std::string, FileState>::const_iterator it = current_state.begin();
       it != current_state.end(); ++it) {
    const std::string& filename = it->first;
    const FileState& current = it->second;

    // SC-011: Check if notifications are suppressed for this file
    // If true, this change came from a remote source and should NOT be republished
    if (change_tracker_.is_suppressed(filename)) {
      ACE_DEBUG((LM_DEBUG,
                ACE_TEXT("FileMonitor: Skipping suppressed file '%C' (remote update in progress)\n"),
                filename.c_str()));
      continue;  // Skip this file - it's being updated from remote
    }

    std::map<std::string, FileState>::const_iterator prev_it = previous_state_.find(filename);
    if (prev_it == previous_state_.end()) {
      // File doesn't exist in previous state - CREATED
      created_files.push_back(filename);
    } else {
      const FileState& previous = prev_it->second;
      // Check if file was modified (compare size, timestamp, or checksum)
      if (current.size != previous.size ||
          current.timestamp_sec != previous.timestamp_sec ||
          current.timestamp_nsec != previous.timestamp_nsec ||
          current.checksum != previous.checksum) {
        modified_files.push_back(filename);
      }
    }
  }

  // Detect deleted files
  for (std::map<std::string, FileState>::const_iterator it = previous_state_.begin();
       it != previous_state_.end(); ++it) {
    const std::string& filename = it->first;
    if (current_state.find(filename) == current_state.end()) {
      // File existed in previous state but not in current - DELETED
      deleted_files.push_back(filename);
    }
  }

  // Update previous state for next scan
  previous_state_ = current_state;

  return true;
}

std::vector<FileMetadata> FileMonitor::get_all_files()
{
  ACE_Guard<ACE_Thread_Mutex> guard(mutex_);

  std::vector<FileMetadata> result;
  std::vector<std::string> files;

  if (!list_directory_files(directory_path_, files)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("ERROR: %N:%l: Failed to list directory: %C\n"),
               directory_path_.c_str()));
    return result;
  }

  for (size_t i = 0; i < files.size(); ++i) {
    FileMetadata metadata;
    if (get_file_metadata(files[i], metadata)) {
      result.push_back(metadata);
    }
  }

  return result;
}

bool FileMonitor::get_file_metadata(const std::string& filename, FileMetadata& metadata)
{
  std::string full_path = build_path(filename);

  if (!file_exists(full_path)) {
    return false;
  }

  metadata.filename = filename.c_str();

  if (!get_file_size(full_path, metadata.size)) {
    return false;
  }

  unsigned long long timestamp_sec;
  unsigned long timestamp_nsec;
  if (!get_modification_time(full_path, timestamp_sec, timestamp_nsec)) {
    return false;
  }
  metadata.timestamp_sec = timestamp_sec;
  metadata.timestamp_nsec = static_cast<CORBA::ULong>(timestamp_nsec);

  unsigned long checksum;
  if (!calculate_file_checksum(full_path, checksum)) {
    return false;
  }
  metadata.checksum = static_cast<CORBA::ULong>(checksum);

  return true;
}

std::string FileMonitor::build_path(const std::string& filename) const
{
  // Simple path concatenation (assumes directory_path_ ends without separator)
  std::string path = directory_path_;
  if (!path.empty() && path[path.length() - 1] != '/' && path[path.length() - 1] != '\\') {
    path += '/';
  }
  path += filename;
  return path;
}

bool FileMonitor::calculate_file_checksum(const std::string& full_path, unsigned long& checksum)
{
  return calculate_file_crc32(full_path.c_str(), checksum);
}

bool FileMonitor::get_modification_time(const std::string& full_path,
                                       unsigned long long& sec,
                                       unsigned long& nsec)
{
  return get_file_mtime(full_path, sec, nsec);
}

} // namespace DirShare
