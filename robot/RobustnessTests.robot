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

Library          libraries/DirShareLibrary.py
Library          resources/process_manager.py
Library          resources/sync_verifier.py
Library          resources/test_files.py
Library          resources/config.py
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
${RTPS_CONFIG}          rtps.ini
${SINGLE_TIMEOUT}       10    # Seconds for single participant restart (US1)
${MULTIPLE_TIMEOUT}     30    # Seconds for multiple/rolling restarts (US2, US3)
${LARGE_FILE_TIMEOUT}   60    # Seconds for large file sender restart (US4.3)
${RECEIVER_TIMEOUT}     15    # Seconds for receiver restart during large file (US4.1, US4.2)

# Participant directories (set during test setup)
${DIR_A}                ${EMPTY}
${DIR_B}                ${EMPTY}
${DIR_C}                ${EMPTY}

*** Test Cases ***
# Test cases will be implemented in Phase 3-6
# This is the skeleton structure for the robustness test suite

Placeholder Test
    [Documentation]    Placeholder test to verify test suite structure
    [Tags]    setup
    Log    Robustness test suite skeleton created
    Log    Ready for Phase 3 implementation (User Story tests)

*** Keywords ***
Setup Robustness Test Suite
    [Documentation]    Initialize the test suite (runs once before all tests)
    Log    ═══════════════════════════════════════════════════════════
    Log    Starting DirShare Robustness Test Suite
    Log    Feature: 002-robustness-testing
    Log    ═══════════════════════════════════════════════════════════

    # Verify DirShare executable exists
    ${dirshare_exists}=    Run Keyword And Return Status    File Should Exist    ./dirshare
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
