#define BOOST_TEST_MODULE ChecksumTest
#include <boost/test/included/unit_test.hpp>

#include "../Checksum.h"
#include <ace/OS_NS_unistd.h>
#include <fstream>
#include <cstring>

BOOST_AUTO_TEST_SUITE(ChecksumTestSuite)

// Test: CRC32 of empty data
BOOST_AUTO_TEST_CASE(test_crc32_empty_data)
{
  const unsigned char data[] = "";
  unsigned long crc = DirShare::calculate_crc32(data, 0);
  // CRC32 of empty data should be 0
  BOOST_CHECK_EQUAL(crc, 0UL);
}

// Test: CRC32 of known data
BOOST_AUTO_TEST_CASE(test_crc32_known_value)
{
  const unsigned char data[] = "123456789";
  unsigned long crc = DirShare::calculate_crc32(data, strlen((const char*)data));
  // Known CRC32 value for "123456789" is 0xCBF43926
  BOOST_CHECK_EQUAL(crc, 0xCBF43926UL);
}

// Test: CRC32 incremental calculation
BOOST_AUTO_TEST_CASE(test_crc32_incremental)
{
  const unsigned char data[] = "123456789";

  // Calculate in one pass
  unsigned long crc_full = DirShare::calculate_crc32(data, strlen((const char*)data));

  // Calculate incrementally
  unsigned long crc_inc = 0xFFFFFFFF;
  crc_inc = DirShare::calculate_crc32_incremental(data, 4, crc_inc);
  crc_inc = DirShare::calculate_crc32_incremental(data + 4, 5, crc_inc);
  crc_inc = DirShare::finalize_crc32(crc_inc);

  BOOST_CHECK_EQUAL(crc_full, crc_inc);
}

// Test: CRC32 file calculation
BOOST_AUTO_TEST_CASE(test_crc32_file)
{
  const char* test_file = "test_crc32_file_boost.txt";
  const char* test_data = "Hello, World!";

  // Create test file
  std::ofstream file(test_file, std::ios::binary);
  file.write(test_data, strlen(test_data));
  file.close();

  // Calculate CRC32 of file
  unsigned long checksum;
  bool result = DirShare::calculate_file_crc32(test_file, checksum);

  BOOST_REQUIRE(result);

  // Verify against direct calculation
  unsigned long expected = DirShare::calculate_crc32(
    (const unsigned char*)test_data, strlen(test_data));
  BOOST_CHECK_EQUAL(checksum, expected);

  // Cleanup
  ACE_OS::unlink(test_file);
}

// Test: CRC32 file not found
BOOST_AUTO_TEST_CASE(test_crc32_file_not_found)
{
  unsigned long checksum;
  bool result = DirShare::calculate_file_crc32("nonexistent_file_boost.txt", checksum);
  BOOST_CHECK(!result); // Should fail
}

// Test: CRC32 consistency
BOOST_AUTO_TEST_CASE(test_crc32_consistency)
{
  const unsigned char data[] = "Test data for consistency check";
  unsigned long crc1 = DirShare::calculate_crc32(data, strlen((const char*)data));
  unsigned long crc2 = DirShare::calculate_crc32(data, strlen((const char*)data));
  BOOST_CHECK_EQUAL(crc1, crc2); // Should be deterministic
}

// Additional test: CRC32 with different data sizes
BOOST_AUTO_TEST_CASE(test_crc32_various_sizes)
{
  // Test with 1 byte
  const unsigned char data1[] = "A";
  unsigned long crc1 = DirShare::calculate_crc32(data1, 1);
  BOOST_CHECK(crc1 != 0); // Should produce non-zero checksum

  // Test with 100 bytes
  unsigned char data100[100];
  for (int i = 0; i < 100; ++i) {
    data100[i] = (unsigned char)(i % 256);
  }
  unsigned long crc100 = DirShare::calculate_crc32(data100, 100);
  BOOST_CHECK(crc100 != 0);

  // Verify different data produces different checksums
  BOOST_CHECK(crc1 != crc100);
}

// Additional test: CRC32 incremental edge cases
BOOST_AUTO_TEST_CASE(test_crc32_incremental_edge_cases)
{
  const unsigned char data[] = "ABCDEFGHIJ";
  unsigned long full_crc = DirShare::calculate_crc32(data, 10);

  // Calculate incrementally with different chunk sizes
  unsigned long inc_crc = 0xFFFFFFFF;
  inc_crc = DirShare::calculate_crc32_incremental(data, 1, inc_crc);
  inc_crc = DirShare::calculate_crc32_incremental(data + 1, 3, inc_crc);
  inc_crc = DirShare::calculate_crc32_incremental(data + 4, 6, inc_crc);
  inc_crc = DirShare::finalize_crc32(inc_crc);

  BOOST_CHECK_EQUAL(full_crc, inc_crc);
}

BOOST_AUTO_TEST_SUITE_END()
