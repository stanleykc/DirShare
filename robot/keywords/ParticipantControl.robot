*** Settings ***
Documentation    Keywords for controlling DirShare participants during robustness tests
Library          ../resources/process_manager.py
Library          ../libraries/DirShareLibrary.py
Library          OperatingSystem
Library          String

*** Keywords ***
Start Three Participants
    [Documentation]    Start three participants (A, B, C) with RTPS discovery
    [Arguments]    ${config_file}=rtps.ini

    # Create test directories for all participants
    ${dir_A}=    Create Test Directory    A
    ${dir_B}=    Create Test Directory    B
    ${dir_C}=    Create Test Directory    C

    # Start all three participants
    ${pid_A}=    Start Participant    A    ${dir_A}    ${config_file}
    ${pid_B}=    Start Participant    B    ${dir_B}    ${config_file}
    ${pid_C}=    Start Participant    C    ${dir_C}    ${config_file}

    Log    Started 3 participants: A (${pid_A}), B (${pid_B}), C (${pid_C})

    # Wait for DDS discovery
    Sleep    3s    reason=Wait for DDS discovery and initial synchronization

    RETURN    ${dir_A}    ${dir_B}    ${dir_C}

Start Participants
    [Documentation]    Start multiple participants with custom labels
    [Arguments]    ${config_file}=rtps.ini    @{labels}

    @{directories}=    Create List

    FOR    ${label}    IN    @{labels}
        ${dir}=    Create Test Directory    ${label}
        ${pid}=    Start Participant    ${label}    ${dir}    ${config_file}
        Log    Started participant ${label} (PID: ${pid}) in ${dir}
        Append To List    ${directories}    ${dir}
    END

    # Wait for DDS discovery
    Sleep    3s    reason=Wait for DDS discovery

    RETURN    @{directories}

Shutdown Participant
    [Documentation]    Gracefully shutdown a participant using SIGTERM
    [Arguments]    ${label}    ${timeout}=5

    ${success}=    Shutdown Participant    ${label}    ${timeout}
    Should Be True    ${success}    msg=Failed to shutdown participant ${label} within ${timeout} seconds
    Log    Participant ${label} shutdown successfully

Kill Participant
    [Documentation]    Force kill a participant using SIGKILL
    [Arguments]    ${label}

    ${success}=    Kill Participant    ${label}
    Should Be True    ${success}    msg=Failed to kill participant ${label}
    Log    Participant ${label} killed

Restart Participant
    [Documentation]    Restart a participant (shutdown then start)
    [Arguments]    ${label}    ${directory}=${None}    ${config_file}=rtps.ini

    Log    Restarting participant ${label}
    ${pid}=    Restart Participant    ${label}    ${directory}    ${config_file}
    Log    Participant ${label} restarted (PID: ${pid})

    # Wait for re-initialization and discovery
    Sleep    3s    reason=Wait for participant restart and DDS discovery

    RETURN    ${pid}

Participant Should Be Running
    [Documentation]    Verify that a participant is currently running
    [Arguments]    ${label}

    ${is_running}=    Is Running    ${label}
    Should Be True    ${is_running}    msg=Participant ${label} should be running but is not
    Log    Participant ${label} is running

Participant Should Not Be Running
    [Documentation]    Verify that a participant is not running
    [Arguments]    ${label}

    ${is_running}=    Is Running    ${label}
    Should Not Be True    ${is_running}    msg=Participant ${label} should not be running but is
    Log    Participant ${label} is not running

Stop All Participants
    [Documentation]    Stop all managed participants (for test cleanup)

    Log    Stopping all participants
    Cleanup All

Get Participant Directory
    [Documentation]    Get the test directory for a participant
    [Arguments]    ${label}

    ${dir}=    Get Test Directory    ${label}
    RETURN    ${dir}

Wait After Shutdown
    [Documentation]    Wait a specified time after participant shutdown
    [Arguments]    ${seconds}=2

    Log    Waiting ${seconds} seconds after shutdown
    Sleep    ${seconds}s

Wait After Restart
    [Documentation]    Wait for participant to stabilize after restart
    [Arguments]    ${seconds}=3

    Log    Waiting ${seconds} seconds after restart for stabilization
    Sleep    ${seconds}s
