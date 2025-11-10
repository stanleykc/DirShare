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
  ACE_OS::sleep(ACE_Time_Value(0, 100000)); // 100ms

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
