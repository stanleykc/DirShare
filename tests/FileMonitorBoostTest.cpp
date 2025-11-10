#define BOOST_TEST_MODULE FileMonitorTest
#include <boost/test/included/unit_test.hpp>

#include "../FileMonitor.h"
#include "../FileChangeTracker.h"
#include "../FileUtils.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <fstream>
#include <vector>
#include <algorithm>

// Test fixture for directory cleanup
struct FileMonitorTestFixture {
  DirShare::FileChangeTracker change_tracker;  // Shared tracker for all tests

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
};

BOOST_FIXTURE_TEST_SUITE(FileMonitorTestSuite, FileMonitorTestFixture)

// Test: Monitor detects file creation
BOOST_AUTO_TEST_CASE(test_detect_file_creation)
{
  const char* test_dir = "test_monitor_create_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan (empty directory)
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);

  // Create a file
  std::string test_file = std::string(test_dir) + "/newfile.txt";
  std::ofstream file(test_file.c_str());
  file << "test content";
  file.close();

  // Scan again
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], "newfile.txt");
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  cleanup_directory(test_dir);
}

// Test: Monitor detects file modification
BOOST_AUTO_TEST_CASE(test_detect_file_modification)
{
  const char* test_dir = "test_monitor_modify_boost";
  ACE_OS::mkdir(test_dir);

  // Create initial file
  std::string test_file = std::string(test_dir) + "/testfile.txt";
  std::ofstream file(test_file.c_str());
  file << "initial content";
  file.close();

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan (establish baseline)
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  // Wait a bit to ensure timestamp changes
  ACE_OS::sleep(1);

  // Modify file
  file.open(test_file.c_str(), std::ios::trunc);
  file << "modified content that is different";
  file.close();

  // Scan again
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK_EQUAL(modified[0], "testfile.txt");
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  cleanup_directory(test_dir);
}

// Test: Monitor detects file deletion
BOOST_AUTO_TEST_CASE(test_detect_file_deletion)
{
  const char* test_dir = "test_monitor_delete_boost";
  ACE_OS::mkdir(test_dir);

  // Create initial file
  std::string test_file = std::string(test_dir) + "/deleteme.txt";
  std::ofstream file(test_file.c_str());
  file << "to be deleted";
  file.close();

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan (establish baseline)
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  // Delete file
  ACE_OS::unlink(test_file.c_str());

  // Scan again
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 1u);
  BOOST_CHECK_EQUAL(deleted[0], "deleteme.txt");

  cleanup_directory(test_dir);
}

// Test: Monitor detects multiple changes
BOOST_AUTO_TEST_CASE(test_detect_multiple_changes)
{
  const char* test_dir = "test_monitor_multiple_boost";
  ACE_OS::mkdir(test_dir);

  // Create initial files
  std::string file1 = std::string(test_dir) + "/file1.txt";
  std::string file2 = std::string(test_dir) + "/file2.txt";
  std::ofstream(file1.c_str()) << "file1 content";
  std::ofstream(file2.c_str()) << "file2 content";

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 2u);

  // Wait for timestamp change
  ACE_OS::sleep(1);

  // Make multiple changes:
  // - Create file3
  // - Modify file1
  // - Delete file2
  std::string file3 = std::string(test_dir) + "/file3.txt";
  std::ofstream(file3.c_str()) << "file3 content";
  std::ofstream(file1.c_str(), std::ios::trunc) << "file1 modified";
  ACE_OS::unlink(file2.c_str());

  // Scan again
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK(std::find(created.begin(), created.end(), "file3.txt") != created.end());
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK(std::find(modified.begin(), modified.end(), "file1.txt") != modified.end());
  BOOST_CHECK_EQUAL(deleted.size(), 1u);
  BOOST_CHECK(std::find(deleted.begin(), deleted.end(), "file2.txt") != deleted.end());

  cleanup_directory(test_dir);
}

// Test: Get all files
BOOST_AUTO_TEST_CASE(test_get_all_files)
{
  const char* test_dir = "test_monitor_getall_boost";
  ACE_OS::mkdir(test_dir);

  // Create test files
  std::string file1 = std::string(test_dir) + "/file1.txt";
  std::string file2 = std::string(test_dir) + "/file2.txt";
  std::ofstream(file1.c_str()) << "test1";
  std::ofstream(file2.c_str()) << "test2";

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Get all files
  std::vector<DirShare::FileMetadata> files = monitor.get_all_files();
  BOOST_CHECK_EQUAL(files.size(), 2u);

  // Verify metadata contains filenames
  bool found_file1 = false, found_file2 = false;
  for (size_t i = 0; i < files.size(); ++i) {
    std::string name = files[i].filename.in();
    if (name == "file1.txt") found_file1 = true;
    if (name == "file2.txt") found_file2 = true;
  }
  BOOST_CHECK(found_file1);
  BOOST_CHECK(found_file2);

  cleanup_directory(test_dir);
}

// Test: Get file metadata
BOOST_AUTO_TEST_CASE(test_get_file_metadata)
{
  const char* test_dir = "test_monitor_metadata_boost";
  ACE_OS::mkdir(test_dir);

  // Create test file with known content
  std::string test_file = std::string(test_dir) + "/metadata_test.txt";
  const char* content = "test content for metadata";
  std::ofstream file(test_file.c_str(), std::ios::binary);
  file.write(content, strlen(content));
  file.close();

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Get file metadata
  DirShare::FileMetadata metadata;
  bool result = monitor.get_file_metadata("metadata_test.txt", metadata);
  BOOST_REQUIRE(result);

  // Verify metadata
  BOOST_CHECK_EQUAL(std::string(metadata.filename.in()), "metadata_test.txt");
  BOOST_CHECK_EQUAL(metadata.size, strlen(content));
  BOOST_CHECK(metadata.checksum != 0); // Should have valid checksum

  cleanup_directory(test_dir);
}

// Test: Monitor with nonexistent directory
BOOST_AUTO_TEST_CASE(test_nonexistent_directory)
{
  const char* test_dir = "nonexistent_dir_boost_12345";

  // Constructor should handle nonexistent directory gracefully
  DirShare::FileMonitor monitor(test_dir, change_tracker, true); // fail_silently = true

  // Scan should fail gracefully
  std::vector<std::string> created, modified, deleted;
  bool result = monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK(!result);
}

// Additional test: Empty directory handling
BOOST_AUTO_TEST_CASE(test_empty_directory)
{
  const char* test_dir = "test_monitor_empty_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Multiple scans of empty directory should work
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);

  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  cleanup_directory(test_dir);
}

// Additional test: Rapid successive scans
BOOST_AUTO_TEST_CASE(test_rapid_scans)
{
  const char* test_dir = "test_monitor_rapid_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Create file
  std::string test_file = std::string(test_dir) + "/rapid_test.txt";
  std::ofstream(test_file.c_str()) << "content";

  // Multiple rapid scans should handle state correctly
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u); // Should not report same file again

  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);

  cleanup_directory(test_dir);
}

BOOST_AUTO_TEST_SUITE_END()
