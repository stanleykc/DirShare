#define BOOST_TEST_MODULE FileMonitorCreateTest
#include <boost/test/included/unit_test.hpp>

#include "../FileMonitor.h"
#include "../FileUtils.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <fstream>
#include <vector>

// Test fixture for directory cleanup
struct FileMonitorCreateTestFixture {
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

BOOST_FIXTURE_TEST_SUITE(FileMonitorCreateTestSuite, FileMonitorCreateTestFixture)

// Test: Monitor detects single file creation
BOOST_AUTO_TEST_CASE(test_detect_single_file_creation)
{
  const char* test_dir = "test_monitor_single_create_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan (empty directory)
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  // Create a file
  std::string test_file = std::string(test_dir) + "/newfile.txt";
  std::ofstream file(test_file.c_str());
  file << "test content";
  file.close();

  // Scan again - should detect creation
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], "newfile.txt");
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  cleanup_directory(test_dir);
}

// Test: Monitor detects multiple file creations
BOOST_AUTO_TEST_CASE(test_detect_multiple_file_creations)
{
  const char* test_dir = "test_monitor_multi_create_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan (empty directory)
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);

  // Create multiple files
  for (int i = 0; i < 5; ++i) {
    std::string filename = std::string(test_dir) + "/file" + std::to_string(i) + ".txt";
    std::ofstream file(filename.c_str());
    file << "content " << i;
    file.close();
  }

  // Scan again - should detect all creations
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 5u);
  BOOST_CHECK_EQUAL(modified.size(), 0u);
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  // Verify all expected files are in created list
  for (int i = 0; i < 5; ++i) {
    std::string expected = "file" + std::to_string(i) + ".txt";
    bool found = false;
    for (size_t j = 0; j < created.size(); ++j) {
      if (created[j] == expected) {
        found = true;
        break;
      }
    }
    BOOST_CHECK(found);
  }

  cleanup_directory(test_dir);
}

// Test: Scan state comparison - empty to populated
BOOST_AUTO_TEST_CASE(test_scan_state_empty_to_populated)
{
  const char* test_dir = "test_state_empty_pop_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // First scan - empty state
  std::vector<std::string> created1, modified1, deleted1;
  monitor.scan_for_changes(created1, modified1, deleted1);
  BOOST_CHECK_EQUAL(created1.size(), 0u);

  // Add files
  std::string file1 = std::string(test_dir) + "/file1.txt";
  std::ofstream f1(file1.c_str());
  f1 << "content1";
  f1.close();

  // Second scan - should detect file1
  std::vector<std::string> created2, modified2, deleted2;
  monitor.scan_for_changes(created2, modified2, deleted2);
  BOOST_CHECK_EQUAL(created2.size(), 1u);
  BOOST_CHECK_EQUAL(created2[0], "file1.txt");

  // Add another file
  std::string file2 = std::string(test_dir) + "/file2.txt";
  std::ofstream f2(file2.c_str());
  f2 << "content2";
  f2.close();

  // Third scan - should detect only file2 (file1 already tracked)
  std::vector<std::string> created3, modified3, deleted3;
  monitor.scan_for_changes(created3, modified3, deleted3);
  BOOST_CHECK_EQUAL(created3.size(), 1u);
  BOOST_CHECK_EQUAL(created3[0], "file2.txt");

  cleanup_directory(test_dir);
}

// Test: Scan state comparison - no changes
BOOST_AUTO_TEST_CASE(test_scan_state_no_changes)
{
  const char* test_dir = "test_state_no_change_boost";
  ACE_OS::mkdir(test_dir);

  // Create initial files
  std::string file1 = std::string(test_dir) + "/static1.txt";
  std::ofstream f1(file1.c_str());
  f1 << "static content 1";
  f1.close();

  std::string file2 = std::string(test_dir) + "/static2.txt";
  std::ofstream f2(file2.c_str());
  f2 << "static content 2";
  f2.close();

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // First scan - detect existing files
  std::vector<std::string> created1, modified1, deleted1;
  monitor.scan_for_changes(created1, modified1, deleted1);
  BOOST_CHECK_EQUAL(created1.size(), 2u);

  // Second scan - no changes
  std::vector<std::string> created2, modified2, deleted2;
  monitor.scan_for_changes(created2, modified2, deleted2);
  BOOST_CHECK_EQUAL(created2.size(), 0u);
  BOOST_CHECK_EQUAL(modified2.size(), 0u);
  BOOST_CHECK_EQUAL(deleted2.size(), 0u);

  // Third scan - still no changes
  std::vector<std::string> created3, modified3, deleted3;
  monitor.scan_for_changes(created3, modified3, deleted3);
  BOOST_CHECK_EQUAL(created3.size(), 0u);
  BOOST_CHECK_EQUAL(modified3.size(), 0u);
  BOOST_CHECK_EQUAL(deleted3.size(), 0u);

  cleanup_directory(test_dir);
}

// Test: Scan state comparison - mixed operations
BOOST_AUTO_TEST_CASE(test_scan_state_mixed_operations)
{
  const char* test_dir = "test_state_mixed_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial empty scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 0u);

  // Create file1
  std::string file1 = std::string(test_dir) + "/file1.txt";
  std::ofstream f1(file1.c_str());
  f1 << "content1";
  f1.close();

  // Scan - should see create
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], "file1.txt");

  // Create file2 and modify file1
  ACE_OS::sleep(1); // Ensure timestamp changes
  f1.open(file1.c_str(), std::ios::trunc);
  f1 << "modified content1 with more data";
  f1.close();

  std::string file2 = std::string(test_dir) + "/file2.txt";
  std::ofstream f2(file2.c_str());
  f2 << "content2";
  f2.close();

  // Scan - should see create and modify
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], "file2.txt");
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK_EQUAL(modified[0], "file1.txt");

  cleanup_directory(test_dir);
}

// Test: File creation detection with different file sizes
BOOST_AUTO_TEST_CASE(test_create_detection_various_sizes)
{
  const char* test_dir = "test_create_sizes_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);

  // Create files of different sizes
  struct TestFile {
    std::string name;
    size_t size;
  };

  std::vector<TestFile> test_files;
  test_files.push_back((TestFile){"empty.txt", 0});
  test_files.push_back((TestFile){"small.txt", 100});
  test_files.push_back((TestFile){"medium.txt", 10000});
  test_files.push_back((TestFile){"large.txt", 1000000});

  for (size_t i = 0; i < test_files.size(); ++i) {
    std::string full_path = std::string(test_dir) + "/" + test_files[i].name;
    std::ofstream file(full_path.c_str());
    for (size_t j = 0; j < test_files[i].size; ++j) {
      file << 'x';
    }
    file.close();
  }

  // Scan - should detect all files regardless of size
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), test_files.size());

  cleanup_directory(test_dir);
}

// Test: Creation detection preserves filename exactly
BOOST_AUTO_TEST_CASE(test_create_filename_preservation)
{
  const char* test_dir = "test_create_filename_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);

  // Create file with specific name
  const std::string exact_name = "ExactName-With_Special.123.txt";
  std::string full_path = std::string(test_dir) + "/" + exact_name;
  std::ofstream file(full_path.c_str());
  file << "content";
  file.close();

  // Scan - verify exact filename is preserved
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_REQUIRE_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], exact_name);

  cleanup_directory(test_dir);
}

// Test: Rapid successive creations
BOOST_AUTO_TEST_CASE(test_rapid_successive_creations)
{
  const char* test_dir = "test_rapid_create_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Initial scan
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);

  // Create multiple files rapidly (no sleep between)
  for (int i = 0; i < 10; ++i) {
    std::string filename = std::string(test_dir) + "/rapid" + std::to_string(i) + ".txt";
    std::ofstream file(filename.c_str());
    file << "rapid content " << i;
    file.close();
  }

  // Scan - should detect all files
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 10u);

  cleanup_directory(test_dir);
}

// Test: Create after delete (same filename)
BOOST_AUTO_TEST_CASE(test_create_after_delete_same_name)
{
  const char* test_dir = "test_create_after_del_boost";
  ACE_OS::mkdir(test_dir);

  std::string filename = "recreate.txt";
  std::string full_path = std::string(test_dir) + "/" + filename;

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Create initial file
  std::ofstream file1(full_path.c_str());
  file1 << "initial content";
  file1.close();

  // First scan - detect creation
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  // Delete file
  ACE_OS::unlink(full_path.c_str());

  // Scan - detect deletion
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(deleted.size(), 1u);
  BOOST_CHECK_EQUAL(deleted[0], filename);

  // Recreate file with same name
  std::ofstream file2(full_path.c_str());
  file2 << "recreated content";
  file2.close();

  // Scan - should detect as new creation
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], filename);

  cleanup_directory(test_dir);
}

// Test: State comparison with concurrent create and modify
BOOST_AUTO_TEST_CASE(test_state_concurrent_create_modify)
{
  const char* test_dir = "test_concurrent_ops_boost";
  ACE_OS::mkdir(test_dir);

  DirShare::FileChangeTracker change_tracker;


  DirShare::FileMonitor monitor(test_dir, change_tracker);

  // Create baseline file
  std::string file1 = std::string(test_dir) + "/baseline.txt";
  std::ofstream f1(file1.c_str());
  f1 << "baseline";
  f1.close();

  // First scan - establish baseline
  std::vector<std::string> created, modified, deleted;
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);

  // Wait for timestamp to change
  ACE_OS::sleep(1);

  // Modify baseline and create new file simultaneously
  f1.open(file1.c_str(), std::ios::trunc);
  f1 << "modified baseline with different content";
  f1.close();

  std::string file2 = std::string(test_dir) + "/newfile.txt";
  std::ofstream f2(file2.c_str());
  f2 << "new content";
  f2.close();

  // Scan - should detect both operations
  monitor.scan_for_changes(created, modified, deleted);
  BOOST_CHECK_EQUAL(created.size(), 1u);
  BOOST_CHECK_EQUAL(created[0], "newfile.txt");
  BOOST_CHECK_EQUAL(modified.size(), 1u);
  BOOST_CHECK_EQUAL(modified[0], "baseline.txt");
  BOOST_CHECK_EQUAL(deleted.size(), 0u);

  cleanup_directory(test_dir);
}

BOOST_AUTO_TEST_SUITE_END()
