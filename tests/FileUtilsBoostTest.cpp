#define BOOST_TEST_MODULE FileUtilsTest
#include <boost/test/included/unit_test.hpp>

#include "../FileUtils.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <fstream>
#include <vector>

BOOST_AUTO_TEST_SUITE(FileUtilsTestSuite)

// Test: Write and read file
BOOST_AUTO_TEST_CASE(test_write_read_file)
{
  const char* test_file = "test_write_read_boost.txt";
  const unsigned char test_data[] = "Hello, FileUtils!";
  size_t data_size = sizeof(test_data) - 1; // Exclude null terminator

  // Write file
  bool write_result = DirShare::write_file(test_file, test_data, data_size);
  BOOST_REQUIRE(write_result);

  // Read file
  std::vector<unsigned char> read_data;
  bool read_result = DirShare::read_file(test_file, read_data);
  BOOST_REQUIRE(read_result);
  BOOST_CHECK_EQUAL(read_data.size(), data_size);

  // Verify content
  for (size_t i = 0; i < data_size; ++i) {
    BOOST_CHECK_EQUAL(read_data[i], test_data[i]);
  }

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Test: File exists
BOOST_AUTO_TEST_CASE(test_file_exists)
{
  const char* test_file = "test_exists_boost.txt";

  // File should not exist initially
  BOOST_CHECK(!DirShare::file_exists(test_file));

  // Create file
  std::ofstream file(test_file);
  file << "test";
  file.close();

  // File should now exist
  BOOST_CHECK(DirShare::file_exists(test_file));

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Test: Get file size
BOOST_AUTO_TEST_CASE(test_get_file_size)
{
  const char* test_file = "test_size_boost.txt";
  const char* test_data = "12345";
  size_t expected_size = 5;

  // Create file
  std::ofstream file(test_file, std::ios::binary);
  file.write(test_data, expected_size);
  file.close();

  // Get file size
  unsigned long long size;
  bool result = DirShare::get_file_size(test_file, size);
  BOOST_REQUIRE(result);
  BOOST_CHECK_EQUAL(size, expected_size);

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Test: Delete file
BOOST_AUTO_TEST_CASE(test_delete_file)
{
  const char* test_file = "test_delete_boost.txt";

  // Create file
  std::ofstream file(test_file);
  file << "test";
  file.close();

  BOOST_REQUIRE(DirShare::file_exists(test_file));

  // Delete file
  bool result = DirShare::delete_file(test_file);
  BOOST_CHECK(result);
  BOOST_CHECK(!DirShare::file_exists(test_file));
}

// Test: Is directory
BOOST_AUTO_TEST_CASE(test_is_directory)
{
  const char* test_dir = "test_dir_boost";
  const char* test_file = "test_file_boost.txt";

  // Create directory
  ACE_OS::mkdir(test_dir);
  BOOST_CHECK(DirShare::is_directory(test_dir));

  // Create file
  std::ofstream file(test_file);
  file << "test";
  file.close();

  // File should not be a directory
  BOOST_CHECK(!DirShare::is_directory(test_file));

  // Cleanup
  ACE_OS::rmdir(test_dir);
  ACE_OS::unlink(test_file);
}

// Test: List directory files
BOOST_AUTO_TEST_CASE(test_list_directory_files)
{
  const char* test_dir = "test_list_dir_boost";
  ACE_OS::mkdir(test_dir);

  // Create test files
  std::string file1 = std::string(test_dir) + "/file1.txt";
  std::string file2 = std::string(test_dir) + "/file2.txt";
  std::ofstream(file1.c_str()) << "test1";
  std::ofstream(file2.c_str()) << "test2";

  // List files
  std::vector<std::string> files;
  bool result = DirShare::list_directory_files(test_dir, files);
  BOOST_REQUIRE(result);
  BOOST_CHECK_EQUAL(files.size(), 2u);

  // Cleanup
  ACE_OS::unlink(file1.c_str());
  ACE_OS::unlink(file2.c_str());
  ACE_OS::rmdir(test_dir);
}

// Test: Validate filename - safe names
BOOST_AUTO_TEST_CASE(test_validate_filename_safe)
{
  BOOST_CHECK(DirShare::is_valid_filename("file.txt"));
  BOOST_CHECK(DirShare::is_valid_filename("document.pdf"));
  BOOST_CHECK(DirShare::is_valid_filename("my-file_123.txt"));
}

// Test: Validate filename - reject path traversal
BOOST_AUTO_TEST_CASE(test_validate_filename_reject_traversal)
{
  BOOST_CHECK(!DirShare::is_valid_filename("../etc/passwd"));
  BOOST_CHECK(!DirShare::is_valid_filename("..\\windows\\system32"));
  BOOST_CHECK(!DirShare::is_valid_filename("file/../../etc"));
}

// Test: Validate filename - reject absolute paths
BOOST_AUTO_TEST_CASE(test_validate_filename_reject_absolute)
{
  BOOST_CHECK(!DirShare::is_valid_filename("/etc/passwd"));
  BOOST_CHECK(!DirShare::is_valid_filename("\\Windows\\System32"));
  BOOST_CHECK(!DirShare::is_valid_filename("C:\\Windows"));
}

// Test: Validate filename - reject subdirectories
BOOST_AUTO_TEST_CASE(test_validate_filename_reject_subdirs)
{
  BOOST_CHECK(!DirShare::is_valid_filename("subdir/file.txt"));
  BOOST_CHECK(!DirShare::is_valid_filename("subdir\\file.txt"));
}

// Test: Get and set file modification time
BOOST_AUTO_TEST_CASE(test_get_set_file_mtime)
{
  const char* test_file = "test_mtime_boost.txt";

  // Create file
  std::ofstream file(test_file);
  file << "test";
  file.close();

  // Get original modification time
  unsigned long long orig_sec, new_sec;
  unsigned long orig_nsec, new_nsec;
  bool result = DirShare::get_file_mtime(test_file, orig_sec, orig_nsec);
  BOOST_REQUIRE(result);

  // Set new modification time (1 hour earlier)
  unsigned long long test_sec = orig_sec - 3600;
  unsigned long test_nsec = 0;
  result = DirShare::set_file_mtime(test_file, test_sec, test_nsec);
  BOOST_REQUIRE(result);

  // Verify modification time changed
  result = DirShare::get_file_mtime(test_file, new_sec, new_nsec);
  BOOST_REQUIRE(result);
  BOOST_CHECK_EQUAL(new_sec, test_sec);

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Additional test: Binary file handling
BOOST_AUTO_TEST_CASE(test_binary_file_handling)
{
  const char* test_file = "test_binary_boost.bin";

  // Create binary data with non-text characters
  unsigned char binary_data[256];
  for (int i = 0; i < 256; ++i) {
    binary_data[i] = (unsigned char)i;
  }

  // Write binary file
  bool write_result = DirShare::write_file(test_file, binary_data, 256);
  BOOST_REQUIRE(write_result);

  // Read binary file
  std::vector<unsigned char> read_data;
  bool read_result = DirShare::read_file(test_file, read_data);
  BOOST_REQUIRE(read_result);
  BOOST_CHECK_EQUAL(read_data.size(), 256u);

  // Verify all bytes
  for (size_t i = 0; i < 256; ++i) {
    BOOST_CHECK_EQUAL(read_data[i], binary_data[i]);
  }

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Additional test: Empty file handling
BOOST_AUTO_TEST_CASE(test_empty_file)
{
  const char* test_file = "test_empty_boost.txt";

  // Write empty file
  unsigned char empty_data[] = "";
  bool write_result = DirShare::write_file(test_file, empty_data, 0);
  BOOST_REQUIRE(write_result);

  // Verify file exists and has size 0
  BOOST_CHECK(DirShare::file_exists(test_file));
  unsigned long long size;
  bool size_result = DirShare::get_file_size(test_file, size);
  BOOST_REQUIRE(size_result);
  BOOST_CHECK_EQUAL(size, 0ULL);

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Additional test: Large file handling
BOOST_AUTO_TEST_CASE(test_large_file)
{
  const char* test_file = "test_large_boost.bin";

  // Create 1MB of data
  const size_t large_size = 1024 * 1024;
  std::vector<unsigned char> large_data(large_size);
  for (size_t i = 0; i < large_size; ++i) {
    large_data[i] = (unsigned char)(i % 256);
  }

  // Write large file
  bool write_result = DirShare::write_file(test_file, large_data.data(), large_size);
  BOOST_REQUIRE(write_result);

  // Verify file size
  unsigned long long size;
  bool size_result = DirShare::get_file_size(test_file, size);
  BOOST_REQUIRE(size_result);
  BOOST_CHECK_EQUAL(size, large_size);

  // Read and verify (sample check, not all bytes)
  std::vector<unsigned char> read_data;
  bool read_result = DirShare::read_file(test_file, read_data);
  BOOST_REQUIRE(read_result);
  BOOST_CHECK_EQUAL(read_data.size(), large_size);

  // Cleanup
  ACE_OS::unlink(test_file);
}

BOOST_AUTO_TEST_SUITE_END()
