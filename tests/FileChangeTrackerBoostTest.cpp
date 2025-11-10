// FileChangeTrackerBoostTest.cpp
// Boost.Test suite for FileChangeTracker notification loop prevention (SC-011)

#define BOOST_TEST_MODULE FileChangeTrackerTest
#include <boost/test/included/unit_test.hpp>
#include "../FileChangeTracker.h"
#include <thread>
#include <chrono>

using namespace DirShare;

// Test fixture for common setup
struct FileChangeTrackerFixture {
  FileChangeTrackerFixture() : tracker() {}
  ~FileChangeTrackerFixture() {}

  FileChangeTracker tracker;
};

// ================================================================================
// T027s: Basic suppression flag set/clear operations
// ================================================================================

BOOST_FIXTURE_TEST_SUITE(SuppressionOperations, FileChangeTrackerFixture)

BOOST_AUTO_TEST_CASE(test_suppress_single_file)
{
  // Arrange
  std::string filename = "test.txt";

  // Act
  tracker.suppress_notifications(filename);

  // Assert
  BOOST_CHECK(tracker.is_suppressed(filename));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 1);
}

BOOST_AUTO_TEST_CASE(test_resume_single_file)
{
  // Arrange
  std::string filename = "test.txt";
  tracker.suppress_notifications(filename);

  // Act
  tracker.resume_notifications(filename);

  // Assert
  BOOST_CHECK(!tracker.is_suppressed(filename));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 0);
}

BOOST_AUTO_TEST_CASE(test_suppress_multiple_files)
{
  // Arrange & Act
  tracker.suppress_notifications("file1.txt");
  tracker.suppress_notifications("file2.txt");
  tracker.suppress_notifications("file3.txt");

  // Assert
  BOOST_CHECK(tracker.is_suppressed("file1.txt"));
  BOOST_CHECK(tracker.is_suppressed("file2.txt"));
  BOOST_CHECK(tracker.is_suppressed("file3.txt"));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 3);
}

BOOST_AUTO_TEST_CASE(test_resume_nonexistent_file)
{
  // Act - resume a file that was never suppressed
  tracker.resume_notifications("nonexistent.txt");

  // Assert - should not crash, just no-op
  BOOST_CHECK(!tracker.is_suppressed("nonexistent.txt"));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 0);
}

BOOST_AUTO_TEST_CASE(test_clear_all_suppressions)
{
  // Arrange
  tracker.suppress_notifications("file1.txt");
  tracker.suppress_notifications("file2.txt");
  tracker.suppress_notifications("file3.txt");

  // Act
  tracker.clear();

  // Assert
  BOOST_CHECK(!tracker.is_suppressed("file1.txt"));
  BOOST_CHECK(!tracker.is_suppressed("file2.txt"));
  BOOST_CHECK(!tracker.is_suppressed("file3.txt"));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ================================================================================
// T027t: Thread-safety tests (concurrent access)
// ================================================================================

BOOST_FIXTURE_TEST_SUITE(ThreadSafety, FileChangeTrackerFixture)

BOOST_AUTO_TEST_CASE(test_concurrent_suppress)
{
  // Arrange
  const int num_threads = 10;
  const int operations_per_thread = 100;

  // Act - multiple threads suppressing different files concurrently
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      for (int j = 0; j < operations_per_thread; ++j) {
        std::string filename = "file_" + std::to_string(i * operations_per_thread + j) + ".txt";
        tracker.suppress_notifications(filename);
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // Assert - should have all files suppressed
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), num_threads * operations_per_thread);
}

BOOST_AUTO_TEST_CASE(test_concurrent_suppress_resume)
{
  // Arrange
  const int num_threads = 10;
  std::vector<std::string> filenames;
  for (int i = 0; i < num_threads; ++i) {
    filenames.push_back("file_" + std::to_string(i) + ".txt");
  }

  // Act - concurrent suppress and resume operations
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      // Suppress
      tracker.suppress_notifications(filenames[i]);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      // Resume
      tracker.resume_notifications(filenames[i]);
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // Assert - all should be resumed
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 0);
  for (const auto& filename : filenames) {
    BOOST_CHECK(!tracker.is_suppressed(filename));
  }
}

BOOST_AUTO_TEST_CASE(test_concurrent_is_suppressed)
{
  // Arrange
  tracker.suppress_notifications("test.txt");

  // Act - multiple threads checking suppression status concurrently
  const int num_threads = 20;
  std::atomic<int> suppressed_count(0);

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&]() {
      for (int j = 0; j < 1000; ++j) {
        if (tracker.is_suppressed("test.txt")) {
          suppressed_count++;
        }
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // Assert - should have detected suppression in all checks
  BOOST_CHECK_EQUAL(suppressed_count.load(), num_threads * 1000);
}

BOOST_AUTO_TEST_SUITE_END()

// ================================================================================
// T027u: Multiple file tracking tests
// ================================================================================

BOOST_FIXTURE_TEST_SUITE(MultipleFileTracking, FileChangeTrackerFixture)

BOOST_AUTO_TEST_CASE(test_suppress_A_suppress_B_resume_A_verify_B_still_suppressed)
{
  // Arrange & Act
  tracker.suppress_notifications("fileA.txt");
  tracker.suppress_notifications("fileB.txt");
  tracker.resume_notifications("fileA.txt");

  // Assert
  BOOST_CHECK(!tracker.is_suppressed("fileA.txt"));
  BOOST_CHECK(tracker.is_suppressed("fileB.txt"));  // B should still be suppressed
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 1);
}

BOOST_AUTO_TEST_CASE(test_independent_file_tracking)
{
  // Arrange
  std::vector<std::string> files = {"file1.txt", "file2.txt", "file3.txt", "file4.txt", "file5.txt"};

  // Act - suppress odd-numbered files
  for (size_t i = 0; i < files.size(); ++i) {
    if (i % 2 == 0) {
      tracker.suppress_notifications(files[i]);
    }
  }

  // Assert - only odd indices should be suppressed
  BOOST_CHECK(tracker.is_suppressed("file1.txt"));
  BOOST_CHECK(!tracker.is_suppressed("file2.txt"));
  BOOST_CHECK(tracker.is_suppressed("file3.txt"));
  BOOST_CHECK(!tracker.is_suppressed("file4.txt"));
  BOOST_CHECK(tracker.is_suppressed("file5.txt"));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 3);
}

BOOST_AUTO_TEST_CASE(test_suppress_same_file_multiple_times)
{
  // Arrange
  std::string filename = "test.txt";

  // Act - suppress the same file multiple times
  tracker.suppress_notifications(filename);
  tracker.suppress_notifications(filename);
  tracker.suppress_notifications(filename);

  // Assert - should still count as one suppressed file (set behavior)
  BOOST_CHECK(tracker.is_suppressed(filename));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 1);

  // Resume once should clear it
  tracker.resume_notifications(filename);
  BOOST_CHECK(!tracker.is_suppressed(filename));
  BOOST_CHECK_EQUAL(tracker.suppressed_count(), 0);
}

BOOST_AUTO_TEST_CASE(test_rapid_changes_multiple_files)
{
  // Simulate rapid file changes with multiple files
  for (int iteration = 0; iteration < 100; ++iteration) {
    // Suppress 3 files
    tracker.suppress_notifications("file1.txt");
    tracker.suppress_notifications("file2.txt");
    tracker.suppress_notifications("file3.txt");

    // Resume first file
    tracker.resume_notifications("file1.txt");

    // Check states
    BOOST_CHECK(!tracker.is_suppressed("file1.txt"));
    BOOST_CHECK(tracker.is_suppressed("file2.txt"));
    BOOST_CHECK(tracker.is_suppressed("file3.txt"));

    // Resume remaining
    tracker.resume_notifications("file2.txt");
    tracker.resume_notifications("file3.txt");

    BOOST_CHECK_EQUAL(tracker.suppressed_count(), 0);
  }
}

BOOST_AUTO_TEST_SUITE_END()

// ================================================================================
// T027v: FileMonitor integration tests (verify notifications suppressed)
// ================================================================================

BOOST_FIXTURE_TEST_SUITE(FileMonitorIntegration, FileChangeTrackerFixture)

BOOST_AUTO_TEST_CASE(test_suppression_workflow)
{
  // Simulate the workflow: FileEventListener receives remote change
  std::string filename = "remote_file.txt";

  // Step 1: Suppress before applying remote change
  tracker.suppress_notifications(filename);
  BOOST_CHECK(tracker.is_suppressed(filename));

  // Step 2: FileMonitor would detect the change but should skip it
  // (FileMonitor should call is_suppressed and skip if true)
  bool should_publish = !tracker.is_suppressed(filename);
  BOOST_CHECK(!should_publish);  // Should NOT publish

  // Step 3: Resume after change applied
  tracker.resume_notifications(filename);
  BOOST_CHECK(!tracker.is_suppressed(filename));

  // Step 4: Now FileMonitor can publish if file changes again
  should_publish = !tracker.is_suppressed(filename);
  BOOST_CHECK(should_publish);  // Should publish now
}

BOOST_AUTO_TEST_CASE(test_local_vs_remote_changes)
{
  // Local change - should be published
  std::string local_file = "local.txt";
  BOOST_CHECK(!tracker.is_suppressed(local_file));  // Not suppressed, should publish

  // Remote change - should NOT be published during suppression
  std::string remote_file = "remote.txt";
  tracker.suppress_notifications(remote_file);
  BOOST_CHECK(tracker.is_suppressed(remote_file));  // Suppressed, should NOT publish

  // After resume, remote file can be published again if it changes locally
  tracker.resume_notifications(remote_file);
  BOOST_CHECK(!tracker.is_suppressed(remote_file));  // Can publish now
}

BOOST_AUTO_TEST_CASE(test_error_recovery_with_suppression)
{
  // Simulate an error scenario where file update fails
  std::string filename = "error_file.txt";

  // Suppress before update
  tracker.suppress_notifications(filename);

  // Simulate error during file update
  // (in real code, catch exception and ensure resume is called)
  bool update_failed = true;

  if (update_failed) {
    // Ensure we resume even on error
    tracker.resume_notifications(filename);
  }

  // Assert - should be resumed despite error
  BOOST_CHECK(!tracker.is_suppressed(filename));
}

BOOST_AUTO_TEST_SUITE_END()

// ================================================================================
// Edge cases and error handling
// ================================================================================

BOOST_FIXTURE_TEST_SUITE(EdgeCases, FileChangeTrackerFixture)

BOOST_AUTO_TEST_CASE(test_empty_filename)
{
  // Empty filename should be handled gracefully
  std::string empty = "";
  tracker.suppress_notifications(empty);
  BOOST_CHECK(tracker.is_suppressed(empty));
  tracker.resume_notifications(empty);
  BOOST_CHECK(!tracker.is_suppressed(empty));
}

BOOST_AUTO_TEST_CASE(test_long_filename)
{
  // Very long filename
  std::string long_name(1000, 'a');
  long_name += ".txt";

  tracker.suppress_notifications(long_name);
  BOOST_CHECK(tracker.is_suppressed(long_name));
  tracker.resume_notifications(long_name);
  BOOST_CHECK(!tracker.is_suppressed(long_name));
}

BOOST_AUTO_TEST_CASE(test_special_characters_in_filename)
{
  // Filenames with special characters
  std::vector<std::string> special_files = {
    "file with spaces.txt",
    "file-with-dashes.txt",
    "file_with_underscores.txt",
    "file.multiple.dots.txt",
    "fileéà.txt"  // Unicode
  };

  for (const auto& filename : special_files) {
    tracker.suppress_notifications(filename);
    BOOST_CHECK(tracker.is_suppressed(filename));
    tracker.resume_notifications(filename);
    BOOST_CHECK(!tracker.is_suppressed(filename));
  }
}

BOOST_AUTO_TEST_SUITE_END()
