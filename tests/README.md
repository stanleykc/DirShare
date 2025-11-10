# DirShare Phase 2 Component Unit Tests

This directory contains unit tests for the foundational components of DirShare developed in Phase 2.

## Test Coverage

### ChecksumTest.cpp
Tests the CRC32 checksum utilities (`Checksum.h/cpp`):
- Empty data CRC32 calculation
- Known value verification (standard test vector)
- Incremental CRC32 calculation
- File-based CRC32 calculation
- Error handling for nonexistent files
- Consistency and determinism

### FileUtilsTest.cpp
Tests the file I/O utilities (`FileUtils.h/cpp`):
- File read and write operations
- File existence checking
- File size retrieval
- File deletion
- Directory detection
- Directory file listing
- Filename validation (security)
  - Safe filenames
  - Path traversal rejection
  - Absolute path rejection
  - Subdirectory rejection
- File modification time get/set

### FileMonitorTest.cpp
Tests the file monitoring component (`FileMonitor.h/cpp`):
- File creation detection
- File modification detection
- File deletion detection
- Multiple simultaneous changes
- Get all files in directory
- Get file metadata (FileMetadata struct)
- Error handling for nonexistent directories

## Building Tests

### Using MPC (Make Project Creator)

From the repository root (after running `./configure` and `source setenv.sh`):

```bash
cd DevGuideExamples/DCPS/DirShare/tests
mwc.pl -type gnuace tests.mpc
make
```

Or from the tests directory if gnuace makefiles already generated:

```bash
make
```

### Expected Build Artifacts

**Boost.Test Framework Tests:**
- `ChecksumBoostTest`
- `FileUtilsBoostTest`
- `FileMonitorBoostTest`

## Running Tests

### Run all tests:

```bash
./run_tests.pl
```

This script runs all Boost.Test suites automatically.

### Run individual Boost.Test suites:

```bash
# Basic execution
./ChecksumBoostTest
./FileUtilsBoostTest
./FileMonitorBoostTest

# With detailed logging
./ChecksumBoostTest --log_level=all
./FileUtilsBoostTest --log_level=all --report_level=detailed
./FileMonitorBoostTest --log_level=all

# Run specific test case
./ChecksumBoostTest --run_test=test_crc32_known_value --log_level=all

# Show available tests
./ChecksumBoostTest --list_content
```

## Test Output

Tests use the Boost.Test framework with colored output:
- **Green ✓**: Test passed
- **Red ✗**: Test failed
- Detailed test results and assertion failures

Example output:
```
╔══════════════════════════════════════════════╗
║   DirShare Phase 2 Component Unit Tests     ║
╔══════════════════════════════════════════════╗

=== Running ChecksumBoostTest ===
Running 6 test cases...
test_crc32_empty_data: OK
test_crc32_known_value: OK
...

*** No errors detected

✓ ChecksumBoostTest PASSED
```

## Boost.Test Framework

### Why Boost.Test?

All tests use the Boost.Test framework:

**Advantages of Boost.Test:**
- Industry-standard testing framework
- Rich assertion macros (`BOOST_CHECK`, `BOOST_REQUIRE`, `BOOST_CHECK_EQUAL`)
- Detailed test reporting and logging
- Test fixtures for setup/teardown
- Can run specific tests or suites
- Better integration with IDEs and CI/CD tools

### Boost.Test Features Used

**Test Suites:**
```cpp
BOOST_AUTO_TEST_SUITE(ChecksumTestSuite)
BOOST_AUTO_TEST_CASE(test_crc32_empty_data) { ... }
BOOST_AUTO_TEST_SUITE_END()
```

**Assertions:**
- `BOOST_CHECK(condition)` - Check condition, continue on failure
- `BOOST_REQUIRE(condition)` - Check condition, abort test on failure
- `BOOST_CHECK_EQUAL(actual, expected)` - Check equality with detailed output
- `BOOST_CHECK_NO_THROW(expression)` - Verify no exception thrown

**Test Fixtures:**
```cpp
struct FileMonitorTestFixture {
  void cleanup_directory(const char* dir) { ... }
};
BOOST_FIXTURE_TEST_SUITE(FileMonitorTestSuite, FileMonitorTestFixture)
```

### Boost.Test Command-Line Options

- `--log_level=all` - Show all log messages
- `--report_level=detailed` - Detailed test report
- `--run_test=test_name` - Run specific test case
- `--list_content` - List all tests in the suite
- `--help` - Show all options

### Boost.Test Example Output

```
Running 8 test cases...
test_crc32_empty_data: OK
test_crc32_known_value: OK
test_crc32_incremental: OK
...

*** No errors detected
```

## Test Philosophy

These unit tests validate the Phase 2 foundational components in isolation before they are integrated into the full DirShare application. This approach:

1. **Catches bugs early**: Issues in utilities are found before DDS integration
2. **Documents behavior**: Tests serve as executable specifications
3. **Enables refactoring**: Changes can be made with confidence
4. **Simplifies debugging**: Failures are isolated to specific components
5. **Framework flexibility**: Both custom and Boost.Test frameworks provide complementary benefits

## CI/CD Integration

The `run_tests.pl` script returns:
- Exit code **0**: All tests passed
- Exit code **1**: One or more tests failed

This makes it suitable for CI/CD pipelines:

```bash
./run_tests.pl && echo "Tests passed" || echo "Tests failed"
```

## Future Test Additions

As Phase 3+ development proceeds, additional tests should be added:
- **Integration tests**: Test DDS publishers/subscribers with DirShare logic
- **End-to-end tests**: Multi-process file synchronization scenarios
- **Performance tests**: Measure throughput and latency
- **Stress tests**: Large files, many files, rapid changes

## Dependencies

Tests depend on:
- **ACE/TAO**: For cross-platform abstractions and logging
- **OpenDDS**: For IDL-generated types (FileMetadata, etc.)
- **DirShare library**: The components being tested

Ensure environment is configured:
```bash
source ../../../setenv.sh  # From repository root
```

## Notes

- Tests create temporary files/directories in the current directory
- All temporary artifacts are cleaned up after tests complete
- Tests use simple macro-based assertions (not a full framework like GTest)
- Some tests require file system write permissions in the test directory
- FileMonitor tests use 1-second sleep to ensure timestamp changes are detectable
