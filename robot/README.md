# Robot Framework Acceptance Tests for DirShare

## Overview

This directory contains Robot Framework acceptance tests for the DirShare distributed file synchronization example. These tests validate the user stories and success criteria defined in the feature specification.

## Quick Start

```bash
# 1. Ensure OpenDDS environment is sourced
source $DDS_ROOT/setenv.sh

# 2. Build DirShare
cd /path/to/DirShare
make

# 3. Setup Python virtual environment (first time only)
cd robot
python3 -m venv venv
source venv/bin/activate
python3 -m pip install -r requirements.txt

# 4. Run tests (activate venv if not already active)
source venv/bin/activate
robot UserStories.robot

# 5. View results
open report.html
```

## Prerequisites

### Environment Setup

1. **OpenDDS Configured**: Ensure OpenDDS is built and environment is sourced
   ```bash
   cd /path/to/OpenDDS
   source setenv.sh
   ```

2. **DirShare Built**: Build the DirShare executable
   ```bash
   cd /path/to/DirShare
   make
   # Verify the executable exists
   ls -l dirshare
   ```

3. **Python 3.8+**: Required for Robot Framework
   ```bash
   python3 --version  # Should be 3.8 or later
   ```

### Setup Python Virtual Environment and Install Robot Framework

It's recommended to use a virtual environment to isolate dependencies:

```bash
# Navigate to the robot directory
cd /path/to/DirShare/robot

# Create a virtual environment (if not already created)
python3 -m venv venv

# Activate the virtual environment
source venv/bin/activate  # On macOS/Linux
# OR
venv\Scripts\activate     # On Windows

# Install Robot Framework and dependencies
python3 -m pip install -r requirements.txt

# Verify installation
robot --version
```

**Note**: When you're done with testing, you can deactivate the virtual environment:
```bash
deactivate
```

## Test Structure

```
DirShare/                          # Root directory
├── dirshare                       # DirShare executable (built by make)
├── rtps.ini                       # RTPS configuration file
└── robot/                         # Test directory
    ├── README.md                  # This file
    ├── requirements.txt           # Python dependencies
    ├── venv/                      # Python virtual environment (created)
    ├── UserStories.robot          # User story acceptance tests (US1-US6)
    ├── PerformanceTests.robot     # Success criteria tests (SC-001 to SC-011)
    ├── EdgeCaseTests.robot        # Edge case scenarios
    ├── DirShareAcceptance.robot   # Master test suite
    ├── keywords/                  # Custom Robot keywords
    │   ├── DirShareKeywords.robot # DirShare process management
    │   ├── FileOperations.robot   # File system operations
    │   └── DDSKeywords.robot      # DDS-specific operations
    ├── libraries/                 # Python keyword libraries
    │   ├── DirShareLibrary.py     # DirShare control library (portable)
    │   └── ChecksumLibrary.py     # File checksum utilities
    ├── resources/                 # Test resources
    │   ├── test_files/            # Sample files for testing
    │   └── config/                # Test configuration files
    └── results/                   # Test execution results (gitignored)
```

**Note on Portability**: The `DirShareLibrary.py` uses portable relative paths based on its own location. It will automatically find the `dirshare` executable two directories up (`../../dirshare` from the library file), making it work regardless of where the DirShare directory is located in your filesystem.

## Running Tests

**IMPORTANT**: Always activate the virtual environment before running tests:

```bash
cd /path/to/DirShare/robot
source venv/bin/activate  # Activate virtual environment first!
```

### Run All Tests

```bash
# From the robot directory with venv activated
robot UserStories.robot
```

### Run Specific Test Suite

```bash
# User Story tests only
robot UserStories.robot

# Performance/Success Criteria tests (when available)
robot PerformanceTests.robot

# Edge case tests (when available)
robot EdgeCaseTests.robot
```

### Run Specific Test Case

```bash
# Run only US1 tests
robot --test "US1*" UserStories.robot

# Run only initial sync scenario
robot --test "US1 Scenario 1*" UserStories.robot
```

### Run with Specific Discovery Mode

```bash
# Run with InfoRepo (default)
robot --variable DISCOVERY_MODE:inforepo UserStories.robot

# Run with RTPS
robot --variable DISCOVERY_MODE:rtps UserStories.robot
```

### Run with Debug Output

```bash
# Show all log messages
robot --loglevel DEBUG UserStories.robot

# Save detailed output
robot --outputdir results --loglevel TRACE UserStories.robot
```

## Test Reports

After running tests, Robot Framework generates:

- `report.html` - High-level test execution report
- `log.html` - Detailed test execution log with keywords and messages
- `output.xml` - Machine-readable test results (for CI/CD)

To view reports:
```bash
open report.html  # macOS
xdg-open report.html  # Linux
```

## Test Coverage

### User Story Tests (UserStories.robot)

- **US1: Initial Directory Synchronization**
  - Scenario 1: Empty directories converge when populated
  - Scenario 2: Pre-populated directories merge on startup
  - Scenario 3: Three participants synchronize all files

- **US2: Real-Time File Creation Propagation**
  - Scenario 1: New file appears on remote within 5 seconds
  - Scenario 2: Multiple files created in succession
  - Scenario 3: Large file (>10MB) transfers correctly

- **US3: Real-Time File Modification Propagation**
  - Scenario 1: Modified file propagates within 5 seconds
  - Scenario 2: Only modified file is transferred (efficiency)
  - Scenario 3: Timestamp-based conflict resolution

- **US4: Real-Time File Deletion Propagation** (future)
- **US5: Concurrent Modification Conflict Resolution** (future)
- **US6: Metadata Transfer and Preservation** (future)

### Performance/Success Criteria Tests (PerformanceTests.robot)

- **SC-001**: Initial sync of 100 files within 30 seconds
- **SC-002**: File creation propagation within 5 seconds
- **SC-003**: File modification propagation within 5 seconds
- **SC-004**: Bandwidth efficiency (80% reduction measurement)
- **SC-006**: 10+ simultaneous participants performance
- **SC-011**: Notification loop prevention (zero duplicate notifications)

### Edge Case Tests (EdgeCaseTests.robot)

- Network connection loss during file transfer
- Insufficient disk space scenario
- File locked/in-use scenario
- Unicode and special characters in filenames
- Symbolic links (should be ignored)
- File permissions

## Writing New Tests

### Basic Test Case Structure

```robot
*** Test Cases ***
My Test Case Name
    [Documentation]    Description of what this test validates
    [Tags]    us1    acceptance
    Given Participant A Has File "test.txt" With Content "Hello"
    And Participant B Has Empty Directory
    When DirShare Starts On Both Participants
    Then Participant B Should Have File "test.txt" Within 5 Seconds
    And File Content Should Match On Both Participants
```

### Using Keywords

```robot
*** Keywords ***
My Custom Keyword
    [Arguments]    ${arg1}    ${arg2}
    [Documentation]    Description of keyword
    Log    Executing keyword with ${arg1} and ${arg2}
    Some Other Keyword    ${arg1}
    RETURN    ${result}
```

### Best Practices

1. **Use Given-When-Then BDD style** for test cases
2. **Keep test cases at acceptance level** (not implementation details)
3. **Use descriptive test names** matching spec.md scenarios
4. **Tag tests appropriately** (us1, us2, performance, edge-case, etc.)
5. **Clean up after tests** (delete temp directories, kill processes)
6. **Make tests independent** (no dependencies between test cases)
7. **Use timeouts** for operations that should complete quickly

## Troubleshooting

### Issue: Tests Fail with "DirShare executable not found"

**Solution**: Ensure DirShare is built
```bash
cd /path/to/DirShare
make
ls -l dirshare  # Verify executable exists in the DirShare root directory
```

The DirShare executable should be at `/path/to/DirShare/dirshare` (in the same directory as the source files).

**How it works**: The test library (`DirShareLibrary.py`) automatically finds the executable using a portable relative path:
- Library location: `DirShare/robot/libraries/DirShareLibrary.py`
- Executable location: `DirShare/dirshare` (two directories up from the library)
- This works regardless of where you install DirShare in your filesystem

If you see an error message showing all searched locations, the most likely cause is that the executable hasn't been built yet. Run `make` in the DirShare root directory.

### Issue: Tests Fail with "DCPSInfoRepo not found" (InfoRepo mode)

**Solution**: Ensure OpenDDS environment is sourced
```bash
source $DDS_ROOT/setenv.sh
which DCPSInfoRepo  # Should find it
```

### Issue: Tests Hang or Timeout

**Possible Causes**:
- DDS participants not discovering each other
- Firewall blocking multicast (RTPS mode)
- DirShare processes not starting correctly

**Debug Steps**:
```bash
# Enable DDS debug output
export DCPS_debug_level=4

# Run single test with verbose output
robot --loglevel DEBUG --test "US1 Scenario 1*" UserStories.robot

# Check for DirShare processes
ps aux | grep dirshare

# Check log files in results/ directory
cat results/log.html
```

### Issue: File Synchronization Not Working

**Solution**: Check DirShare logs in test results directory
```bash
# Look for error messages
grep ERROR results/*dirshare*.log

# Verify DDS discovery
grep "publication matched\|subscription matched" results/*dirshare*.log
```

## CI/CD Integration

### Run Tests in Headless Mode

```bash
# Generate xunit-compatible output for CI systems
robot --xunit xunit.xml --outputdir results UserStories.robot
```

### Run Tests with Coverage

```bash
# Run all tests and save detailed results
robot --outputdir results --loglevel TRACE --report report.html --log log.html .
```

### Exit Codes

- **0**: All tests passed
- **1-249**: Number of failed test cases
- **250**: 250 or more test failures
- **251**: Help or version information requested
- **252**: Invalid test data or command line options
- **253**: Test execution stopped by user

## References

- [Robot Framework User Guide](https://robotframework.org/robotframework/latest/RobotFrameworkUserGuide.html)
- [Robot Framework Process Library](https://robotframework.org/robotframework/latest/libraries/Process.html)
- [Robot Framework OperatingSystem Library](https://robotframework.org/robotframework/latest/libraries/OperatingSystem.html)
- [DirShare Specification](../../../specs/001-dirshare/spec.md)
- [DirShare Tasks](../../../specs/001-dirshare/tasks.md)

## Typical Workflow

```bash
# 1. One-time setup (if not already done)
cd /path/to/DirShare/robot
python3 -m venv venv
source venv/bin/activate
python3 -m pip install -r requirements.txt
deactivate

# 2. Before each test session
source $DDS_ROOT/setenv.sh         # Source OpenDDS environment
cd /path/to/DirShare
make                                # Rebuild DirShare if needed
cd robot
source venv/bin/activate            # Activate Python virtual environment

# 3. Run tests
robot UserStories.robot
# OR run specific tests
robot --test "US1*" UserStories.robot
robot --variable DISCOVERY_MODE:rtps UserStories.robot

# 4. View results
open report.html

# 5. When finished
deactivate                          # Deactivate virtual environment
```

## Version History

- **v1.1** (2025-11-11): Updated documentation with accurate paths and setup instructions
  - Added Quick Start guide
  - Added Python virtual environment setup instructions
  - Updated paths to reflect actual DirShare project structure
  - Added typical workflow section
  - Improved troubleshooting with accurate paths
- **v1.0** (2025-10-31): Initial Robot Framework test infrastructure
  - Basic test structure and setup
  - US1-US3 acceptance tests
  - DirShare process management keywords
  - File operation keywords
