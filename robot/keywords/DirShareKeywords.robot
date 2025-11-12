*** Settings ***
Documentation    Keywords for managing DirShare processes and participants
Library          ../libraries/DirShareLibrary.py
Library          OperatingSystem
Library          Process
Library          String
Resource         SyncVerification.robot    # For wait keywords

*** Keywords ***
Setup Test Environment
    [Documentation]    Initialize test environment and clean up any previous state
    Stop All DirShare Processes
    Cleanup All Test Directories

Teardown Test Environment
    [Documentation]    Clean up test environment after test execution
    Stop All DirShare Processes
    Cleanup All Test Directories

Create Participant Directory
    [Documentation]    Create a test directory for a participant
    [Arguments]    ${participant_name}
    ${dir}=    Create Test Directory    ${participant_name}
    Log    Created directory for participant ${participant_name}: ${dir}
    RETURN    ${dir}

Get Participant Directory
    [Documentation]    Get the test directory for a participant
    [Arguments]    ${participant_name}
    ${dir}=    Get Test Directory    ${participant_name}
    RETURN    ${dir}

Start DirShare With InfoRepo
    [Documentation]    Start DirShare for a participant using InfoRepo discovery
    [Arguments]    ${participant_name}    ${shared_dir}
    Log    Starting DirShare for ${participant_name} with InfoRepo mode
    Start Dirshare Inforepo    ${participant_name}    ${shared_dir}
    Sleep    2s    reason=Wait for DirShare to initialize

Start DirShare With RTPS
    [Documentation]    Start DirShare for a participant using RTPS discovery
    [Arguments]    ${participant_name}    ${shared_dir}
    Log    Starting DirShare for ${participant_name} with RTPS mode
    Start Dirshare Rtps    ${participant_name}    ${shared_dir}
    Sleep    2s    reason=Wait for DirShare to initialize

Stop DirShare For Participant
    [Documentation]    Stop DirShare for a specific participant
    [Arguments]    ${participant_name}
    Log    Stopping DirShare for ${participant_name}
    Stop Dirshare    ${participant_name}

Stop All DirShare Processes
    [Documentation]    Stop all running DirShare processes
    Log    Stopping all DirShare processes
    Stop All Dirshare

DirShare Should Be Running
    [Documentation]    Verify that DirShare is running for a participant
    [Arguments]    ${participant_name}
    ${is_running}=    Dirshare Is Running    ${participant_name}
    Should Be True    ${is_running}    msg=DirShare is not running for ${participant_name}

DirShare Should Not Be Running
    [Documentation]    Verify that DirShare is not running for a participant
    [Arguments]    ${participant_name}
    ${is_running}=    Dirshare Is Running    ${participant_name}
    Should Not Be True    ${is_running}    msg=DirShare should not be running for ${participant_name}

Start Participant With InfoRepo
    [Documentation]    Create directory and start DirShare with InfoRepo for a participant
    [Arguments]    ${participant_name}
    ${dir}=    Create Participant Directory    ${participant_name}
    Start DirShare With InfoRepo    ${participant_name}    ${dir}
    RETURN    ${dir}

Start Participant With RTPS
    [Documentation]    Create directory and start DirShare with RTPS for a participant
    [Arguments]    ${participant_name}
    ${dir}=    Create Participant Directory    ${participant_name}
    Start DirShare With RTPS    ${participant_name}    ${dir}
    RETURN    ${dir}

Verify Sync Complete
    [Documentation]    Verify that synchronization has completed successfully
    [Arguments]    ${participant1}    ${participant2}    ${filename}
    ${dir1}=    Get Participant Directory    ${participant1}
    ${dir2}=    Get Participant Directory    ${participant2}
    ${file1}=    Set Variable    ${dir1}/${filename}
    ${file2}=    Set Variable    ${dir2}/${filename}
    File Should Exist    ${file1}    msg=File ${filename} should exist in ${participant1}
    File Should Exist    ${file2}    msg=File ${filename} should exist in ${participant2}
    Log    Synchronization complete for ${filename}
