#define BOOST_TEST_MODULE DirectorySnapshotTest
#include <boost/test/included/unit_test.hpp>

#include "../FileMonitor.h"
#include "../FileChangeTracker.h"
#include "../FileUtils.h"
#include "../DirShareTypeSupportImpl.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <set>
#include <fstream>

BOOST_AUTO_TEST_SUITE(DirectorySnapshotTestSuite)

// Test: Generate directory snapshot with multiple files
BOOST_AUTO_TEST_CASE(test_generate_snapshot_multiple_files)
{
  const char* test_dir = "test_snapshot_dir";

  // Create test directory
  ACE_OS::mkdir(test_dir);

  // Create test files
  const char* file1 = "test_snapshot_dir/file1.txt";
  const char* file2 = "test_snapshot_dir/file2.txt";
  const char* file3 = "test_snapshot_dir/file3.txt";

  DirShare::write_file(file1, reinterpret_cast<const unsigned char*>("content1"), 8);
  DirShare::write_file(file2, reinterpret_cast<const unsigned char*>("content2"), 8);
  DirShare::write_file(file3, reinterpret_cast<const unsigned char*>("content3"), 8);

  // Create FileMonitor and get snapshot
  DirShare::FileChangeTracker change_tracker;
  DirShare::FileMonitor monitor(test_dir, change_tracker);
  std::vector<DirShare::FileMetadata> files = monitor.get_all_files();

  // Verify we got all 3 files
  BOOST_CHECK_EQUAL(files.size(), 3);

  // Verify all filenames are present
  std::set<std::string> filenames;
  for (size_t i = 0; i < files.size(); ++i) {
    filenames.insert(files[i].filename.in());
  }

  BOOST_CHECK(filenames.find("file1.txt") != filenames.end());
  BOOST_CHECK(filenames.find("file2.txt") != filenames.end());
  BOOST_CHECK(filenames.find("file3.txt") != filenames.end());

  // Verify metadata is populated
  for (size_t i = 0; i < files.size(); ++i) {
    BOOST_CHECK_EQUAL(files[i].size, 8);
    BOOST_CHECK(files[i].checksum != 0); // CRC32 should be computed
    BOOST_CHECK(files[i].timestamp_sec > 0);
  }

  // Cleanup
  ACE_OS::unlink(file1);
  ACE_OS::unlink(file2);
  ACE_OS::unlink(file3);
  ACE_OS::rmdir(test_dir);
}

// Test: Empty directory snapshot
BOOST_AUTO_TEST_CASE(test_snapshot_empty_directory)
{
  const char* test_dir = "test_empty_snapshot_dir";

  // Create empty test directory
  ACE_OS::mkdir(test_dir);

  // Create FileMonitor and get snapshot
  DirShare::FileChangeTracker change_tracker;
  DirShare::FileMonitor monitor(test_dir, change_tracker);
  std::vector<DirShare::FileMetadata> files = monitor.get_all_files();

  // Verify empty snapshot
  BOOST_CHECK_EQUAL(files.size(), 0);

  // Cleanup
  ACE_OS::rmdir(test_dir);
}

// Test: Snapshot comparison - detect missing files
BOOST_AUTO_TEST_CASE(test_snapshot_comparison_missing_files)
{
  const char* local_dir = "test_local_dir";
  const char* remote_dir = "test_remote_dir";

  // Create directories
  ACE_OS::mkdir(local_dir);
  ACE_OS::mkdir(remote_dir);

  // Create files only in remote directory
  const char* remote_file1 = "test_remote_dir/remote1.txt";
  const char* remote_file2 = "test_remote_dir/remote2.txt";
  DirShare::write_file(remote_file1, reinterpret_cast<const unsigned char*>("remote1"), 7);
  DirShare::write_file(remote_file2, reinterpret_cast<const unsigned char*>("remote2"), 7);

  // Create one shared file
  const char* local_shared = "test_local_dir/shared.txt";
  const char* remote_shared = "test_remote_dir/shared.txt";
  DirShare::write_file(local_shared, reinterpret_cast<const unsigned char*>("shared"), 6);
  DirShare::write_file(remote_shared, reinterpret_cast<const unsigned char*>("shared"), 6);

  // Get snapshots
  DirShare::FileChangeTracker change_tracker;
  DirShare::FileMonitor local_monitor(local_dir, change_tracker);
  DirShare::FileMonitor remote_monitor(remote_dir, change_tracker);

  std::vector<DirShare::FileMetadata> local_files = local_monitor.get_all_files();
  std::vector<DirShare::FileMetadata> remote_files = remote_monitor.get_all_files();

  // Build set of local filenames
  std::set<std::string> local_filenames;
  for (size_t i = 0; i < local_files.size(); ++i) {
    local_filenames.insert(local_files[i].filename.in());
  }

  // Find missing files (files in remote but not in local)
  std::vector<std::string> missing_files;
  for (size_t i = 0; i < remote_files.size(); ++i) {
    std::string filename = remote_files[i].filename.in();
    if (local_filenames.find(filename) == local_filenames.end()) {
      missing_files.push_back(filename);
    }
  }

  // Verify detection
  BOOST_CHECK_EQUAL(missing_files.size(), 2);
  BOOST_CHECK(std::find(missing_files.begin(), missing_files.end(), "remote1.txt") != missing_files.end());
  BOOST_CHECK(std::find(missing_files.begin(), missing_files.end(), "remote2.txt") != missing_files.end());

  // Cleanup
  ACE_OS::unlink(local_shared);
  ACE_OS::unlink(remote_file1);
  ACE_OS::unlink(remote_file2);
  ACE_OS::unlink(remote_shared);
  ACE_OS::rmdir(local_dir);
  ACE_OS::rmdir(remote_dir);
}

// Test: Snapshot comparison - identical directories
BOOST_AUTO_TEST_CASE(test_snapshot_comparison_identical)
{
  const char* dir1 = "test_identical_dir1";
  const char* dir2 = "test_identical_dir2";

  // Create directories
  ACE_OS::mkdir(dir1);
  ACE_OS::mkdir(dir2);

  // Create identical files
  const char* file1_a = "test_identical_dir1/file.txt";
  const char* file1_b = "test_identical_dir2/file.txt";
  DirShare::write_file(file1_a, reinterpret_cast<const unsigned char*>("same"), 4);
  DirShare::write_file(file1_b, reinterpret_cast<const unsigned char*>("same"), 4);

  // Get snapshots
  DirShare::FileChangeTracker change_tracker;
  DirShare::FileMonitor monitor1(dir1, change_tracker);
  DirShare::FileMonitor monitor2(dir2, change_tracker);

  std::vector<DirShare::FileMetadata> files1 = monitor1.get_all_files();
  std::vector<DirShare::FileMetadata> files2 = monitor2.get_all_files();

  // Build sets
  std::set<std::string> filenames1, filenames2;
  for (size_t i = 0; i < files1.size(); ++i) {
    filenames1.insert(files1[i].filename.in());
  }
  for (size_t i = 0; i < files2.size(); ++i) {
    filenames2.insert(files2[i].filename.in());
  }

  // Find differences
  std::vector<std::string> missing_in_1, missing_in_2;
  for (const std::string& fname : filenames2) {
    if (filenames1.find(fname) == filenames1.end()) {
      missing_in_1.push_back(fname);
    }
  }
  for (const std::string& fname : filenames1) {
    if (filenames2.find(fname) == filenames2.end()) {
      missing_in_2.push_back(fname);
    }
  }

  // Verify no differences
  BOOST_CHECK_EQUAL(missing_in_1.size(), 0);
  BOOST_CHECK_EQUAL(missing_in_2.size(), 0);

  // Cleanup
  ACE_OS::unlink(file1_a);
  ACE_OS::unlink(file1_b);
  ACE_OS::rmdir(dir1);
  ACE_OS::rmdir(dir2);
}

// Test: Snapshot with existing files metadata accuracy
BOOST_AUTO_TEST_CASE(test_snapshot_metadata_accuracy)
{
  const char* test_dir = "test_metadata_dir";
  ACE_OS::mkdir(test_dir);

  // Create test file with known content
  const char* test_file = "test_metadata_dir/metadata_test.txt";
  const unsigned char test_data[] = "Test data for metadata verification";
  size_t data_size = sizeof(test_data) - 1;

  DirShare::write_file(test_file, test_data, data_size);

  // Get snapshot
  DirShare::FileChangeTracker change_tracker;
  DirShare::FileMonitor monitor(test_dir, change_tracker);
  std::vector<DirShare::FileMetadata> files = monitor.get_all_files();

  BOOST_REQUIRE_EQUAL(files.size(), 1);

  // Verify metadata
  BOOST_CHECK_EQUAL(std::string(files[0].filename.in()), "metadata_test.txt");
  BOOST_CHECK_EQUAL(files[0].size, data_size);

  // Verify checksum is consistent
  unsigned long long size_check;
  BOOST_CHECK(DirShare::get_file_size(test_file, size_check));
  BOOST_CHECK_EQUAL(size_check, data_size);

  // Cleanup
  ACE_OS::unlink(test_file);
  ACE_OS::rmdir(test_dir);
}

BOOST_AUTO_TEST_SUITE_END()
