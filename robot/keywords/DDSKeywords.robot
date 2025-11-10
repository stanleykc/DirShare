*** Settings ***
Documentation    Keywords for DDS-specific operations and verification
Library          OperatingSystem
Library          String
Library          Process

*** Keywords ***
Verify DDS Discovery Complete
    [Documentation]    Verify that DDS participants have discovered each other
    [Arguments]    ${log_file}    ${expected_matches}=1
    ${log_content}=    Get File    ${log_file}
    Should Contain    ${log_content}    publication matched    msg=No publication matched found in ${log_file}
    Should Contain    ${log_content}    subscription matched    msg=No subscription matched found in ${log_file}
    Log    DDS discovery complete in ${log_file}

Verify No DDS Errors
    [Documentation]    Verify that there are no DDS errors in log file
    [Arguments]    ${log_file}
    ${log_content}=    Get File    ${log_file}
    Should Not Contain    ${log_content}    ERROR    msg=DDS errors found in ${log_file}
    Should Not Contain    ${log_content}    FAILED    msg=DDS failures found in ${log_file}
    Log    No DDS errors in ${log_file}

Check For DDS Warnings
    [Documentation]    Check if there are DDS warnings in log file (non-fatal)
    [Arguments]    ${log_file}
    ${log_content}=    Get File    ${log_file}
    ${has_warnings}=    Run Keyword And Return Status    Should Contain    ${log_content}    WARNING
    IF    ${has_warnings}
        Log    DDS warnings found in ${log_file}    level=WARN
    ELSE
        Log    No DDS warnings in ${log_file}
    END
    RETURN    ${has_warnings}

Verify DDS Cleanup
    [Documentation]    Verify that DDS resources were properly cleaned up
    [Arguments]    ${log_file}
    ${log_content}=    Get File    ${log_file}
    Should Contain    ${log_content}    shutdown    msg=DDS shutdown not found in ${log_file}
    Log    DDS cleanup verified in ${log_file}

Start With Discovery Mode
    [Documentation]    Start DirShare with specified discovery mode (inforepo or rtps)
    [Arguments]    ${participant_name}    ${shared_dir}    ${mode}=inforepo
    IF    '${mode}' == 'inforepo'
        Start DirShare With InfoRepo    ${participant_name}    ${shared_dir}
    ELSE IF    '${mode}' == 'rtps'
        Start DirShare With RTPS    ${participant_name}    ${shared_dir}
    ELSE
        Fail    Invalid discovery mode: ${mode}. Must be 'inforepo' or 'rtps'
    END
    Log    Started DirShare for ${participant_name} with ${mode} mode

Get DDS Log File
    [Documentation]    Get the path to DDS log file for a participant
    [Arguments]    ${participant_name}
    ${log_file}=    Set Variable    dirshare_${participant_name}.log
    RETURN    ${log_file}

Count FileEvent Messages In Log
    [Documentation]    Count the number of FileEvent messages in log file
    [Arguments]    ${log_file}    ${event_type}=CREATE
    ${log_content}=    Get File    ${log_file}
    ${matches}=    Get Regexp Matches    ${log_content}    FileEvent.*${event_type}
    ${count}=    Get Length    ${matches}
    Log    Found ${count} ${event_type} FileEvent messages in ${log_file}
    RETURN    ${count}

Verify No Duplicate Notifications
    [Documentation]    Verify that there are no duplicate FileEvent notifications (SC-011)
    [Arguments]    ${log_file}    ${filename}
    ${log_content}=    Get File    ${log_file}
    ${matches}=    Get Regexp Matches    ${log_content}    FileEvent.*${filename}
    ${count}=    Get Length    ${matches}
    Should Be Equal As Integers    ${count}    1    msg=Expected 1 FileEvent for ${filename}, found ${count} (duplicate notification)
    Log    No duplicate notifications for ${filename} in ${log_file}

Verify Notification Loop Prevention
    [Documentation]    Verify that notification loops are prevented (SC-011)
    [Arguments]    ${sender_log}    ${receiver_log}    ${filename}
    ${sender_content}=    Get File    ${sender_log}
    ${receiver_content}=    Get File    ${receiver_log}
    ${sender_events}=    Get Regexp Matches    ${sender_content}    FileEvent.*${filename}.*sent
    ${receiver_republish}=    Get Regexp Matches    ${receiver_content}    FileEvent.*${filename}.*sent
    ${sender_count}=    Get Length    ${sender_events}
    ${receiver_count}=    Get Length    ${receiver_republish}
    Should Be Equal As Integers    ${sender_count}    1    msg=Sender should send exactly 1 event
    Should Be Equal As Integers    ${receiver_count}    0    msg=Receiver should NOT republish received event (loop prevention)
    Log    Notification loop prevention verified for ${filename}

Wait For DDS Initialization
    [Documentation]    Wait for DDS to initialize (discovery, topic creation, etc.)
    [Arguments]    ${timeout}=5
    Log    Waiting ${timeout} seconds for DDS initialization
    Sleep    ${timeout}s

Verify Topic Creation
    [Documentation]    Verify that DDS topics were created successfully
    [Arguments]    ${log_file}
    ${log_content}=    Get File    ${log_file}
    Should Contain    ${log_content}    FileEvents    msg=FileEvents topic not found
    Should Contain    ${log_content}    FileContent    msg=FileContent topic not found
    Should Contain    ${log_content}    FileChunks    msg=FileChunks topic not found
    Should Contain    ${log_content}    DirectorySnapshot    msg=DirectorySnapshot topic not found
    Log    All DDS topics created successfully

Verify QoS Policy Application
    [Documentation]    Verify that QoS policies were applied correctly
    [Arguments]    ${log_file}    ${policy_name}
    ${log_content}=    Get File    ${log_file}
    Should Contain    ${log_content}    ${policy_name}    msg=QoS policy ${policy_name} not found in ${log_file}
    Log    QoS policy ${policy_name} verified in ${log_file}

Check For Checksum Errors
    [Documentation]    Check if there are checksum verification errors
    [Arguments]    ${log_file}
    ${log_content}=    Get File    ${log_file}
    ${has_errors}=    Run Keyword And Return Status    Should Contain    ${log_content}    checksum mismatch
    IF    ${has_errors}
        Log    Checksum errors found in ${log_file}    level=ERROR
        Fail    Checksum verification errors detected
    ELSE
        Log    No checksum errors in ${log_file}
    END

Verify File Transfer Complete
    [Documentation]    Verify that file transfer completed successfully in logs
    [Arguments]    ${log_file}    ${filename}
    ${log_content}=    Get File    ${log_file}
    ${transfer_complete}=    Run Keyword And Return Status
    ...    Should Contain    ${log_content}    ${filename}.*received
    Should Be True    ${transfer_complete}    msg=File transfer for ${filename} not found in ${log_file}
    Log    File transfer complete for ${filename} in ${log_file}

Measure Propagation Time
    [Documentation]    Measure the time taken for file propagation (for performance tests)
    [Arguments]    ${start_time}    ${end_time}
    ${duration}=    Subtract Date From Date    ${end_time}    ${start_time}
    Log    Propagation time: ${duration} seconds
    RETURN    ${duration}

Verify Propagation Within Timeout
    [Documentation]    Verify that propagation completed within expected timeout
    [Arguments]    ${duration}    ${max_timeout}=5
    ${duration_float}=    Convert To Number    ${duration}
    ${max_float}=    Convert To Number    ${max_timeout}
    Should Be True    ${duration_float} <= ${max_float}
    ...    msg=Propagation took ${duration}s, expected <= ${max_timeout}s
    Log    Propagation completed within ${max_timeout}s (actual: ${duration}s)
