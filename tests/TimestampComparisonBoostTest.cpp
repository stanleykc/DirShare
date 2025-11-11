#define BOOST_TEST_MODULE TimestampComparisonTest
#include <boost/test/included/unit_test.hpp>

#include "../FileEventListenerImpl.h"
#include "../FileUtils.h"
#include "../DirShareTypeSupportImpl.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_time.h>
#include <fstream>
#include <vector>

// Test fixture for timestamp comparison tests
struct TimestampComparisonTestFixture {
  std::string test_dir;

  TimestampComparisonTestFixture() : test_dir("test_timestamp_dir") {
    ACE_OS::mkdir(test_dir.c_str());
  }

  ~TimestampComparisonTestFixture() {
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

  // Helper to compare two timestamps
  bool is_timestamp_newer(
    unsigned long long ts1_sec, unsigned long ts1_nsec,
    unsigned long long ts2_sec, unsigned long ts2_nsec)
  {
    if (ts1_sec > ts2_sec) return true;
    if (ts1_sec < ts2_sec) return false;
    return ts1_nsec > ts2_nsec;
  }
};

BOOST_FIXTURE_TEST_SUITE(TimestampComparisonTestSuite, TimestampComparisonTestFixture)

// Test: Timestamp comparison - seconds differ
BOOST_AUTO_TEST_CASE(test_timestamp_newer_seconds)
{
  unsigned long long older_sec = 1000000000ULL;
  unsigned long older_nsec = 500000000U;

  unsigned long long newer_sec = 1000000001ULL; // 1 second later
  unsigned long newer_nsec = 500000000U;

  // newer should be greater than older
  bool newer_is_greater = is_timestamp_newer(newer_sec, newer_nsec, older_sec, older_nsec);
  BOOST_CHECK(newer_is_greater);

  // older should not be greater than newer
  bool older_is_greater = is_timestamp_newer(older_sec, older_nsec, newer_sec, newer_nsec);
  BOOST_CHECK(!older_is_greater);
}

// Test: Timestamp comparison - nanoseconds differ (same seconds)
BOOST_AUTO_TEST_CASE(test_timestamp_newer_nanoseconds)
{
  unsigned long long same_sec = 1234567890ULL;
  unsigned long older_nsec = 100000000U;
  unsigned long newer_nsec = 200000000U; // 100ms later

  // newer should be greater
  bool newer_is_greater = is_timestamp_newer(same_sec, newer_nsec, same_sec, older_nsec);
  BOOST_CHECK(newer_is_greater);

  // older should not be greater
  bool older_is_greater = is_timestamp_newer(same_sec, older_nsec, same_sec, newer_nsec);
  BOOST_CHECK(!older_is_greater);
}

// Test: Timestamp comparison - exact equality
BOOST_AUTO_TEST_CASE(test_timestamp_equal)
{
  unsigned long long ts_sec = 1700000000ULL;
  unsigned long ts_nsec = 123456789U;

  // Same timestamp should not be "newer"
  bool is_newer = is_timestamp_newer(ts_sec, ts_nsec, ts_sec, ts_nsec);
  BOOST_CHECK(!is_newer);
}

// Test: Timestamp comparison - large second difference
BOOST_AUTO_TEST_CASE(test_timestamp_large_diff)
{
  unsigned long long old_sec = 1000000000ULL; // Year 2001
  unsigned long old_nsec = 0U;

  unsigned long long new_sec = 1700000000ULL; // Year 2023
  unsigned long new_nsec = 0U;

  // Newer should win by large margin
  bool newer_wins = is_timestamp_newer(new_sec, new_nsec, old_sec, old_nsec);
  BOOST_CHECK(newer_wins);
}

// Test: Timestamp comparison - nanosecond precision matters
BOOST_AUTO_TEST_CASE(test_timestamp_nanosecond_precision)
{
  unsigned long long sec = 1234567890ULL;
  unsigned long nsec1 = 999999999U; // 999ms
  unsigned long nsec2 = 1000000000U; // Would overflow to next second

  // nsec2 is invalid (>= 1 billion), but test shows nsec comparison
  unsigned long nsec2_valid = 0U; // Next second, 0 nsec

  // With different seconds
  bool is_newer = is_timestamp_newer(sec + 1, nsec2_valid, sec, nsec1);
  BOOST_CHECK(is_newer); // sec+1 is newer than sec
}

// Test: Timestamp ordering - newer wins
BOOST_AUTO_TEST_CASE(test_timestamp_ordering_newer_wins)
{
  create_file("order_test.txt", "initial");

  std::string path = test_dir + "/order_test.txt";

  // Get initial timestamp
  unsigned long long initial_sec;
  unsigned long initial_nsec;
  BOOST_REQUIRE(DirShare::get_file_mtime(path, initial_sec, initial_nsec));

  // Simulate remote timestamp that's newer (1 second later)
  unsigned long long remote_sec = initial_sec + 1;
  unsigned long remote_nsec = initial_nsec;

  // Remote is newer, should win
  bool remote_is_newer = is_timestamp_newer(remote_sec, remote_nsec, initial_sec, initial_nsec);
  BOOST_CHECK(remote_is_newer);
}

// Test: Timestamp ordering - older ignored
BOOST_AUTO_TEST_CASE(test_timestamp_ordering_older_ignored)
{
  create_file("ignore_test.txt", "local");

  std::string path = test_dir + "/ignore_test.txt";

  // Get local timestamp
  unsigned long long local_sec;
  unsigned long local_nsec;
  BOOST_REQUIRE(DirShare::get_file_mtime(path, local_sec, local_nsec));

  // Simulate remote timestamp that's older (10 seconds earlier)
  unsigned long long remote_sec = (local_sec > 10) ? (local_sec - 10) : 0;
  unsigned long remote_nsec = local_nsec;

  // Remote is older, should NOT win
  bool remote_is_newer = is_timestamp_newer(remote_sec, remote_nsec, local_sec, local_nsec);
  BOOST_CHECK(!remote_is_newer);
}

// Test: Timestamp tie-breaker (same timestamp)
BOOST_AUTO_TEST_CASE(test_timestamp_tie)
{
  unsigned long long ts_sec = 1600000000ULL;
  unsigned long ts_nsec = 500000000U;

  // When timestamps are identical, neither should win
  bool is_newer = is_timestamp_newer(ts_sec, ts_nsec, ts_sec, ts_nsec);
  BOOST_CHECK(!is_newer);
}

// Test: Real file timestamp extraction
BOOST_AUTO_TEST_CASE(test_file_timestamp_extraction)
{
  create_file("extract_test.txt", "content");

  std::string path = test_dir + "/extract_test.txt";

  unsigned long long sec;
  unsigned long nsec;
  bool success = DirShare::get_file_mtime(path, sec, nsec);

  BOOST_REQUIRE(success);
  BOOST_CHECK_GT(sec, 0ULL); // Should be non-zero timestamp
  BOOST_CHECK_LT(nsec, 1000000000U); // Nanoseconds should be < 1 billion
}

// Test: Timestamp comparison with file operations
BOOST_AUTO_TEST_CASE(test_timestamp_file_operations)
{
  create_file("file_op_test.txt", "v1");
  std::string path = test_dir + "/file_op_test.txt";

  // Get first timestamp
  unsigned long long ts1_sec;
  unsigned long ts1_nsec;
  BOOST_REQUIRE(DirShare::get_file_mtime(path, ts1_sec, ts1_nsec));

  // Wait to ensure timestamp changes (2 seconds for filesystems with 1-second granularity like HFS+)
  // This ensures even on filesystems with 1-second resolution, we'll see a change
  ACE_OS::sleep(ACE_Time_Value(2, 0)); // 2 seconds

  // Modify file
  std::ofstream file(path.c_str());
  file << "v2 modified";
  file.close();

  // Get second timestamp
  unsigned long long ts2_sec;
  unsigned long ts2_nsec;
  BOOST_REQUIRE(DirShare::get_file_mtime(path, ts2_sec, ts2_nsec));

  // Second timestamp should be newer (guaranteed with 2-second wait)
  bool ts2_newer = is_timestamp_newer(ts2_sec, ts2_nsec, ts1_sec, ts1_nsec);
  BOOST_CHECK(ts2_newer);
}

// Test: Millisecond precision comparison
BOOST_AUTO_TEST_CASE(test_timestamp_millisecond_precision)
{
  unsigned long long sec = 1700000000ULL;
  unsigned long nsec1 = 123000000U; // 123ms
  unsigned long nsec2 = 124000000U; // 124ms (1ms later)

  // 1ms difference should be detected
  bool is_newer = is_timestamp_newer(sec, nsec2, sec, nsec1);
  BOOST_CHECK(is_newer);

  // Reverse should not be newer
  bool reverse = is_timestamp_newer(sec, nsec1, sec, nsec2);
  BOOST_CHECK(!reverse);
}

// Test: Zero timestamp comparison
BOOST_AUTO_TEST_CASE(test_timestamp_zero)
{
  unsigned long long zero_sec = 0ULL;
  unsigned long zero_nsec = 0U;

  unsigned long long nonzero_sec = 1ULL;
  unsigned long nonzero_nsec = 0U;

  // Non-zero should be newer than zero
  bool is_newer = is_timestamp_newer(nonzero_sec, nonzero_nsec, zero_sec, zero_nsec);
  BOOST_CHECK(is_newer);

  // Zero should not be newer than non-zero
  bool reverse = is_timestamp_newer(zero_sec, zero_nsec, nonzero_sec, nonzero_nsec);
  BOOST_CHECK(!reverse);
}

// Test: Maximum timestamp values
BOOST_AUTO_TEST_CASE(test_timestamp_maximum)
{
  unsigned long long max_sec = 0xFFFFFFFFFFFFFFFFULL; // Max 64-bit
  unsigned long max_nsec = 999999999U; // Max valid nanoseconds

  unsigned long long near_max_sec = max_sec - 1;
  unsigned long near_max_nsec = max_nsec;

  // Max should be newer than near-max
  bool is_newer = is_timestamp_newer(max_sec, max_nsec, near_max_sec, near_max_nsec);
  BOOST_CHECK(is_newer);
}

BOOST_AUTO_TEST_SUITE_END()
