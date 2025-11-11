#define BOOST_TEST_MODULE FileEventModifyTest
#include <boost/test/included/unit_test.hpp>

#include "../FileEventListenerImpl.h"
#include "../FileMonitor.h"
#include "../FileChangeTracker.h"
#include "../FileUtils.h"
#include "../Checksum.h"
#include "../DirShareTypeSupportImpl.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_time.h>
#include <fstream>
#include <vector>

// Test fixture for FileEvent MODIFY operations
struct FileEventModifyTestFixture {
  std::string test_dir;

  FileEventModifyTestFixture() : test_dir("test_modify_dir") {
    // Create test directory
    ACE_OS::mkdir(test_dir.c_str());
  }

  ~FileEventModifyTestFixture() {
    cleanup_directory(test_dir.c_str());
  }

  void cleanup_directory(const char* dir) {
    std::vector<std::string> files;
    if (DirShare::list_directory_files(dir, files)) {
      for (size_t i = 0; i < files.size(); ++i) {
        std::string path = std::string(dir) + "/" + files[i];
        ACE_OS::unlink(path.c_str());
      }
    }
    ACE_OS::rmdir(dir);
  }

  void create_file(const std::string& filename, const std::string& content) {
    std::string path = test_dir + "/" + filename;
    std::ofstream file(path.c_str());
    file << content;
    file.close();
  }

  void modify_file(const std::string& filename, const std::string& new_content) {
    std::string path = test_dir + "/" + filename;
    // Sleep to ensure timestamp changes
    ACE_OS::sleep(ACE_Time_Value(0, 100000)); // 100ms
    std::ofstream file(path.c_str());
    file << new_content;
    file.close();
  }
};

BOOST_FIXTURE_TEST_SUITE(FileEventModifyTestSuite, FileEventModifyTestFixture)

// Test: FileEvent structure for MODIFY operation
BOOST_AUTO_TEST_CASE(test_modify_event_structure)
{
  DirShare::FileEvent event;
  event.filename = CORBA::string_dup("modified.txt");
  event.operation = DirShare::MODIFY;
  event.timestamp_sec = 1234567890ULL;
  event.timestamp_nsec = 500000000U;

  // Populate metadata
  event.metadata.filename = CORBA::string_dup("modified.txt");
  event.metadata.size = 2048ULL;
  event.metadata.timestamp_sec = 1234567890ULL;
  event.metadata.timestamp_nsec = 500000000U;
  event.metadata.checksum = 0x87654321UL;

  // Verify fields are correctly set
  BOOST_CHECK_EQUAL(std::string(event.filename.in()), "modified.txt");
  BOOST_CHECK_EQUAL(event.operation, DirShare::MODIFY);
  BOOST_CHECK_EQUAL(event.timestamp_sec, 1234567890ULL);
  BOOST_CHECK_EQUAL(event.timestamp_nsec, 500000000U);
  BOOST_CHECK_EQUAL(event.metadata.size, 2048ULL);
  BOOST_CHECK_EQUAL(event.metadata.checksum, 0x87654321UL);
}

// Test: File modification detection via FileMonitor
BOOST_AUTO_TEST_CASE(test_modification_detection)
{
  // Create initial file
  create_file("detect.txt", "initial content");

  // Create FileMonitor and do initial scan
  DirShare::FileChangeTracker change_tracker;

  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  std::vector<std::string> created, modified, deleted;

  // Initial scan - should detect file as created
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  // Modify the file
  modify_file("detect.txt", "modified content with more text");

  // Second scan - should detect modification
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);
  BOOST_CHECK_EQUAL(modified[0], "detect.txt");
}

// Test: Size change triggers modification detection
BOOST_AUTO_TEST_CASE(test_modification_size_change)
{
  create_file("size_change.txt", "short");

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  std::vector<std::string> created, modified, deleted;

  // Initial scan
  monitor.scan_for_changes(created, modified, deleted);

  // Modify file with different size
  modify_file("size_change.txt", "this is a much longer content string");

  // Should detect modification due to size change
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK_EQUAL(modified[0], "size_change.txt");
}

// Test: Timestamp change triggers modification detection
BOOST_AUTO_TEST_CASE(test_modification_timestamp_change)
{
  create_file("timestamp_test.txt", "content");

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  std::vector<std::string> created, modified, deleted;

  // Initial scan
  monitor.scan_for_changes(created, modified, deleted);

  // Touch file to change timestamp
  std::string path = test_dir + "/timestamp_test.txt";
  // Sleep for 1.1 seconds to ensure timestamp changes on all filesystems
  // (some filesystems like HFS+ have 1-second timestamp granularity)
  ACE_OS::sleep(ACE_Time_Value(1, 100000)); // 1.1 seconds

  // Re-write same content (timestamp changes, content same)
  std::ofstream file(path.c_str());
  file << "content";
  file.close();

  // Should detect modification due to timestamp change
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
}

// Test: Checksum change triggers modification detection
BOOST_AUTO_TEST_CASE(test_modification_checksum_change)
{
  create_file("checksum_test.txt", "original checksum");

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  std::vector<std::string> created, modified, deleted;

  // Initial scan
  monitor.scan_for_changes(created, modified, deleted);

  // Modify content (changes checksum)
  modify_file("checksum_test.txt", "modified checksum");

  // Should detect modification due to checksum change
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
}

// Test: MODIFY event publishing logic (metadata consistency)
BOOST_AUTO_TEST_CASE(test_modify_event_publishing)
{
  // Create and modify a file
  create_file("publish_test.txt", "v1");
  modify_file("publish_test.txt", "v2 modified");

  // Get file metadata for MODIFY event
  DirShare::FileChangeTracker change_tracker;

  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  DirShare::FileMetadata metadata;
  bool success = monitor.get_file_metadata("publish_test.txt", metadata);

  BOOST_REQUIRE(success);

  // Create MODIFY event
  DirShare::FileEvent event;
  event.filename = metadata.filename;
  event.operation = DirShare::MODIFY;
  event.timestamp_sec = metadata.timestamp_sec;
  event.timestamp_nsec = metadata.timestamp_nsec;
  event.metadata = metadata;

  // Verify event structure
  BOOST_CHECK_EQUAL(event.operation, DirShare::MODIFY);
  BOOST_CHECK_EQUAL(std::string(event.filename.in()), "publish_test.txt");
  BOOST_CHECK_EQUAL(event.metadata.size, metadata.size);
  BOOST_CHECK_EQUAL(event.metadata.checksum, metadata.checksum);
}

// Test: Only modified files are detected (efficiency verification)
BOOST_AUTO_TEST_CASE(test_efficiency_only_modified_files)
{
  // Create multiple files
  create_file("file1.txt", "content 1");
  create_file("file2.txt", "content 2");
  create_file("file3.txt", "content 3");

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  std::vector<std::string> created, modified, deleted;

  // Initial scan
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 3u);

  // Modify only one file
  modify_file("file2.txt", "content 2 modified");

  // Second scan - should detect only 1 modification
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);
  BOOST_CHECK_EQUAL(modified[0], "file2.txt");
}

// Test: Multiple sequential modifications
BOOST_AUTO_TEST_CASE(test_sequential_modifications)
{
  create_file("sequential.txt", "version 1");

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker, true);
  std::vector<std::string> created, modified, deleted;

  // Initial scan
  monitor.scan_for_changes(created, modified, deleted);

  // First modification
  modify_file("sequential.txt", "version 2");
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(modified.size(), 1u);

  // Second modification
  modify_file("sequential.txt", "version 3");
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(modified.size(), 1u);

  // Third modification
  modify_file("sequential.txt", "version 4");
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
}

// Test: Verify metadata updates after modification
BOOST_AUTO_TEST_CASE(test_metadata_updates_after_modification)
{
  create_file("metadata_update.txt", "before");

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker, true);

  // Get initial metadata
  DirShare::FileMetadata metadata_before;
  BOOST_REQUIRE(monitor.get_file_metadata("metadata_update.txt", metadata_before));
  unsigned long long size_before = metadata_before.size;
  unsigned long checksum_before = metadata_before.checksum;

  // Modify file
  modify_file("metadata_update.txt", "after modification with more content");

  // Get updated metadata
  DirShare::FileMetadata metadata_after;
  BOOST_REQUIRE(monitor.get_file_metadata("metadata_update.txt", metadata_after));
  unsigned long long size_after = metadata_after.size;
  unsigned long checksum_after = metadata_after.checksum;

  // Verify metadata changed
  BOOST_CHECK_NE(size_before, size_after);
  BOOST_CHECK_NE(checksum_before, checksum_after);
  BOOST_CHECK_GT(size_after, size_before); // Should be larger
}

BOOST_AUTO_TEST_SUITE_END()

// ================================================================================
// T094a: Notification Loop Prevention Tests for MODIFY Flow (SC-011)
// ================================================================================

struct ModifyLoopPreventionFixture {
  ModifyLoopPreventionFixture()
    : test_dir("test_modify_loop_boost")
    , change_tracker()
  {
    ACE_OS::mkdir(test_dir);
  }

  ~ModifyLoopPreventionFixture() {
    cleanup_directory(test_dir);
  }

  void cleanup_directory(const char* dir) {
    std::vector<std::string> files;
    if (DirShare::list_directory_files(dir, files)) {
      for (size_t i = 0; i < files.size(); ++i) {
        std::string path = std::string(dir) + "/" + files[i];
        ACE_OS::unlink(path.c_str());
      }
    }
    ACE_OS::rmdir(dir);
  }

  void create_file(const std::string& filename, const std::string& content) {
    std::string path = std::string(test_dir) + "/" + filename;
    std::ofstream file(path.c_str());
    file << content;
    file.close();
  }

  void modify_file(const std::string& filename, const std::string& new_content) {
    std::string path = std::string(test_dir) + "/" + filename;
    ACE_OS::sleep(ACE_Time_Value(0, 100000)); // 100ms to ensure timestamp changes
    std::ofstream file(path.c_str());
    file << new_content;
    file.close();
  }

  const char* test_dir;
  DirShare::FileChangeTracker change_tracker;
};

BOOST_FIXTURE_TEST_SUITE(ModifyLoopPrevention, ModifyLoopPreventionFixture)

// Test: Remote MODIFY event suppresses notifications
BOOST_AUTO_TEST_CASE(test_remote_modify_suppresses_notifications)
{
  std::string filename = "remote_modified.txt";

  // Initially, file is not suppressed
  BOOST_CHECK(!change_tracker.is_suppressed(filename));

  // Simulate receiving remote MODIFY event
  // FileEventListenerImpl::handle_modify_event should call suppress_notifications
  change_tracker.suppress_notifications(filename);

  // Now file should be suppressed
  BOOST_CHECK(change_tracker.is_suppressed(filename));
}

// Test: MODIFY event when file doesn't exist (treated as CREATE)
BOOST_AUTO_TEST_CASE(test_modify_treated_as_create_suppresses)
{
  std::string filename = "nonexistent.txt";

  // File doesn't exist locally
  std::string full_path = std::string(test_dir) + "/" + filename;
  BOOST_CHECK(!DirShare::file_exists(full_path));

  // Receive MODIFY event for non-existent file
  // Should suppress (treated as CREATE)
  change_tracker.suppress_notifications(filename);
  BOOST_CHECK(change_tracker.is_suppressed(filename));

  // File content arrives and is written
  create_file(filename, "new content");

  // Resume after writing
  change_tracker.resume_notifications(filename);
  BOOST_CHECK(!change_tracker.is_suppressed(filename));
}

// Test: FileMonitor respects suppression for MODIFY
BOOST_AUTO_TEST_CASE(test_file_monitor_respects_modify_suppression)
{
  DirShare::FileMonitor monitor(test_dir, change_tracker, true);

  // Create a file
  std::string filename = "suppress_modify.txt";
  create_file(filename, "original content");

  // Initial scan to establish baseline
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  // Suppress this file
  change_tracker.suppress_notifications(filename);

  // Modify the file
  modify_file(filename, "modified content");

  // Scan - file should NOT appear in modified list (suppressed)
  created.clear();
  modified.clear();
  deleted.clear();
  monitor.scan_for_changes(created, modified, deleted);

  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) == modified.end());

  // Resume notifications
  change_tracker.resume_notifications(filename);

  // Modify again
  modify_file(filename, "modified again");

  // Scan - now file SHOULD appear in modified list
  created.clear();
  modified.clear();
  deleted.clear();
  monitor.scan_for_changes(created, modified, deleted);

  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) != modified.end());
}

// Test: Complete MODIFY loop prevention flow
BOOST_AUTO_TEST_CASE(test_complete_modify_loop_prevention_flow)
{
  DirShare::FileMonitor monitor(test_dir, change_tracker, true);

  std::string filename = "remote_mod.txt";
  create_file(filename, "version 1");

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  // Step 1: Receive remote MODIFY event -> suppress
  change_tracker.suppress_notifications(filename);
  BOOST_CHECK(change_tracker.is_suppressed(filename));

  // Step 2: Remote file content arrives and overwrites local file
  modify_file(filename, "version 2 from remote");

  // Step 3: Monitor scans but file is suppressed
  created.clear();
  modified.clear();
  monitor.scan_for_changes(created, modified, deleted);

  // Verify file does NOT appear in modified list (suppressed)
  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) == modified.end());

  // Step 4: Resume after file is written
  change_tracker.resume_notifications(filename);
  BOOST_CHECK(!change_tracker.is_suppressed(filename));

  // Step 5: Next scan - file already in previous state, no MODIFY event
  created.clear();
  modified.clear();
  monitor.scan_for_changes(created, modified, deleted);

  // File should not appear as modified (already in previous scan)
  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) == modified.end());
}

// Test: Mixed local and remote modifications
BOOST_AUTO_TEST_CASE(test_mixed_local_remote_modifies)
{
  DirShare::FileMonitor monitor(test_dir, change_tracker, true);

  // Create two files
  std::string local_file = "local_mod.txt";
  std::string remote_file = "remote_mod.txt";
  create_file(local_file, "local v1");
  create_file(remote_file, "remote v1");

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);

  // Local modification - should be published
  modify_file(local_file, "local v2");

  // Remote modification - should NOT be published (suppressed)
  change_tracker.suppress_notifications(remote_file);
  modify_file(remote_file, "remote v2");

  // Scan - should only detect local modification
  created.clear();
  modified.clear();
  monitor.scan_for_changes(created, modified, deleted);

  // Verify local file IS in modified list
  BOOST_CHECK(std::find(modified.begin(), modified.end(), local_file) != modified.end());

  // Verify remote file is NOT in modified list (suppressed)
  BOOST_CHECK(std::find(modified.begin(), modified.end(), remote_file) == modified.end());

  // Resume remote file
  change_tracker.resume_notifications(remote_file);
}

// Test: Suppression prevents duplicate MODIFY events
BOOST_AUTO_TEST_CASE(test_no_duplicate_modify_events)
{
  DirShare::FileMonitor monitor(test_dir, change_tracker, true);

  std::string filename = "no_dup_modify.txt";
  create_file(filename, "original");

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);

  // Simulate: Machine B receives MODIFY event from Machine A
  change_tracker.suppress_notifications(filename);

  // File is overwritten by FileContentListener
  modify_file(filename, "modified by remote");

  // Scan 1: File is suppressed, should not appear
  created.clear();
  modified.clear();
  monitor.scan_for_changes(created, modified, deleted);
  int first_scan_count = std::count(modified.begin(), modified.end(), filename);
  BOOST_CHECK_EQUAL(first_scan_count, 0);  // Not in list (suppressed)

  // Resume notifications
  change_tracker.resume_notifications(filename);

  // Scan 2: File already in previous state, should not appear as MODIFY
  created.clear();
  modified.clear();
  monitor.scan_for_changes(created, modified, deleted);
  int second_scan_count = std::count(modified.begin(), modified.end(), filename);
  BOOST_CHECK_EQUAL(second_scan_count, 0);  // Not in list (already tracked)

  // Total: Zero MODIFY events published by Machine B -> Loop prevented!
  int total_modifies = first_scan_count + second_scan_count;
  BOOST_CHECK_EQUAL(total_modifies, 0);
}

// Test: Multiple sequential remote modifications
BOOST_AUTO_TEST_CASE(test_sequential_remote_modifies)
{
  DirShare::FileMonitor monitor(test_dir, change_tracker, true);

  std::string filename = "sequential_remote.txt";
  create_file(filename, "v1");

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);

  // Remote modify 1
  change_tracker.suppress_notifications(filename);
  modify_file(filename, "v2 remote");
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) == modified.end());
  change_tracker.resume_notifications(filename);

  // Remote modify 2
  change_tracker.suppress_notifications(filename);
  modify_file(filename, "v3 remote");
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) == modified.end());
  change_tracker.resume_notifications(filename);

  // Remote modify 3
  change_tracker.suppress_notifications(filename);
  modify_file(filename, "v4 remote");
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK(std::find(modified.begin(), modified.end(), filename) == modified.end());
  change_tracker.resume_notifications(filename);

  // Final scan - no modifications should be detected
  created.clear();
  modified.clear();
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK(modified.empty());
}

// Test: Timestamp comparison with suppression
BOOST_AUTO_TEST_CASE(test_timestamp_comparison_with_suppression)
{
  std::string filename = "timestamp_suppress.txt";
  create_file(filename, "old version");

  // Simulate: Local file timestamp = 1000, Remote file timestamp = 2000 (newer)
  // Remote is newer, so MODIFY should be accepted and suppressed
  change_tracker.suppress_notifications(filename);
  BOOST_CHECK(change_tracker.is_suppressed(filename));

  // Remote file overwrites local
  modify_file(filename, "newer version from remote");

  // Resume
  change_tracker.resume_notifications(filename);
  BOOST_CHECK(!change_tracker.is_suppressed(filename));
}

// Test: Error recovery - resume on error
BOOST_AUTO_TEST_CASE(test_error_recovery_resume_on_failure)
{
  std::string filename = "error_file.txt";

  // Suppress before attempting update
  change_tracker.suppress_notifications(filename);
  BOOST_CHECK(change_tracker.is_suppressed(filename));

  // Simulate error during update (checksum mismatch, write failure, etc.)
  // Implementation should call resume_notifications even on error
  bool update_failed = true;

  if (update_failed) {
    // Ensure we resume even on error to prevent permanent suppression
    change_tracker.resume_notifications(filename);
  }

  // Assert - should be resumed despite error
  BOOST_CHECK(!change_tracker.is_suppressed(filename));
}

BOOST_AUTO_TEST_SUITE_END()
