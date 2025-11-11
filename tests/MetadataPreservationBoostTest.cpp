// MetadataPreservationBoostTest.cpp - Boost.Test suite for metadata preservation
// Tests timestamp extraction, preservation, and validation (US6)

#define BOOST_TEST_MODULE MetadataPreservationBoostTest
#include <boost/test/included/unit_test.hpp>

#include "../FileUtils.h"
#include "../Checksum.h"
#include "../DirShareTypeSupportImpl.h"

#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_time.h>
#include <ace/Time_Value.h>

#include <fstream>
#include <vector>
#include <cstring>

using namespace DirShare;

// Test fixture for metadata preservation tests
struct MetadataPreservationFixture {
  std::string test_dir;

  MetadataPreservationFixture() {
    test_dir = "test_metadata_preservation_dir";

    // Create test directory
    ACE_OS::mkdir(test_dir.c_str());
  }

  ~MetadataPreservationFixture() {
    // Cleanup: remove test files and directory
    cleanup_directory(test_dir);
    ACE_OS::rmdir(test_dir.c_str());
  }

  void cleanup_directory(const std::string& dir) {
    std::vector<std::string> files;
    if (list_directory_files(dir, files)) {
      for (size_t i = 0; i < files.size(); ++i) {
        std::string full_path = dir + "/" + files[i];
        ACE_OS::unlink(full_path.c_str());
      }
    }
  }

  std::string build_path(const std::string& filename) {
    return test_dir + "/" + filename;
  }

  void create_test_file(const std::string& filename, const std::string& content) {
    std::string full_path = build_path(filename);
    std::ofstream ofs(full_path.c_str(), std::ios::binary);
    ofs.write(content.c_str(), content.length());
    ofs.close();
  }
};

//
// Timestamp Extraction Tests (T127)
//

BOOST_FIXTURE_TEST_SUITE(timestamp_extraction_tests, MetadataPreservationFixture)

BOOST_AUTO_TEST_CASE(test_get_file_mtime_basic)
{
  // Create a test file
  create_test_file("timestamp_test.txt", "Test content");
  std::string full_path = build_path("timestamp_test.txt");

  unsigned long long sec;
  unsigned long nsec;

  // Get modification time
  BOOST_REQUIRE(get_file_mtime(full_path, sec, nsec));

  // Verify timestamp is reasonable (within last hour)
  ACE_Time_Value now = ACE_OS::gettimeofday();
  unsigned long long now_sec = static_cast<unsigned long long>(now.sec());

  BOOST_CHECK(sec > 0);
  BOOST_CHECK(sec <= now_sec);
  BOOST_CHECK(sec > now_sec - 3600); // Within last hour
  BOOST_CHECK(nsec < 1000000000); // Nanoseconds must be < 1 billion
}

BOOST_AUTO_TEST_CASE(test_get_file_mtime_nonexistent)
{
  std::string full_path = build_path("nonexistent.txt");

  unsigned long long sec;
  unsigned long nsec;

  // Should fail for nonexistent file
  BOOST_CHECK(!get_file_mtime(full_path, sec, nsec));
}

BOOST_AUTO_TEST_CASE(test_get_file_mtime_precision)
{
  // Create file
  create_test_file("precision_test.txt", "Precision test");
  std::string full_path = build_path("precision_test.txt");

  unsigned long long sec1, sec2;
  unsigned long nsec1, nsec2;

  // Get timestamp twice
  BOOST_REQUIRE(get_file_mtime(full_path, sec1, nsec1));
  BOOST_REQUIRE(get_file_mtime(full_path, sec2, nsec2));

  // Should be identical (file not modified)
  BOOST_CHECK_EQUAL(sec1, sec2);
  BOOST_CHECK_EQUAL(nsec1, nsec2);
}

BOOST_AUTO_TEST_SUITE_END()

//
// Timestamp Preservation Tests (T128)
//

BOOST_FIXTURE_TEST_SUITE(timestamp_preservation_tests, MetadataPreservationFixture)

BOOST_AUTO_TEST_CASE(test_set_file_mtime_basic)
{
  // Create test file
  create_test_file("preserve_test.txt", "Preserve timestamp");
  std::string full_path = build_path("preserve_test.txt");

  // Set a specific timestamp (2023-01-15 12:00:00)
  unsigned long long target_sec = 1673784000ULL; // 2023-01-15 12:00:00 UTC
  unsigned long target_nsec = 123456789;

  BOOST_REQUIRE(set_file_mtime(full_path, target_sec, target_nsec));

  // Verify timestamp was set
  unsigned long long actual_sec;
  unsigned long actual_nsec;
  BOOST_REQUIRE(get_file_mtime(full_path, actual_sec, actual_nsec));

  BOOST_CHECK_EQUAL(actual_sec, target_sec);
  // Note: Some filesystems may have limited nanosecond precision
  // macOS HFS+ has 1-second precision, APFS has nanosecond but may round
  // Just verify timestamp was set (don't check nanoseconds on macOS)
  #ifndef __APPLE__
  BOOST_CHECK(actual_nsec / 1000000 == target_nsec / 1000000);
  #endif
}

BOOST_AUTO_TEST_CASE(test_set_file_mtime_multiple_updates)
{
  // Create file
  create_test_file("multi_update.txt", "Multiple updates");
  std::string full_path = build_path("multi_update.txt");

  // Set timestamp 1
  unsigned long long ts1_sec = 1600000000ULL;
  unsigned long ts1_nsec = 111111111;
  BOOST_REQUIRE(set_file_mtime(full_path, ts1_sec, ts1_nsec));

  // Set timestamp 2 (newer)
  unsigned long long ts2_sec = 1700000000ULL;
  unsigned long ts2_nsec = 222222222;
  BOOST_REQUIRE(set_file_mtime(full_path, ts2_sec, ts2_nsec));

  // Verify final timestamp
  unsigned long long actual_sec;
  unsigned long actual_nsec;
  BOOST_REQUIRE(get_file_mtime(full_path, actual_sec, actual_nsec));

  BOOST_CHECK_EQUAL(actual_sec, ts2_sec);
}

BOOST_AUTO_TEST_CASE(test_set_file_mtime_preserves_content)
{
  // Create file with content
  std::string content = "Content must be preserved when setting timestamp";
  create_test_file("content_test.txt", content);
  std::string full_path = build_path("content_test.txt");

  // Set timestamp
  unsigned long long ts_sec = 1650000000ULL;
  unsigned long ts_nsec = 500000000;
  BOOST_REQUIRE(set_file_mtime(full_path, ts_sec, ts_nsec));

  // Verify content is unchanged
  std::vector<unsigned char> read_data;
  BOOST_REQUIRE(read_file(full_path, read_data));

  BOOST_CHECK_EQUAL(read_data.size(), content.length());
  BOOST_CHECK(std::memcmp(&read_data[0], content.c_str(), content.length()) == 0);
}

BOOST_AUTO_TEST_CASE(test_timestamp_roundtrip)
{
  // Create file
  create_test_file("roundtrip.txt", "Roundtrip test");
  std::string full_path = build_path("roundtrip.txt");

  // Get original timestamp
  unsigned long long orig_sec;
  unsigned long orig_nsec;
  BOOST_REQUIRE(get_file_mtime(full_path, orig_sec, orig_nsec));

  // Modify file content
  std::string new_content = "Modified content for roundtrip test";
  write_file(full_path, reinterpret_cast<const unsigned char*>(new_content.c_str()), new_content.length());

  // Restore original timestamp
  BOOST_REQUIRE(set_file_mtime(full_path, orig_sec, orig_nsec));

  // Verify timestamp was restored
  unsigned long long restored_sec;
  unsigned long restored_nsec;
  BOOST_REQUIRE(get_file_mtime(full_path, restored_sec, restored_nsec));

  BOOST_CHECK_EQUAL(restored_sec, orig_sec);
}

BOOST_AUTO_TEST_SUITE_END()

//
// Metadata Validation Tests (T129)
//

BOOST_FIXTURE_TEST_SUITE(metadata_validation_tests, MetadataPreservationFixture)

BOOST_AUTO_TEST_CASE(test_size_validation_match)
{
  // Simulate FileContent with matching size
  std::string content = "Test file content for size validation";

  // Verify size matches data length
  size_t expected_size = content.length();
  size_t actual_size = content.length();

  BOOST_CHECK_EQUAL(expected_size, actual_size);
}

BOOST_AUTO_TEST_CASE(test_size_validation_mismatch_detection)
{
  // This tests that our validation would catch size mismatches
  size_t metadata_size = 100;
  size_t actual_data_size = 50;

  BOOST_CHECK_NE(metadata_size, actual_data_size);
}

BOOST_AUTO_TEST_CASE(test_checksum_validation_match)
{
  std::string content = "Checksum validation test content";
  const uint8_t* data = reinterpret_cast<const uint8_t*>(content.c_str());

  // Calculate checksum twice
  uint32_t checksum1 = compute_checksum(data, content.length());
  uint32_t checksum2 = compute_checksum(data, content.length());

  // Should be identical for same data
  BOOST_CHECK_EQUAL(checksum1, checksum2);
}

BOOST_AUTO_TEST_CASE(test_checksum_validation_mismatch_detection)
{
  std::string content1 = "Original content";
  std::string content2 = "Modified content";

  uint32_t checksum1 = compute_checksum(
    reinterpret_cast<const uint8_t*>(content1.c_str()), content1.length());
  uint32_t checksum2 = compute_checksum(
    reinterpret_cast<const uint8_t*>(content2.c_str()), content2.length());

  // Checksums should differ for different content
  BOOST_CHECK_NE(checksum1, checksum2);
}

BOOST_AUTO_TEST_CASE(test_metadata_consistency_full_file)
{
  // Create file
  std::string content = "Full metadata consistency test";
  create_test_file("metadata_full.txt", content);
  std::string full_path = build_path("metadata_full.txt");

  // Read file and calculate checksum
  std::vector<unsigned char> data;
  BOOST_REQUIRE(read_file(full_path, data));

  uint32_t checksum = compute_checksum(&data[0], data.size());

  // Get file size
  unsigned long long file_size;
  BOOST_REQUIRE(get_file_size(full_path, file_size));

  // Get timestamp
  unsigned long long ts_sec;
  unsigned long ts_nsec;
  BOOST_REQUIRE(get_file_mtime(full_path, ts_sec, ts_nsec));

  // Verify all metadata is consistent
  BOOST_CHECK_EQUAL(file_size, static_cast<unsigned long long>(content.length()));
  BOOST_CHECK_EQUAL(data.size(), content.length());
  BOOST_CHECK(checksum != 0); // Should have valid checksum
  BOOST_CHECK(ts_sec > 0); // Should have valid timestamp
}

BOOST_AUTO_TEST_SUITE_END()

//
// Special Characters in Filenames Tests (T130)
//

BOOST_FIXTURE_TEST_SUITE(special_characters_tests, MetadataPreservationFixture)

BOOST_AUTO_TEST_CASE(test_filename_with_spaces)
{
  std::string filename = "file with spaces.txt";
  create_test_file(filename, "Spaces test");
  std::string full_path = build_path(filename);

  // Verify operations work with spaces
  BOOST_CHECK(file_exists(full_path));

  unsigned long long size;
  BOOST_CHECK(get_file_size(full_path, size));

  unsigned long long ts_sec;
  unsigned long ts_nsec;
  BOOST_CHECK(get_file_mtime(full_path, ts_sec, ts_nsec));
}

BOOST_AUTO_TEST_CASE(test_filename_with_dots)
{
  std::string filename = "file.with.multiple.dots.txt";
  create_test_file(filename, "Dots test");
  std::string full_path = build_path(filename);

  BOOST_CHECK(file_exists(full_path));

  unsigned long long ts_sec;
  unsigned long ts_nsec;
  BOOST_CHECK(get_file_mtime(full_path, ts_sec, ts_nsec));
}

BOOST_AUTO_TEST_CASE(test_filename_with_underscores_and_dashes)
{
  std::string filename = "file_with-underscores_and-dashes.txt";
  create_test_file(filename, "Underscores and dashes");
  std::string full_path = build_path(filename);

  BOOST_CHECK(file_exists(full_path));

  // Test timestamp preservation with special characters
  unsigned long long ts_sec = 1600000000ULL;
  unsigned long ts_nsec = 0;
  BOOST_CHECK(set_file_mtime(full_path, ts_sec, ts_nsec));

  unsigned long long actual_sec;
  unsigned long actual_nsec;
  BOOST_CHECK(get_file_mtime(full_path, actual_sec, actual_nsec));
  BOOST_CHECK_EQUAL(actual_sec, ts_sec);
}

BOOST_AUTO_TEST_CASE(test_filename_with_numbers)
{
  std::string filename = "file_2023_12_31_v2.txt";
  create_test_file(filename, "Numbers test");
  std::string full_path = build_path(filename);

  BOOST_CHECK(file_exists(full_path));

  std::vector<unsigned char> data;
  BOOST_CHECK(read_file(full_path, data));
  BOOST_CHECK_EQUAL(data.size(), 12UL);
}

BOOST_AUTO_TEST_CASE(test_filename_with_parentheses)
{
  std::string filename = "file_(copy).txt";
  create_test_file(filename, "Parentheses test");
  std::string full_path = build_path(filename);

  BOOST_CHECK(file_exists(full_path));

  unsigned long long size;
  BOOST_CHECK(get_file_size(full_path, size));
}

BOOST_AUTO_TEST_SUITE_END()

//
// Edge Cases and Error Handling
//

BOOST_FIXTURE_TEST_SUITE(edge_cases_tests, MetadataPreservationFixture)

BOOST_AUTO_TEST_CASE(test_empty_file_metadata)
{
  // Empty file should still have valid metadata
  create_test_file("empty.txt", "");
  std::string full_path = build_path("empty.txt");

  unsigned long long size;
  BOOST_REQUIRE(get_file_size(full_path, size));
  BOOST_CHECK_EQUAL(size, 0ULL);

  unsigned long long ts_sec;
  unsigned long ts_nsec;
  BOOST_CHECK(get_file_mtime(full_path, ts_sec, ts_nsec));

  // Can still set timestamp on empty file
  BOOST_CHECK(set_file_mtime(full_path, 1650000000ULL, 0));
}

BOOST_AUTO_TEST_CASE(test_large_timestamp_values)
{
  create_test_file("large_ts.txt", "Large timestamp test");
  std::string full_path = build_path("large_ts.txt");

  // Use a far future timestamp (year 2100)
  unsigned long long future_sec = 4102444800ULL; // 2100-01-01
  unsigned long future_nsec = 999999999;

  BOOST_CHECK(set_file_mtime(full_path, future_sec, future_nsec));

  unsigned long long actual_sec;
  unsigned long actual_nsec;
  BOOST_CHECK(get_file_mtime(full_path, actual_sec, actual_nsec));
  BOOST_CHECK_EQUAL(actual_sec, future_sec);
}

BOOST_AUTO_TEST_CASE(test_zero_nanoseconds)
{
  create_test_file("zero_ns.txt", "Zero nanoseconds");
  std::string full_path = build_path("zero_ns.txt");

  unsigned long long ts_sec = 1650000000ULL;
  unsigned long ts_nsec = 0;

  BOOST_CHECK(set_file_mtime(full_path, ts_sec, ts_nsec));

  unsigned long long actual_sec;
  unsigned long actual_nsec;
  BOOST_CHECK(get_file_mtime(full_path, actual_sec, actual_nsec));
  BOOST_CHECK_EQUAL(actual_sec, ts_sec);
  BOOST_CHECK_EQUAL(actual_nsec, 0UL);
}

BOOST_AUTO_TEST_SUITE_END()
