# Research: Multi-Participant Robustness Testing

**Feature**: 002-robustness-testing | **Date**: 2025-11-12

## Overview

This document captures technology decisions and best practices research for implementing a Robot Framework test suite to validate DirShare multi-participant robustness.

## Technology Stack

### Robot Framework for DDS Application Testing

**Decision**: Use Robot Framework as the primary test automation framework

**Rationale**:
- **Readability**: Keyword-driven syntax makes test scenarios accessible to non-programmers
- **Process Management**: Python `psutil` library provides robust process lifecycle control (start, stop, kill)
- **Timing Control**: Built-in `Sleep` and `Wait Until Keyword Succeeds` keywords handle asynchronous validation
- **Reporting**: HTML/XML reports with execution logs for debugging timing-sensitive failures
- **Extensibility**: Python libraries can be imported as Robot Framework keywords
- **Existing Pattern**: DirShare already uses Robot Framework in `robot/UserStories.robot`

**Alternatives Considered**:
- **Perl Test Scripts** (`run_test.pl`): Traditional OpenDDS approach, but limited readability and harder to extend for complex orchestration
- **pytest with Python subprocess**: More flexible but loses keyword-driven readability benefit
- **Bash scripts**: Insufficient for complex timing coordination and validation logic

**Best Practices**:
- Organize keywords into resource files by responsibility (ParticipantControl, FileOperations, SyncVerification)
- Use Python libraries for complex logic (process management, directory comparison)
- Separate test data from test logic using variables and configuration files
- Implement retry logic at keyword level, not test level (aligns with "no automatic test retries" requirement)

### Process Management Strategy

**Decision**: Use Python `psutil` library wrapped in Robot Framework keywords

**Rationale**:
- **Cross-platform**: Works on macOS, Linux, Windows
- **Graceful Shutdown**: Supports SIGTERM for controlled participant shutdown
- **Process Monitoring**: Can detect if process crashes vs. intentional shutdown
- **Exit Code Validation**: Verify participants exit cleanly (status 0)
- **Resource Cleanup**: Ensures zombie processes are killed between tests

**Implementation Pattern**:
```python
# resources/process_manager.py
import psutil
import subprocess
import signal

class ProcessManager:
    def start_participant(self, label, directory, config_file):
        """Start DirShare participant with label and monitored directory"""
        cmd = ["./dirshare", "-DCPSConfigFile", config_file, directory]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.processes[label] = proc
        return proc.pid

    def shutdown_participant(self, label):
        """Send SIGTERM to participant for graceful shutdown"""
        proc = self.processes[label]
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=5)

    def kill_participant(self, label):
        """Force kill participant (for crash simulation)"""
        proc = self.processes[label]
        proc.kill()
```

**Alternatives Considered**:
- **subprocess.Popen without psutil**: Less robust process monitoring
- **os.system()**: No process handle for controlled shutdown
- **Docker containers**: Overhead too high for rapid startup/shutdown testing

### Directory Synchronization Verification

**Decision**: Implement recursive directory comparison with checksum validation

**Rationale**:
- **Eventual Consistency**: Must poll until all participants converge
- **Content Validation**: Not just file names, but checksums match (FR-010)
- **Timestamp Preservation**: Verify modification times are preserved (US1.2)
- **Deletion Detection**: Verify files deleted on one participant are removed from others

**Implementation Pattern**:
```python
# resources/sync_verifier.py
import os
import hashlib
from pathlib import Path

class SyncVerifier:
    def directories_are_synchronized(self, *directories):
        """Return True if all directories have identical files and checksums"""
        file_maps = [self._build_file_map(d) for d in directories]
        reference = file_maps[0]
        return all(self._maps_equal(reference, fm) for fm in file_maps[1:])

    def _build_file_map(self, directory):
        """Build {relative_path: (size, checksum, mtime)} mapping"""
        file_map = {}
        for filepath in Path(directory).rglob('*'):
            if filepath.is_file():
                rel_path = filepath.relative_to(directory)
                file_map[str(rel_path)] = (
                    filepath.stat().st_size,
                    self._checksum(filepath),
                    filepath.stat().st_mtime
                )
        return file_map

    def _checksum(self, filepath):
        """Calculate CRC32 checksum (matches DirShare Checksum.cpp)"""
        crc = 0
        with open(filepath, 'rb') as f:
            while chunk := f.read(8192):
                crc = zlib.crc32(chunk, crc)
        return crc & 0xFFFFFFFF
```

**Best Practices**:
- Use polling with timeout (10s/30s/60s per specification)
- Compare checksums, not just file sizes (detect silent corruption)
- Handle missing files gracefully (expected during transitional states)
- Log differences clearly for debugging failed assertions

### Timeout Strategy

**Decision**: Three-tier timeout strategy based on scenario complexity

**Rationale** (from specification clarifications):
- **10 seconds**: Single participant restart (US1.1, US1.2) - fastest scenario
- **30 seconds**: Multiple/rolling restarts (US1.3, US3.2) - more complex coordination
- **60 seconds**: Sender restart during large file transfer (US4.3) - includes discovery + chunk retransmission

**Implementation Pattern**:
```robot
*** Settings ***
Library    resources/sync_verifier.py

*** Keywords ***
Wait For Synchronization
    [Arguments]    ${timeout}    @{directories}
    Wait Until Keyword Succeeds    ${timeout}    1s
    ...    Directories Are Synchronized    @{directories}
```

**Best Practices**:
- Use `Wait Until Keyword Succeeds` for polling assertions
- Set retry interval to 1 second (balance responsiveness vs. system load)
- Fail fast if timeout exceeded (per "no retries" requirement)
- Log timing statistics to identify flaky scenarios

### Test Data Management

**Decision**: Programmatic test file generation with controlled sizes

**Rationale**:
- **Reproducibility**: Generate files with predictable content for checksum validation
- **Size Control**: Create exact file sizes (5MB, 15MB, 20MB, 25MB per specification)
- **Cleanup**: Remove test directories between tests for clean state
- **Performance**: Fast file generation doesn't contribute to 15-minute time budget

**Implementation Pattern**:
```python
# resources/test_files.py
import os

def create_test_file(directory, filename, size_mb, seed=42):
    """Create file with predictable content for given size"""
    filepath = os.path.join(directory, filename)
    chunk = bytes([i % 256 for i in range(seed, seed + 1024)])  # 1KB chunk
    with open(filepath, 'wb') as f:
        for _ in range(size_mb * 1024):
            f.write(chunk)
    return filepath
```

### Configuration Management

**Decision**: Centralized configuration file for test parameters

**Rationale**:
- **Single Source of Truth**: Timeouts, paths, sizes defined once
- **Easy Adjustment**: Modify timing for different environments without editing tests
- **Documentation**: Configuration file serves as specification reference

**Configuration Structure**:
```python
# resources/config.py
TIMEOUTS = {
    'single_restart': 10,        # seconds
    'multiple_restart': 30,
    'large_file_transfer': 60
}

FILE_SIZES = {
    'small': 5,                  # MB
    'medium': 15,
    'large': 20,
    'extra_large': 25
}

DIRSHARE_EXECUTABLE = './dirshare'
RTPS_CONFIG = 'rtps.ini'
TEST_DIR_BASE = '/tmp/dirshare_robustness_test'
```

## Integration Patterns

### Test Case Structure

**Best Practice**: One Robot Framework test file per user story

```robot
*** Settings ***
Library           resources/process_manager.py
Library           resources/sync_verifier.py
Library           resources/test_files.py
Resource          keywords/ParticipantControl.robot
Resource          keywords/FileOperations.robot
Resource          keywords/SyncVerification.robot

*** Test Cases ***
US1.1: Single Participant Restart With File Creation
    [Documentation]    Verify participant B synchronizes files created while offline
    [Tags]    US1    priority-P1

    Given Three Participants Are Running And Synchronized
    When Participant B Is Shut Down
    And Participant A Creates File "test1.txt"
    And Participant C Creates File "test2.txt"
    And Participant B Is Restarted
    Then All Participants Have Files "test1.txt" And "test2.txt" Within 10 Seconds
```

### Keyword Layering

**Pattern**: Three-layer keyword hierarchy
1. **Low-level keywords**: Direct library calls (Python functions)
2. **Mid-level keywords**: Composite operations (setup participant, verify sync)
3. **High-level keywords**: BDD-style test steps (Given/When/Then)

### Error Handling

**Strategy**: Fail fast with detailed diagnostics

```robot
*** Keywords ***
Shutdown Participant
    [Arguments]    ${label}
    ${success}=    Graceful Shutdown    ${label}    timeout=5
    Run Keyword If    not ${success}    Fail    Participant ${label} did not shutdown cleanly
    Log    Participant ${label} shutdown successfully
```

## Testing Best Practices

### Test Independence

- Each test creates fresh directories in `/tmp/dirshare_robustness_test_[timestamp]`
- Teardown kills all participant processes (even on test failure)
- No shared state between tests
- Tests can run in parallel (different directory namespaces)

### Timing Sensitivity

- Use `Wait Until Keyword Succeeds` instead of fixed `Sleep`
- Log actual synchronization time for statistical analysis
- Accept up to 5% flakiness (per specification SC-004)
- Run test suite 10 times consecutively to validate 95% pass rate

### Debugging Support

- Capture stdout/stderr from each participant to separate log files
- Include participant logs in Robot Framework HTML report
- Log directory state before/after each operation
- Preserve test directories on failure for manual inspection

## Open Questions

None - specification provides complete technical guidance.

## References

- Robot Framework User Guide: https://robotframework.org/robotframework/latest/RobotFrameworkUserGuide.html
- psutil Documentation: https://psutil.readthedocs.io/
- Existing DirShare Robot Tests: `robot/UserStories.robot`
- DirShare Architecture: `specs/001-dirshare/spec.md`, `specs/001-dirshare/plan.md`
