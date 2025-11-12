# Data Model: Multi-Participant Robustness Testing

**Feature**: 002-robustness-testing | **Date**: 2025-11-12

## Overview

This document defines the test entities, their attributes, relationships, and state transitions for the robustness test suite. Unlike application data models, this describes the test harness's internal representation of participants, operations, and synchronization state.

## Core Entities

### Test Participant

Represents a single DirShare process instance under test control.

**Attributes**:
- `label` (string): Unique identifier (A, B, C, etc.)
- `process_id` (int): OS process ID from subprocess.Popen
- `directory` (Path): Absolute path to monitored directory
- `config_file` (Path): Path to DDS configuration (typically rtps.ini)
- `state` (ParticipantState): Current lifecycle state
- `stdout_log` (Path): Captured stdout output file
- `stderr_log` (Path): Captured stderr output file
- `start_time` (datetime): When participant was started
- `stop_time` (datetime | None): When participant was stopped (if applicable)

**Relationships**:
- Monitors one `TestDirectory`
- Produces zero or more `FileOperations` (implicitly via DirShare FileMonitor)
- Participates in zero or more `SyncAssertions`

**State Transitions**:
```
[NOT_STARTED] --start()--> [RUNNING] --shutdown()--> [STOPPED]
                                      --kill()--> [KILLED]
                                      --crash()--> [CRASHED]
[STOPPED] --restart()--> [RUNNING]
[KILLED] --restart()--> [RUNNING]
[CRASHED] (terminal state, test fails)
```

**Validation Rules**:
- Label must be unique within test case
- Directory must exist before participant starts
- Process ID must be valid OS PID while in RUNNING state
- Cannot restart participant in CRASHED state

### Test Directory

Represents a filesystem directory monitored by a participant.

**Attributes**:
- `path` (Path): Absolute path to directory
- `owner_label` (string): Which Test Participant monitors this directory
- `files` (Dict[str, FileState]): Mapping of relative paths to file states

**Relationships**:
- Monitored by exactly one `Test Participant`
- Contains zero or more `FileStates`

**Operations**:
- `create_file(name, size_mb, content_seed)`: Create test file with predictable content
- `modify_file(name, new_content)`: Update existing file
- `delete_file(name)`: Remove file
- `snapshot()`: Capture current {filename: (size, checksum, mtime)} mapping

**Validation Rules**:
- Path must be absolute
- Directory must be empty at test start (clean state requirement)
- Must be deleted/cleaned up at test end

### File State

Represents the expected state of a file at a specific point in time.

**Attributes**:
- `relative_path` (string): Path relative to test directory (e.g., "test.txt", "subdir/file.dat")
- `size` (int): File size in bytes
- `checksum` (int): CRC32 checksum (32-bit unsigned integer)
- `mtime` (float): Modification timestamp (Unix epoch seconds)
- `operation` (OperationType): How this file was created/modified

**Relationships**:
- Exists within one `Test Directory`
- Created by one `File Operation`

**Validation Rules**:
- Relative path must not start with "/" or contain ".."
- Size must be >= 0
- Checksum must be 32-bit unsigned integer (matches DirShare Checksum.cpp)

### File Operation

Represents a file system operation performed during test execution.

**Attributes**:
- `operation_type` (OperationType): CREATE | MODIFY | DELETE
- `participant_label` (string): Which participant's directory was modified
- `relative_path` (string): File path relative to participant directory
- `timestamp` (datetime): When operation was performed
- `size_mb` (int | None): File size for CREATE/MODIFY operations
- `content_seed` (int | None): Seed for reproducible content generation

**Relationships**:
- Performed in context of one `Test Participant`
- May trigger multiple `DDS Events` (FileEvent, FileContent, FileChunk)

**OperationType Enum**:
```python
class OperationType(Enum):
    CREATE = "CREATE"
    MODIFY = "MODIFY"
    DELETE = "DELETE"
```

**Sequencing**:
Operations are ordered by timestamp to validate DDS event ordering and last-write-wins semantics.

### Synchronization State

Represents the collective state of all participants' directories at a checkpoint.

**Attributes**:
- `timestamp` (datetime): When snapshot was taken
- `participant_states` (Dict[str, DirectorySnapshot]): Mapping of participant label to directory state
- `is_consistent` (bool): Whether all participants have identical directory contents
- `differences` (List[str]): Human-readable list of inconsistencies (empty if consistent)

**DirectorySnapshot** (nested structure):
```python
@dataclass
class DirectorySnapshot:
    participant_label: str
    files: Dict[str, Tuple[int, int, float]]  # {rel_path: (size, checksum, mtime)}
    file_count: int
    total_size: int
```

**Operations**:
- `capture(participants)`: Take snapshot of all participant directories
- `compare()`: Identify differences between participant states
- `wait_for_consistency(timeout)`: Poll until all participants synchronized

**Validation Rules**:
- All participant_states must have matching file sets for is_consistent=True
- Checksums must match for files with same name
- File count and total size derived from files dict

### Shutdown Event

Represents a controlled participant termination for testing restart scenarios.

**Attributes**:
- `participant_label` (string): Which participant was shut down
- `timestamp` (datetime): When shutdown was initiated
- `shutdown_type` (ShutdownType): GRACEFUL | KILL | CRASH
- `exit_code` (int | None): Process exit code (0 for graceful, non-zero for crash)
- `duration_seconds` (float): How long participant was offline before restart

**ShutdownType Enum**:
```python
class ShutdownType(Enum):
    GRACEFUL = "SIGTERM"   # Controlled shutdown via SIGTERM
    KILL = "SIGKILL"       # Force kill via SIGKILL
    CRASH = "CRASH"        # Simulated crash (unexpected termination)
```

**Relationships**:
- Targets one `Test Participant`
- Paired with one `Restart Event` (if restart occurs)

**Validation Rules**:
- Graceful shutdown must complete within 5 seconds
- Exit code 0 required for GRACEFUL shutdown type
- Cannot shutdown participant not in RUNNING state

### Restart Event

Represents participant reinitialization after shutdown.

**Attributes**:
- `participant_label` (string): Which participant was restarted
- `timestamp` (datetime): When restart was initiated
- `paired_shutdown` (ShutdownEvent): The shutdown event this restart follows
- `expected_sync_time` (int): Expected seconds to achieve consistency (10/30/60)
- `actual_sync_time` (float | None): Measured seconds to consistency (None if still pending/failed)
- `received_snapshot` (bool): Whether DirectorySnapshot was received (from DDS)
- `received_events` (int): Count of FileEvents received during catch-up

**Relationships**:
- Targets one `Test Participant`
- Follows one `Shutdown Event`
- May trigger one `DirectorySnapshot` DDS message reception
- May trigger multiple `FileEvent` DDS message receptions

**Validation Rules**:
- Participant must be in STOPPED or KILLED state to restart
- expected_sync_time must be 10, 30, or 60 seconds (per specification FR-017)
- actual_sync_time should be <= expected_sync_time (if not, timing violation)

### Sync Assertion

Represents a test assertion that all participants have consistent directory state.

**Attributes**:
- `test_case` (string): Name of test case (e.g., "US1.1")
- `assertion_time` (datetime): When assertion was evaluated
- `participants` (List[string]): Labels of participants being compared
- `timeout_seconds` (int): Maximum wait time (10/30/60)
- `result` (AssertionResult): PASS | FAIL | TIMEOUT
- `actual_consistency_time` (float | None): How long it took to synchronize
- `final_state` (SynchronizationState): Directory state when assertion completed

**AssertionResult Enum**:
```python
class AssertionResult(Enum):
    PASS = "PASS"          # Consistency achieved within timeout
    FAIL = "FAIL"          # Inconsistency detected after timeout
    TIMEOUT = "TIMEOUT"    # Timeout reached, consistency unknown
```

**Validation Rules**:
- All participants in list must be in RUNNING state
- timeout_seconds must match scenario type (10/30/60 per FR-017)
- PASS result requires is_consistent=True in final_state
- actual_consistency_time must be <= timeout_seconds for PASS

## Entity Relationships Diagram

```
Test Participant (1) ----monitors----> (1) Test Directory
      |                                       |
      |                                       | contains
      | produces                              |
      v                                       v
File Operation  ---------------creates---> File State (N)
      |
      | grouped in
      v
File Operation Sequence
      |
      | validated by
      v
Sync Assertion --references--> Synchronization State
      |                                |
      | evaluated on                   | compares
      |                                |
      +--------< Test Participant (N) >--------+

Shutdown Event (1) ---targets---> (1) Test Participant
      |
      | paired with
      v
Restart Event (1) ---targets---> (1) Test Participant
```

## State Transition Sequences

### Single Participant Restart (US1.1)

```
1. Initial State: 3 participants (A, B, C) in RUNNING state
2. SyncAssertion(A, B, C) -> PASS (baseline consistency)
3. ShutdownEvent(B, GRACEFUL) -> B transitions to STOPPED
4. FileOperation(A, CREATE, "test1.txt")
5. FileOperation(C, CREATE, "test2.txt")
6. RestartEvent(B, expected_sync=10s)
7. B transitions to RUNNING
8. B receives DirectorySnapshot (received_snapshot=True)
9. SyncAssertion(A, B, C, timeout=10s) -> PASS
   actual_consistency_time recorded
```

### Concurrent Deletions (US2.2)

```
1. Initial State: 3 participants (A, B, C) synchronized with "test.txt"
2. ShutdownEvent(A, GRACEFUL) -> A transitions to STOPPED
3. FileOperation(B, MODIFY, "test.txt")
4. FileOperation(C, DELETE, "test.txt")  [last operation]
5. RestartEvent(A, expected_sync=10s)
6. A transitions to RUNNING
7. SyncAssertion(A, B, C, timeout=10s) -> PASS
8. Verify "test.txt" is absent in all directories (last-write-wins: DELETE)
```

### Large File Sender Restart (US4.3)

```
1. Initial State: 3 participants (A, B, C) synchronized
2. FileOperation(A, CREATE, "large.dat", size=25MB)
3. Wait 1s (allow partial chunk transfer)
4. ShutdownEvent(A, GRACEFUL) -> A transitions to STOPPED
5. B and C have partial file (some chunks received)
6. RestartEvent(A, expected_sync=60s)
7. A re-publishes DirectorySnapshot
8. B and C request remaining chunks
9. SyncAssertion(A, B, C, timeout=60s) -> PASS
10. Verify all have "large.dat" with matching checksum
```

## Data Validation Patterns

### Checksum Consistency

All files with the same relative_path across participants must have:
- Identical size
- Identical CRC32 checksum (matches DirShare Checksum.cpp implementation)
- Modification time within 5 seconds (per assumptions)

### Event Ordering

FileOperations are totally ordered by timestamp. When conflicts occur:
- Last operation wins (by timestamp)
- DELETE beats MODIFY (if close timestamps)
- CREATE/MODIFY with newer timestamp overwrites previous content

### Timeout Compliance

All SyncAssertions must complete within specified timeouts:
- Single restart: 10 seconds (FR-017)
- Multiple/rolling restart: 30 seconds (FR-017)
- Sender restart during large file: 60 seconds (FR-017)

Violations indicate potential synchronization bugs or environmental issues.

## Test Data Generation

### Reproducible File Content

Files generated with `content_seed` parameter:
```python
chunk = bytes([i % 256 for i in range(seed, seed + 1024)])
```

Same seed produces same content -> predictable checksums for validation.

### Size Categories

Per specification acceptance scenarios:
- Small: 5MB (US1.2)
- Medium: 15MB (US4.1)
- Large: 20MB (US4.2)
- Extra Large: 25MB (US4.3)

### File Naming Conventions

- Simple tests: `test.txt`, `file1.txt`, `file2.txt`
- Large file tests: `large_15mb.dat`, `large_25mb.dat`
- Multiple file tests: `file_001.txt` through `file_100.txt`

## Summary

This data model defines test harness entities, not application data structures. The DirShare application's IDL-defined types (FileMetadata, FileEvent, FileContent, FileChunk, DirectorySnapshot) remain unchanged. This model describes how the Robot Framework test suite represents participants, operations, and synchronization state for validation purposes.
