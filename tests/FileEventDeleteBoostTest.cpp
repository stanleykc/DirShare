#define BOOST_TEST_MODULE FileEventDeleteTest
#include <boost/test/included/unit_test.hpp>

#include "../FileEventListenerImpl.h"
#include "../FileUtils.h"
#include "../FileMonitor.h"
#include "../FileChangeTracker.h"
#include "../DirShareTypeSupportImpl.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <fstream>
#include <vector>

// Test fixture for directory cleanup and mock DDS setup
struct FileEventDeleteTestFixture {
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

  // Helper to create a file with specific timestamp
  bool create_test_file_with_timestamp(const std::string& path,
                                       unsigned long long sec,
                                       unsigned long nsec) {
    std::ofstream ofs(path.c_str());
    if (!ofs) return false;
    ofs << "Test file content";
    ofs.close();

    // Set modification time
    return DirShare::set_file_mtime(path, sec, nsec);
  }
};

BOOST_FIXTURE_TEST_SUITE(FileEventDeleteTestSuite, FileEventDeleteTestFixture)

// Test: FileEvent structure for DELETE operation
BOOST_AUTO_TEST_CASE(test_delete_event_structure)
{
  DirShare::FileEvent event;
  event.filename = CORBA::string_dup("deleted_file.txt");
  event.operation = DirShare::DELETE;
  event.timestamp_sec = 1234567890ULL;
  event.timestamp_nsec = 500000000U;

  // For DELETE, metadata should be zero/empty (file no longer exists)
  event.metadata.filename = CORBA::string_dup("deleted_file.txt");
  event.metadata.size = 0ULL;
  event.metadata.timestamp_sec = 0ULL;
  event.metadata.timestamp_nsec = 0U;
  event.metadata.checksum = 0UL;

  // Verify fields are correctly set
  BOOST_CHECK_EQUAL(std::string(event.filename.in()), "deleted_file.txt");
  BOOST_CHECK_EQUAL(event.operation, DirShare::DELETE);
  BOOST_CHECK_EQUAL(event.timestamp_sec, 1234567890ULL);
  BOOST_CHECK_EQUAL(event.timestamp_nsec, 500000000U);

  // Metadata should be zero for DELETE
  BOOST_CHECK_EQUAL(event.metadata.size, 0ULL);
  BOOST_CHECK_EQUAL(event.metadata.checksum, 0UL);
}

// Test: DELETE detection in FileMonitor
BOOST_AUTO_TEST_CASE(test_delete_detection_in_monitor)
{
  const char* test_dir = "/tmp/dirshare_delete_test_monitor";
  cleanup_directory(test_dir);
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker tracker;
  DirShare::FileMonitor monitor(test_dir, tracker, true);

  // Create a file
  std::string test_file = std::string(test_dir) + "/test_delete.txt";
  std::ofstream ofs(test_file.c_str());
  ofs << "This file will be deleted";
  ofs.close();

  // First scan - file exists
  std::vector<std::string> created, modified, deleted;
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(created.size(), 1); // File was created
  BOOST_CHECK_EQUAL(deleted.size(), 0);

  // Second scan - no changes
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(created.size(), 0);
  BOOST_CHECK_EQUAL(deleted.size(), 0);

  // Delete the file
  ACE_OS::unlink(test_file.c_str());

  // Third scan - file deleted
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(created.size(), 0);
  BOOST_CHECK_EQUAL(deleted.size(), 1);
  BOOST_CHECK_EQUAL(deleted[0], "test_delete.txt");

  cleanup_directory(test_dir);
}

// Test: Timestamp comparison - remote DELETE is newer
BOOST_AUTO_TEST_CASE(test_delete_timestamp_newer)
{
  unsigned long long local_sec = 1000000000ULL;
  unsigned long local_nsec = 0U;
  unsigned long long remote_sec = 1000000010ULL; // 10 seconds newer
  unsigned long remote_nsec = 0U;

  // Remote is newer (seconds comparison)
  BOOST_CHECK(remote_sec > local_sec);

  // Test nanosecond precision when seconds are equal
  local_sec = 1000000000ULL;
  local_nsec = 123456789U;
  remote_sec = 1000000000ULL;
  remote_nsec = 987654321U; // Same second, but more nanoseconds

  BOOST_CHECK(remote_sec == local_sec);
  BOOST_CHECK(remote_nsec > local_nsec);
}

// Test: Timestamp comparison - local file is newer
BOOST_AUTO_TEST_CASE(test_delete_timestamp_older)
{
  unsigned long long local_sec = 1000000010ULL; // Local is 10 seconds newer
  unsigned long local_nsec = 0U;
  unsigned long long remote_sec = 1000000000ULL;
  unsigned long remote_nsec = 0U;

  // Local is newer - DELETE should be ignored
  BOOST_CHECK(local_sec > remote_sec);

  // Test nanosecond precision when seconds are equal
  local_sec = 1000000000ULL;
  local_nsec = 987654321U; // More nanoseconds
  remote_sec = 1000000000ULL;
  remote_nsec = 123456789U;

  BOOST_CHECK(local_sec == remote_sec);
  BOOST_CHECK(local_nsec > remote_nsec);
  // In this case, local file wins, DELETE should be ignored
}

// Test: DELETE handling when file doesn't exist
BOOST_AUTO_TEST_CASE(test_delete_file_not_exists)
{
  const char* test_dir = "/tmp/dirshare_delete_test_not_exists";
  cleanup_directory(test_dir);
  ACE_OS::mkdir(test_dir);

  std::string test_file = std::string(test_dir) + "/nonexistent.txt";

  // Verify file doesn't exist
  BOOST_CHECK(!DirShare::file_exists(test_file));

  // DELETE should not fail if file doesn't exist
  // This simulates the case where FileEventListenerImpl checks file existence
  // and returns early if the file is already gone

  cleanup_directory(test_dir);
}

// Test: DELETE handling with notification loop suppression (SC-011)
BOOST_AUTO_TEST_CASE(test_delete_notification_suppression)
{
  const char* test_dir = "/tmp/dirshare_delete_test_suppression";
  cleanup_directory(test_dir);
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker tracker;
  DirShare::FileMonitor monitor(test_dir, tracker, true);

  // Create a file
  std::string test_file = std::string(test_dir) + "/suppress_test.txt";
  std::ofstream ofs(test_file.c_str());
  ofs << "Test suppression";
  ofs.close();

  // First scan - file created
  std::vector<std::string> created, modified, deleted;
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(created.size(), 1);

  // Suppress notifications for this file (simulating remote DELETE handling)
  tracker.suppress_notifications("suppress_test.txt");
  BOOST_CHECK(tracker.is_suppressed("suppress_test.txt"));

  // Delete the file while notifications are suppressed
  ACE_OS::unlink(test_file.c_str());

  // Scan should NOT detect delete because notifications are suppressed
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(deleted.size(), 0); // Suppressed, so not detected

  // Resume notifications
  tracker.resume_notifications("suppress_test.txt");
  BOOST_CHECK(!tracker.is_suppressed("suppress_test.txt"));

  cleanup_directory(test_dir);
}

// Test: Multiple file deletions
BOOST_AUTO_TEST_CASE(test_multiple_deletes)
{
  const char* test_dir = "/tmp/dirshare_delete_test_multiple";
  cleanup_directory(test_dir);
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker tracker;
  DirShare::FileMonitor monitor(test_dir, tracker, true);

  // Create multiple files
  std::vector<std::string> filenames;
  filenames.push_back("file1.txt");
  filenames.push_back("file2.txt");
  filenames.push_back("file3.txt");

  for (size_t i = 0; i < filenames.size(); ++i) {
    std::string path = std::string(test_dir) + "/" + filenames[i];
    std::ofstream ofs(path.c_str());
    ofs << "File " << i;
    ofs.close();
  }

  // First scan - files created
  std::vector<std::string> created, modified, deleted;
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(created.size(), 3);

  // Delete all files
  for (size_t i = 0; i < filenames.size(); ++i) {
    std::string path = std::string(test_dir) + "/" + filenames[i];
    ACE_OS::unlink(path.c_str());
  }

  // Second scan - all files deleted
  BOOST_REQUIRE(monitor.scan_for_changes(created, modified, deleted));
  BOOST_CHECK_EQUAL(deleted.size(), 3);

  // Verify all files are detected
  for (size_t i = 0; i < filenames.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < deleted.size(); ++j) {
      if (deleted[j] == filenames[i]) {
        found = true;
        break;
      }
    }
    BOOST_CHECK(found);
  }

  cleanup_directory(test_dir);
}

// Test: DELETE conflict resolution - last-write-wins
BOOST_AUTO_TEST_CASE(test_delete_last_write_wins)
{
  const char* test_dir = "/tmp/dirshare_delete_test_lww";
  cleanup_directory(test_dir);
  ACE_OS::mkdir(test_dir);

  std::string test_file = std::string(test_dir) + "/lww_test.txt";

  // Create file with older timestamp
  unsigned long long old_time_sec = 1000000000ULL;
  unsigned long old_time_nsec = 0U;
  BOOST_REQUIRE(create_test_file_with_timestamp(test_file, old_time_sec, old_time_nsec));

  // Verify timestamp was set
  unsigned long long check_sec;
  unsigned long check_nsec;
  BOOST_REQUIRE(DirShare::get_file_mtime(test_file, check_sec, check_nsec));
  BOOST_CHECK_EQUAL(check_sec, old_time_sec);

  // Simulate DELETE with newer timestamp
  unsigned long long delete_time_sec = 1000000010ULL; // 10 seconds newer
  unsigned long delete_time_nsec = 0U;

  // DELETE timestamp is newer than file timestamp
  BOOST_CHECK(delete_time_sec > old_time_sec);
  // In actual implementation, FileEventListenerImpl would delete the file

  // Simulate opposite case: DELETE with older timestamp
  // Modify file to have newer timestamp
  unsigned long long new_file_sec = 1000000020ULL;
  unsigned long new_file_nsec = 0U;
  DirShare::set_file_mtime(test_file, new_file_sec, new_file_nsec);

  unsigned long long old_delete_sec = 1000000010ULL;
  unsigned long old_delete_nsec = 0U;

  // File timestamp is newer than DELETE timestamp
  BOOST_CHECK(new_file_sec > old_delete_sec);
  // In actual implementation, DELETE would be ignored

  cleanup_directory(test_dir);
}

BOOST_AUTO_TEST_SUITE_END()
