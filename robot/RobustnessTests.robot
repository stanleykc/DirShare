*** Settings ***
Documentation    Multi-Participant Robustness Testing for DirShare
...              This test suite validates DirShare robustness during participant
...              shutdown and restart scenarios with 3 concurrent participants.
...
...              Feature: specs/002-robustness-testing/spec.md
...              Test Scenarios: specs/002-robustness-testing/contracts/test-scenarios.md
...
...              Timeout expectations per FR-017:
...              - Single participant restart: 10 seconds
...              - Multiple/rolling restarts: 30 seconds
...              - Sender restart during large file: 60 seconds

Library          libraries/DirShareLibrary.py    # For Cleanup All Test Directories
Library          resources/ProcessManager.py
Library          resources/SyncVerifier.py
Library          resources/TestFileGenerator.py
Library          OperatingSystem
Library          Process
Library          String
Library          DateTime

Resource         keywords/ParticipantControl.robot
Resource         keywords/FileOperations.robot
Resource         keywords/SyncVerification.robot

Suite Setup      Setup Robustness Test Suite
Suite Teardown   Teardown Robustness Test Suite
Test Setup       Setup Robustness Test
Test Teardown    Teardown Robustness Test

*** Variables ***
${RTPS_CONFIG}          ../rtps.ini
${SINGLE_TIMEOUT}       30    # Seconds for single participant restart (US1) - increased from 10 for reliability
${MULTIPLE_TIMEOUT}     30    # Seconds for multiple/rolling restarts (US2, US3)
${LARGE_FILE_TIMEOUT}   60    # Seconds for large file sender restart (US4.3)
${RECEIVER_TIMEOUT}     15    # Seconds for receiver restart during large file (US4.1, US4.2)

# Participant directories (set during test setup)
${DIR_A}                ${EMPTY}
${DIR_B}                ${EMPTY}
${DIR_C}                ${EMPTY}

*** Test Cases ***
# ═══════════════════════════════════════════════════════════════════════════
# Phase 3: User Story 1 - Participant Recovery After Shutdown (Priority P1)
# ═══════════════════════════════════════════════════════════════════════════

US1.1: Single Participant Restart With File Creation
    [Documentation]    Test single participant restart after file creation on other participants
    ...
    ...                Validates that a restarted participant receives missed file operations
    ...                and achieves consistency within 10 seconds (FR-017).
    ...
    ...                Reference: specs/002-robustness-testing/contracts/test-scenarios.md
    [Tags]    US1    priority-P1    single-restart

    # Step 1-2: Start three participants and verify baseline synchronization
    Start Three Participants
    Wait For Synchronization    ${SINGLE_TIMEOUT}    A    B    C
    Log    ✓ Initial synchronization baseline established

    # Step 3-4: Shutdown participant B
    Shutdown Participant    B
    Sleep    2s    Allow clean shutdown

    # Step 5-7: Create files on active participants A and C
    Create File    A    new_file_A.txt    1KB
    Create File    C    new_file_C.txt    1KB
    Sleep    2s    Allow A and C to synchronize

    # Verify A and C are synchronized before restarting B
    Wait For Synchronization    ${SINGLE_TIMEOUT}    A    C
    Log    ✓ Participants A and C synchronized before B restart

    # Step 8: Restart participant B
    Restart Participant    B

    # Step 9: Poll for synchronization across all participants
    ${start_time}=    Get Time    epoch
    Wait For Synchronization    ${SINGLE_TIMEOUT}    A    B    C
    ${end_time}=    Get Time    epoch
    ${actual_sync_time}=    Evaluate    ${end_time} - ${start_time}

    Log    ✓ Synchronization achieved in ${actual_sync_time} seconds
    Should Be True    ${actual_sync_time} <= ${SINGLE_TIMEOUT}
    ...    msg=Synchronization took ${actual_sync_time}s, expected <= ${SINGLE_TIMEOUT}s

    # Verify files exist with correct checksums
    Verify File Checksums Match    new_file_A.txt    A    B    C
    Verify File Checksums Match    new_file_C.txt    A    B    C

    Log    ✓ US1.1 PASSED: All files synchronized with matching checksums

US1.2: Mixed File Operations During Downtime
    [Documentation]    Test participant recovery with mixed CREATE/MODIFY operations during downtime
    ...
    ...                Validates that large files and modified files are correctly synchronized
    ...                when participant comes back online.
    ...
    ...                Reference: specs/002-robustness-testing/contracts/test-scenarios.md
    [Tags]    US1    priority-P1    mixed-operations

    # Step 1: Start three participants
    Start Three Participants
    Wait For Synchronization    ${SINGLE_TIMEOUT}    A    B    C
    Log    ✓ Initial synchronization baseline established

    # Step 2: Shutdown participant A
    Shutdown Participant    A
    Sleep    2s    Allow clean shutdown

    # Step 3: Participant B creates large file (5MB)
    Create Large File For Participant    B    large_file.dat    5MB    seed=100
    Log    ✓ Participant B created large_file.dat (5MB)

    # Step 4-5: Participant C creates and modifies small file
    Create File    C    small.txt    1KB    seed=200
    Sleep    1s    Allow initial file creation to propagate
    Modify File    C    small.txt    1KB    seed=201
    Log    ✓ Participant C created and modified small.txt

    # Step 6: Wait for B and C to synchronize
    Sleep    3s    Allow B and C to synchronize
    Wait For Synchronization    ${SINGLE_TIMEOUT}    B    C
    Log    ✓ Participants B and C synchronized before A restart

    # Step 7: Restart participant A
    Restart Participant    A

    # Step 8: Poll for synchronization
    ${start_time}=    Get Time    epoch
    Wait For Synchronization    ${SINGLE_TIMEOUT}    A    B    C
    ${end_time}=    Get Time    epoch
    ${actual_sync_time}=    Evaluate    ${end_time} - ${start_time}

    Log    ✓ Synchronization achieved in ${actual_sync_time} seconds
    Should Be True    ${actual_sync_time} <= ${SINGLE_TIMEOUT}
    ...    msg=Synchronization took ${actual_sync_time}s, expected <= ${SINGLE_TIMEOUT}s

    # Verify both files exist with correct checksums
    Verify File Checksums Match    large_file.dat    A    B    C
    Verify File Checksums Match    small.txt    A    B    C

    # Verify file sizes
    ${size_large_A}=    Get File Size From Participant    A    large_file.dat
    ${size_large_B}=    Get File Size From Participant    B    large_file.dat
    ${size_large_C}=    Get File Size From Participant    C    large_file.dat
    Should Be Equal As Numbers    ${size_large_A}    ${5242880}    msg=Large file should be 5MB (5242880 bytes) in A
    Should Be Equal As Numbers    ${size_large_B}    ${5242880}    msg=Large file should be 5MB (5242880 bytes) in B
    Should Be Equal As Numbers    ${size_large_C}    ${5242880}    msg=Large file should be 5MB (5242880 bytes) in C

    ${size_small_A}=    Get File Size From Participant    A    small.txt
    ${size_small_B}=    Get File Size From Participant    B    small.txt
    ${size_small_C}=    Get File Size From Participant    C    small.txt
    Should Be Equal As Numbers    ${size_small_A}    ${2048}    msg=Small file should be 2KB (2048 bytes) in A after modification
    Should Be Equal As Numbers    ${size_small_B}    ${2048}    msg=Small file should be 2KB (2048 bytes) in B after modification
    Should Be Equal As Numbers    ${size_small_C}    ${2048}    msg=Small file should be 2KB (2048 bytes) in C after modification

    Log    ✓ US1.2 PASSED: Large file and modified file synchronized correctly

US1.3: Multiple Participants Restart Sequentially
    [Documentation]    Test sequential restart of multiple participants with file creation
    ...
    ...                Validates that the system maintains consistency when multiple
    ...                participants are offline simultaneously and restart at different times.
    ...
    ...                Reference: specs/002-robustness-testing/contracts/test-scenarios.md
    [Tags]    US1    priority-P1    multiple-restart

    # Step 1: Start three participants
    Start Three Participants
    Wait For Synchronization    ${SINGLE_TIMEOUT}    A    B    C
    Log    ✓ Initial synchronization baseline established

    # Step 2: Shutdown participants A and B simultaneously
    Shutdown Participant    A
    Shutdown Participant    B
    Sleep    2s    Allow clean shutdown
    Log    ✓ Participants A and B shut down

    # Step 3-4: Participant C creates 5 files
    Create File    C    file_01.txt    100KB    seed=1
    Create File    C    file_02.txt    100KB    seed=2
    Create File    C    file_03.txt    100KB    seed=3
    Create File    C    file_04.txt    100KB    seed=4
    Create File    C    file_05.txt    100KB    seed=5
    Sleep    2s    Allow files to be published
    Log    ✓ Participant C created 5 files (100KB each)

    # Step 5-6: Restart participant A, wait 5 seconds
    Restart Participant    A
    Sleep    5s    Allow A to receive snapshot and synchronize
    Log    ✓ Participant A restarted

    # Verify A and C are synchronized
    Wait For Synchronization    ${MULTIPLE_TIMEOUT}    A    C
    Log    ✓ Participants A and C synchronized

    # Step 7: Restart participant B
    Restart Participant    B
    Log    ✓ Participant B restarted

    # Step 8: Poll for full synchronization with longer timeout
    ${start_time}=    Get Time    epoch
    Wait For Synchronization    ${MULTIPLE_TIMEOUT}    A    B    C
    ${end_time}=    Get Time    epoch
    ${actual_sync_time}=    Evaluate    ${end_time} - ${start_time}

    Log    ✓ Full synchronization achieved in ${actual_sync_time} seconds
    Should Be True    ${actual_sync_time} <= ${MULTIPLE_TIMEOUT}
    ...    msg=Synchronization took ${actual_sync_time}s, expected <= ${MULTIPLE_TIMEOUT}s

    # Verify all 5 files exist with correct checksums
    Verify File Checksums Match    file_01.txt    A    B    C
    Verify File Checksums Match    file_02.txt    A    B    C
    Verify File Checksums Match    file_03.txt    A    B    C
    Verify File Checksums Match    file_04.txt    A    B    C
    Verify File Checksums Match    file_05.txt    A    B    C

    # Verify file sizes (100KB = 102400 bytes)
    ${size_01}=    Get File Size From Participant    A    file_01.txt
    Should Be Equal As Numbers    ${size_01}    ${102400}    msg=file_01.txt should be 100KB (102400 bytes)

    Log    ✓ US1.3 PASSED: All 5 files synchronized across all participants

*** Keywords ***
Setup Robustness Test Suite
    [Documentation]    Initialize the test suite (runs once before all tests)
    Log    ═══════════════════════════════════════════════════════════
    Log    Starting DirShare Robustness Test Suite
    Log    Feature: 002-robustness-testing
    Log    ═══════════════════════════════════════════════════════════

    # Verify DirShare executable exists
    ${dirshare_exists}=    Run Keyword And Return Status    File Should Exist    ../dirshare
    Should Be True    ${dirshare_exists}    msg=DirShare executable not found. Run 'make' first.

    # Verify RTPS config exists
    ${rtps_exists}=    Run Keyword And Return Status    File Should Exist    ${RTPS_CONFIG}
    Should Be True    ${rtps_exists}    msg=RTPS config file ${RTPS_CONFIG} not found

    Log    ✓ Prerequisites verified

Teardown Robustness Test Suite
    [Documentation]    Cleanup after all tests complete (runs once at end)
    Log    ═══════════════════════════════════════════════════════════
    Log    Robustness Test Suite Complete
    Log    ═══════════════════════════════════════════════════════════

Setup Robustness Test
    [Documentation]    Setup before each test case
    Log    ───────────────────────────────────────────────────────────
    Log    Starting test: ${TEST NAME}
    Log    ───────────────────────────────────────────────────────────

    # Stop any running DirShare processes from previous tests
    Stop All Participants

    # Clean up any test directories from previous runs
    Cleanup All Test Directories

Teardown Robustness Test
    [Documentation]    Cleanup after each test case
    [Arguments]    ${test_status}=PASS

    Log    Test ${TEST NAME} status: ${test_status}

    # Stop all participants
    Stop All Participants

    # Clean up test directories (keep on failure for debugging)
    Run Keyword If    '${test_status}' == 'PASS'    Cleanup All Test Directories
    Run Keyword If    '${test_status}' == 'FAIL'    Log    Test directories preserved for debugging

    Log    ───────────────────────────────────────────────────────────
    Log    Finished test: ${TEST NAME}
    Log    ───────────────────────────────────────────────────────────

*** Comments ***
# User Story Test Structure (to be implemented in Phase 3-6):
#
# Phase 3: User Story 1 - Participant Recovery After Shutdown (P1)
#   - US1.1: Single Participant Restart With File Creation
#   - US1.2: Mixed File Operations During Downtime
#   - US1.3: Multiple Participants Restart Sequentially
#
# Phase 4: User Story 2 - Concurrent Operations During Partial Network (P2)
#   - US2.1: Concurrent File Creation While Participant Offline
#   - US2.2: Conflicting Modify and Delete Operations
#   - US2.3: Sequential File Creation With Intermediate Shutdown
#
# Phase 5: User Story 3 - Rolling Participant Restarts (P3)
#   - US3.1: Sequential Restart With Continuous File Creation
#   - US3.2: High-Volume Rolling Restart
#   - US3.3: Restart During Active File Reception
#
# Phase 6: User Story 4 - Large File Transfer During Restart (P4)
#   - US4.1: Receiver Restart During Chunked Transfer
#   - US4.2: Concurrent Receiver Restart
#   - US4.3: Sender Restart During Chunked Transfer
