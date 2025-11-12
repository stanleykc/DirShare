# Test Scenarios Contract: Multi-Participant Robustness Testing

**Feature**: 002-robustness-testing | **Date**: 2025-11-12

## Overview

This document defines the test scenarios, their preconditions, actions, and expected outcomes for the robustness test suite. Each scenario maps directly to acceptance criteria from the feature specification.

## Scenario Naming Convention

Format: `US[story].[scenario]: [brief description]`

Example: `US1.1: Single Participant Restart With File Creation`

## User Story 1: Participant Recovery After Shutdown (Priority P1)

### US1.1: Single Participant Restart With File Creation

**Preconditions**:
- Three participants (A, B, C) running and synchronized
- All directories empty initially
- RTPS discovery mode

**Test Actions**:
1. Start participants A, B, C with separate directories
2. Verify initial synchronization (baseline)
3. Shutdown participant B (SIGTERM)
4. Wait 2 seconds for clean shutdown
5. Create `new_file_A.txt` (1KB) in participant A's directory
6. Create `new_file_C.txt` (1KB) in participant C's directory
7. Wait 2 seconds (allow A and C to synchronize)
8. Restart participant B
9. Poll directories every 1 second for synchronization

**Expected Outcomes**:
- Participant B receives DirectorySnapshot from A or C
- Within 10 seconds of restart, all three directories contain:
  - `new_file_A.txt` with correct checksum
  - `new_file_C.txt` with correct checksum
- All files have matching sizes and checksums across participants
- Test execution time < 30 seconds total

**Timeout**: 10 seconds (FR-017: single participant restart)

**Success Criteria**: SC-001, SC-002

---

### US1.2: Mixed File Operations During Downtime

**Preconditions**:
- Three participants (A, B, C) running and synchronized
- All directories empty initially

**Test Actions**:
1. Start participants A, B, C
2. Shutdown participant A (SIGTERM)
3. Participant B creates `large_file.dat` (5MB, seed=100)
4. Participant C creates `small.txt` (1KB, seed=200)
5. Participant C modifies `small.txt` (append 1KB, seed=201)
6. Wait 3 seconds (allow B and C to sync)
7. Restart participant A
8. Poll for synchronization

**Expected Outcomes**:
- Within 10 seconds, participant A has:
  - `large_file.dat` (5MB) with checksum matching B and C
  - `small.txt` (2KB) with checksum matching C's final version
- Modification timestamp on `small.txt` reflects C's modification time
- All participants synchronized

**Timeout**: 10 seconds

**Success Criteria**: SC-001, SC-002, SC-003

---

### US1.3: Multiple Participants Restart Sequentially

**Preconditions**:
- Three participants (A, B, C) running and synchronized

**Test Actions**:
1. Start participants A, B, C
2. Shutdown participants A and B simultaneously (SIGTERM)
3. Participant C creates 5 files: `file_01.txt` through `file_05.txt` (each 100KB)
4. Wait 2 seconds
5. Restart participant A
6. Wait 5 seconds
7. Restart participant B
8. Poll for synchronization

**Expected Outcomes**:
- Within 30 seconds after B restarts, all three participants have identical directories
- All 5 files present with matching checksums
- Files received in creation order (validated by sequence numbers if logged)

**Timeout**: 30 seconds (FR-017: multiple restart)

**Success Criteria**: SC-001, SC-002, SC-005

---

## User Story 2: Concurrent Operations During Partial Network (Priority P2)

### US2.1: Concurrent File Creation While Participant Offline

**Preconditions**:
- Three participants (A, B, C) running and synchronized

**Test Actions**:
1. Start participants A, B, C
2. Shutdown participant A (SIGTERM)
3. Participant B creates `file1.txt` (500KB, seed=42)
4. Participant C creates `file2.txt` (500KB, seed=43)
5. Shutdown participant B (SIGTERM)
6. Wait 2 seconds
7. Restart participant A
8. Wait 5 seconds
9. Restart participant B
10. Poll for synchronization

**Expected Outcomes**:
- Within 30 seconds after final restart, all participants have:
  - `file1.txt` with checksum matching B's original
  - `file2.txt` with checksum matching C's original
- Files visible on all three participants
- Directory consistency achieved

**Timeout**: 30 seconds

**Success Criteria**: SC-002, SC-005, SC-008

---

### US2.2: Conflicting Modify and Delete Operations

**Preconditions**:
- Three participants (A, B, C) synchronized with `test.txt` present

**Test Actions**:
1. Start participants A, B, C
2. Create `test.txt` (1KB, seed=100) on participant A
3. Wait 5 seconds (allow full synchronization)
4. Verify all have `test.txt`
5. Shutdown participant A (SIGTERM)
6. Participant B modifies `test.txt` (append 1KB, seed=101) - timestamp T1
7. Wait 100ms
8. Participant C deletes `test.txt` - timestamp T2 (T2 > T1)
9. Wait 3 seconds (B and C resolve conflict - DELETE wins)
10. Restart participant A
11. Poll for synchronization

**Expected Outcomes**:
- Within 10 seconds, `test.txt` is absent from all three directories
- Last operation (DELETE at T2) wins over earlier MODIFY (T1)
- No residual file fragments

**Timeout**: 10 seconds

**Success Criteria**: SC-002, SC-006, SC-008

**Note**: This validates DDS event ordering and last-write-wins semantics

---

### US2.3: Sequential File Creation With Intermediate Shutdown

**Preconditions**:
- Three participants (A, B, C) running

**Test Actions**:
1. Start participants A, B, C
2. Shutdown participant B (SIGTERM)
3. Participant A creates 10 files sequentially:
   - `seq_01.txt` through `seq_10.txt` (each 100KB, seeds 1-10)
   - 500ms delay between each creation
4. Wait 2 seconds after last file
5. Shutdown participant A (SIGTERM)
6. Restart participant B
7. Poll for synchronization

**Expected Outcomes**:
- Within 30 seconds, participant B receives all 10 files
- Files have correct checksums (matching seed-based content)
- File order preserved (if DDS FIFO ordering maintained)
- Participant C also has all 10 files (was online entire time)

**Timeout**: 30 seconds

**Success Criteria**: SC-002, SC-005, SC-008

---

## User Story 3: Rolling Participant Restarts (Priority P3)

### US3.1: Sequential Restart With Continuous File Creation

**Preconditions**:
- Three participants (A, B, C) running and synchronized

**Test Actions**:
1. Start participants A, B, C
2. Create `initial.txt` on A, wait for sync
3. Shutdown participant A (SIGTERM)
4. Wait 2 seconds
5. Restart participant A
6. Create `after_A_restart.txt` on B
7. Wait 2 seconds
8. Shutdown participant B (SIGTERM)
9. Wait 2 seconds
10. Restart participant B
11. Create `after_B_restart.txt` on C
12. Wait 2 seconds
13. Shutdown participant C (SIGTERM)
14. Wait 2 seconds
15. Restart participant C
16. Wait for final synchronization

**Expected Outcomes**:
- After all restarts complete, all participants have:
  - `initial.txt`
  - `after_A_restart.txt`
  - `after_B_restart.txt`
- All checksums match
- Total time < 60 seconds

**Timeout**: 30 seconds (after last restart)

**Success Criteria**: SC-002, SC-005, SC-008

---

### US3.2: High-Volume Rolling Restart

**Preconditions**:
- Three participants (A, B, C) running

**Test Actions**:
1. Start participants A, B, C
2. **Background file creation thread**: Create 20 files over 60 seconds
   - Distributed across A, B, C (round-robin)
   - Files named `bulk_01.txt` through `bulk_20.txt` (each 50KB)
3. **Restart sequence** (parallel with file creation):
   - T+10s: Restart A (shutdown, wait 3s, start)
   - T+25s: Restart B
   - T+40s: Restart C
4. After all restarts and file creation complete, wait for synchronization

**Expected Outcomes**:
- Within 30 seconds after last restart, all 20 files present on all participants
- No files lost during restart windows
- All checksums match
- Total test time < 120 seconds

**Timeout**: 30 seconds (post-restart synchronization)

**Success Criteria**: SC-002, SC-005, SC-006, SC-008

**Implementation Note**: Requires background thread/process for continuous file creation

---

### US3.3: Restart During Active File Reception

**Preconditions**:
- Three participants (A, B, C) running

**Test Actions**:
1. Start participants A, B, C
2. Participants B and C create 5 files each in rapid succession (total 10 files, 200KB each)
3. **Simultaneously**: Restart participant A (shutdown + immediate restart)
4. Poll for synchronization

**Expected Outcomes**:
- Participant A receives both:
  - DirectorySnapshot (baseline state)
  - Real-time FileEvents (for files created during restart)
- No duplicate files
- Within 30 seconds, all participants have all 10 files with correct checksums
- No data loss despite mid-flight events

**Timeout**: 30 seconds

**Success Criteria**: SC-002, SC-008 (tests snapshot + event deduplication)

---

## User Story 4: Large File Transfer During Participant Restart (Priority P4)

### US4.1: Receiver Restart During Chunked Transfer

**Preconditions**:
- Three participants (A, B, C) running

**Test Actions**:
1. Start participants A, B, C
2. Participant A creates `large_15mb.dat` (15MB, seed=500)
3. Monitor participant B's directory size
4. When B's file size reaches ~7.5MB (50% of chunks received):
   - Shutdown participant B (SIGTERM)
5. Wait 3 seconds
6. Restart participant B
7. Poll for synchronization

**Expected Outcomes**:
- Within 15 seconds of restart, participant B has complete `large_15mb.dat`
- Checksum matches A and C
- DirShare resumes chunk transfer (doesn't retransmit all chunks)
- File size exactly 15MB

**Timeout**: 15 seconds (FR-017: receiver restart during large file)

**Success Criteria**: SC-003, SC-008

**Implementation Note**: Requires monitoring file size to trigger shutdown at 50% threshold

---

### US4.2: Concurrent Receiver Restart

**Preconditions**:
- Three participants (A, B, C) running

**Test Actions**:
1. Start participants A, B, C
2. Participant A creates `large_20mb.dat` (20MB, seed=600)
3. Monitor participants B and C
4. When C reaches ~10MB received:
   - Shutdown participant C (SIGTERM)
   - Wait 2 seconds
   - Restart participant C
5. Wait for synchronization

**Expected Outcomes**:
- Participant C receives complete file after restart
- Participant B receives complete file without restart (baseline)
- Both B and C have matching checksums
- Within 15 seconds of C's restart, all synchronized

**Timeout**: 15 seconds

**Success Criteria**: SC-003, SC-008

---

### US4.3: Sender Restart During Chunked Transfer

**Preconditions**:
- Three participants (A, B, C) running

**Test Actions**:
1. Start participants A, B, C
2. Participant A creates `large_25mb.dat` (25MB, seed=700)
3. Wait 1 second (allow partial chunk transmission to B and C)
4. Shutdown participant A (SIGTERM) - **sender goes offline**
5. Wait 5 seconds (B and C have incomplete file)
6. Restart participant A
7. A re-publishes DirectorySnapshot with complete file
8. B and C request remaining chunks
9. Poll for synchronization

**Expected Outcomes**:
- Within 60 seconds of A's restart, B and C have complete `large_25mb.dat`
- Checksums match across all participants
- File transfer completes despite sender interruption
- No partial/corrupt files remain

**Timeout**: 60 seconds (FR-017: sender restart during large file)

**Success Criteria**: SC-003, SC-008

**Note**: This is the most complex scenario - tests DirectorySnapshot re-publication and chunk request recovery

---

## Test Execution Order

Recommended execution order (by priority and complexity):

1. **P1 scenarios** (US1.1, US1.2, US1.3) - Core functionality
2. **P2 scenarios** (US2.1, US2.2, US2.3) - Conflict resolution
3. **P3 scenarios** (US3.1, US3.2, US3.3) - Rolling restarts
4. **P4 scenarios** (US4.1, US4.2, US4.3) - Large file handling

Total scenarios: 12 (3+3+3+3)
Estimated total execution time: 10-12 minutes (within 15-minute budget)

## Common Test Patterns

### Pattern: Wait For Synchronization

```robot
*** Keywords ***
Wait For Synchronization
    [Arguments]    ${timeout}    @{participants}
    ${start_time}=    Get Time    epoch
    Wait Until Keyword Succeeds    ${timeout}    1s
    ...    Directories Are Synchronized    @{participants}
    ${end_time}=    Get Time    epoch
    ${actual_time}=    Evaluate    ${end_time} - ${start_time}
    Log    Synchronization achieved in ${actual_time} seconds
    Should Be True    ${actual_time} <= ${timeout}    Timeout exceeded
```

### Pattern: Graceful Participant Shutdown

```robot
*** Keywords ***
Shutdown Participant
    [Arguments]    ${label}
    Send SIGTERM To Participant    ${label}
    Wait For Process Exit    ${label}    timeout=5
    Verify Exit Code    ${label}    expected=0
    Log    Participant ${label} shutdown cleanly
```

### Pattern: Verify File Checksums

```robot
*** Keywords ***
Verify File Checksums Match
    [Arguments]    ${filename}    @{participants}
    ${checksums}=    Get File Checksums    ${filename}    @{participants}
    ${unique_checksums}=    Get Unique Values    ${checksums}
    ${count}=    Get Length    ${unique_checksums}
    Should Be Equal As Integers    ${count}    1
    ...    Checksum mismatch for ${filename}: ${checksums}
```

## Failure Scenarios (Negative Tests)

While the specification focuses on successful synchronization, the following failure modes should be validated:

1. **Timeout Violations**: Verify test fails if sync not achieved within timeout
2. **Checksum Mismatches**: Detect if files have different content
3. **Missing Files**: Catch when expected files not synchronized
4. **Process Crashes**: Handle unexpected participant termination

These are implicit in the "no retries" and "95% pass rate" requirements.

## Summary

This contract defines 12 test scenarios covering:
- 3 single participant restart scenarios (US1)
- 3 concurrent operation scenarios (US2)
- 3 rolling restart scenarios (US3)
- 3 large file transfer scenarios (US4)

All scenarios specify:
- Explicit preconditions
- Step-by-step actions
- Expected outcomes with timeouts (10s/30s/60s)
- Success criteria references

This contract serves as the implementation blueprint for `robot/RobustnessTests.robot`.
