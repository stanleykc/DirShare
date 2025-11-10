#define BOOST_TEST_MODULE FileContentTest
#include <boost/test/included/unit_test.hpp>

#include "../FileUtils.h"
#include "../Checksum.h"
#include "../DirShareTypeSupportImpl.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/Time_Value.h>
#include <vector>

BOOST_AUTO_TEST_SUITE(FileContentTestSuite)

// Test: Small file transfer (<10MB threshold)
BOOST_AUTO_TEST_CASE(test_small_file_transfer)
{
  const char* source_file = "test_source_small.txt";
  const char* dest_file = "test_dest_small.txt";

  const unsigned char test_data[] = "This is a small file for testing FileContent transfer";
  size_t data_size = sizeof(test_data) - 1;

  // Write source file
  BOOST_REQUIRE(DirShare::write_file(source_file, test_data, data_size));

  // Read source file
  std::vector<unsigned char> file_data;
  BOOST_REQUIRE(DirShare::read_file(source_file, file_data));

  // Simulate FileContent creation
  DirShare::FileContent content;
  content.filename = "test_dest_small.txt";
  content.size = file_data.size();
  content.checksum = DirShare::compute_checksum(&file_data[0], file_data.size());
  content.data.length(static_cast<CORBA::ULong>(file_data.size()));
  std::memcpy(content.data.get_buffer(), &file_data[0], file_data.size());

  // Simulate receiver: verify checksum
  uint32_t computed_checksum = DirShare::compute_checksum(
    reinterpret_cast<const uint8_t*>(content.data.get_buffer()),
    content.data.length());

  BOOST_CHECK_EQUAL(computed_checksum, content.checksum);

  // Write to destination
  BOOST_REQUIRE(DirShare::write_file(dest_file, content.data.get_buffer(), content.data.length()));

  // Verify content matches
  std::vector<unsigned char> dest_data;
  BOOST_REQUIRE(DirShare::read_file(dest_file, dest_data));
  BOOST_CHECK_EQUAL(dest_data.size(), data_size);

  for (size_t i = 0; i < data_size; ++i) {
    BOOST_CHECK_EQUAL(dest_data[i], test_data[i]);
  }

  // Cleanup
  ACE_OS::unlink(source_file);
  ACE_OS::unlink(dest_file);
}

// Test: FileContent with 1MB file (just under chunking threshold)
BOOST_AUTO_TEST_CASE(test_file_content_1mb)
{
  const char* test_file = "test_1mb.dat";
  const size_t file_size = 1024 * 1024; // 1MB

  // Create 1MB test data
  std::vector<unsigned char> test_data(file_size);
  for (size_t i = 0; i < file_size; ++i) {
    test_data[i] = static_cast<unsigned char>(i % 256);
  }

  // Write file
  BOOST_REQUIRE(DirShare::write_file(test_file, &test_data[0], file_size));

  // Verify we can read it back
  std::vector<unsigned char> read_data;
  BOOST_REQUIRE(DirShare::read_file(test_file, read_data));
  BOOST_CHECK_EQUAL(read_data.size(), file_size);

  // Verify checksum computation
  uint32_t checksum1 = DirShare::compute_checksum(&test_data[0], file_size);
  uint32_t checksum2 = DirShare::compute_checksum(&read_data[0], read_data.size());
  BOOST_CHECK_EQUAL(checksum1, checksum2);

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Test: Checksum verification - valid checksum
BOOST_AUTO_TEST_CASE(test_checksum_verification_valid)
{
  const unsigned char test_data[] = "Test data for checksum verification";
  size_t data_size = sizeof(test_data) - 1;

  // Compute checksum
  uint32_t original_checksum = DirShare::compute_checksum(test_data, data_size);

  // Simulate FileContent
  DirShare::FileContent content;
  content.checksum = original_checksum;
  content.data.length(static_cast<CORBA::ULong>(data_size));
  std::memcpy(content.data.get_buffer(), test_data, data_size);

  // Verify checksum
  uint32_t computed_checksum = DirShare::compute_checksum(
    reinterpret_cast<const uint8_t*>(content.data.get_buffer()),
    content.data.length());

  BOOST_CHECK_EQUAL(computed_checksum, original_checksum);
  BOOST_CHECK_EQUAL(computed_checksum, content.checksum);
}

// Test: Checksum verification - detect corruption
BOOST_AUTO_TEST_CASE(test_checksum_verification_corruption)
{
  const unsigned char test_data[] = "Original data";
  size_t data_size = sizeof(test_data) - 1;

  // Compute checksum for original data
  uint32_t original_checksum = DirShare::compute_checksum(test_data, data_size);

  // Create corrupted data
  unsigned char corrupted_data[data_size];
  std::memcpy(corrupted_data, test_data, data_size);
  corrupted_data[5] = 'X'; // Corrupt one byte

  // Compute checksum for corrupted data
  uint32_t corrupted_checksum = DirShare::compute_checksum(corrupted_data, data_size);

  // Verify checksums are different
  BOOST_CHECK_NE(original_checksum, corrupted_checksum);
}

// Test: Timestamp preservation
BOOST_AUTO_TEST_CASE(test_timestamp_preservation)
{
  const char* source_file = "test_timestamp_source.txt";
  const char* dest_file = "test_timestamp_dest.txt";

  const unsigned char test_data[] = "Data with timestamp";
  size_t data_size = sizeof(test_data) - 1;

  // Write source file
  BOOST_REQUIRE(DirShare::write_file(source_file, test_data, data_size));

  // Get source file timestamp
  unsigned long long orig_sec, dest_sec;
  unsigned long orig_nsec, dest_nsec;
  BOOST_REQUIRE(DirShare::get_file_mtime(source_file, orig_sec, orig_nsec));

  // Simulate FileContent with timestamp
  DirShare::FileContent content;
  content.filename = "test_timestamp_dest.txt";
  content.timestamp_sec = orig_sec;
  content.timestamp_nsec = orig_nsec;
  content.data.length(static_cast<CORBA::ULong>(data_size));
  std::memcpy(content.data.get_buffer(), test_data, data_size);

  // Write destination file
  BOOST_REQUIRE(DirShare::write_file(dest_file, content.data.get_buffer(), content.data.length()));

  // Set timestamp
  BOOST_REQUIRE(DirShare::set_file_mtime(dest_file, content.timestamp_sec, content.timestamp_nsec));

  // Verify timestamp was preserved
  BOOST_REQUIRE(DirShare::get_file_mtime(dest_file, dest_sec, dest_nsec));
  BOOST_CHECK_EQUAL(dest_sec, orig_sec);

  // Note: nsec may have slight variations due to filesystem limitations
  // Check they're within reasonable range (1 second)
  long long time_diff = static_cast<long long>(dest_sec) - static_cast<long long>(orig_sec);
  BOOST_CHECK(std::abs(time_diff) <= 1);

  // Cleanup
  ACE_OS::unlink(source_file);
  ACE_OS::unlink(dest_file);
}

// Test: Empty file transfer
BOOST_AUTO_TEST_CASE(test_empty_file_transfer)
{
  const char* empty_file = "test_empty.txt";

  // Create empty file
  BOOST_REQUIRE(DirShare::write_file(empty_file, nullptr, 0));

  // Verify file exists and is empty
  unsigned long long size;
  BOOST_REQUIRE(DirShare::get_file_size(empty_file, size));
  BOOST_CHECK_EQUAL(size, 0);

  // Read empty file
  std::vector<unsigned char> data;
  BOOST_REQUIRE(DirShare::read_file(empty_file, data));
  BOOST_CHECK_EQUAL(data.size(), 0);

  // Cleanup
  ACE_OS::unlink(empty_file);
}

// Test: Large file near 10MB threshold (9.5MB)
BOOST_AUTO_TEST_CASE(test_file_content_near_threshold)
{
  const char* test_file = "test_9_5mb.dat";
  const size_t file_size = 9 * 1024 * 1024 + 512 * 1024; // 9.5MB

  // Create test data (sparse pattern to save time)
  std::vector<unsigned char> test_data(file_size);
  for (size_t i = 0; i < 1024; ++i) {
    test_data[i] = static_cast<unsigned char>(i % 256);
  }

  // Write file
  BOOST_REQUIRE(DirShare::write_file(test_file, &test_data[0], file_size));

  // Verify size
  unsigned long long size;
  BOOST_REQUIRE(DirShare::get_file_size(test_file, size));
  BOOST_CHECK_EQUAL(size, file_size);

  // Verify checksum computation works on large file
  uint32_t checksum = DirShare::compute_checksum(&test_data[0], file_size);
  BOOST_CHECK(checksum != 0);

  // Cleanup
  ACE_OS::unlink(test_file);
}

BOOST_AUTO_TEST_SUITE_END()
