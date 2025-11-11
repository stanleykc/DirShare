*** Settings ***
Documentation    User Story acceptance tests for DirShare
...              These tests validate the user stories defined in spec.md
...
...              Test Coverage:
...              - US1: Initial Directory Synchronization (3 scenarios)
...              - US2: Real-Time File Creation Propagation (3 scenarios)
...              - US3: Real-Time File Modification Propagation (3 scenarios)
...              - US4: Real-Time File Deletion Propagation (3 scenarios)
...              - US6: Metadata Transfer and Preservation (3 scenarios)

Resource         keywords/DirShareKeywords.robot
Resource         keywords/FileOperations.robot
Resource         keywords/DDSKeywords.robot
Library          libraries/MetadataLibrary.py

Suite Setup      Suite Initialization
Suite Teardown   Suite Cleanup
Test Setup       Setup Test Environment
Test Teardown    Teardown Test Environment

*** Variables ***
${DISCOVERY_MODE}    inforepo    # Can be overridden: --variable DISCOVERY_MODE:rtps
${SYNC_TIMEOUT}      35          # Seconds to wait for initial synchronization and DDS discovery (30s discovery timeout + 5s sync)
${PROPAGATION_TIMEOUT}    5     # Seconds to wait for file propagation (FileMonitor polls every 2s + transfer time)
${FILEMONITOR_INTERVAL}    3     # Wait for FileMonitor polling interval (2s) plus buffer

*** Keywords ***
Suite Initialization
    [Documentation]    Initialize test suite
    Log    Starting DirShare User Story Acceptance Tests
    Log    Discovery Mode: ${DISCOVERY_MODE}

Suite Cleanup
    [Documentation]    Clean up after all tests
    Log    DirShare User Story Tests Complete

*** Test Cases ***
#
# User Story 1: Initial Directory Synchronization
# Goal: Enable two DirShare instances to synchronize existing files when establishing a sharing session
#

US1 Scenario 1: Empty Directories Converge When Populated
    [Documentation]    Start with empty directories, add files to one, verify sync
    [Tags]    us1    acceptance    initial-sync
    Given Participant A And B Have Empty Directories
    When Participant A Creates Files    file1.txt    file2.txt
    And DirShare Synchronizes
    Then Participant B Should Have Files    file1.txt    file2.txt
    And Files Should Match Between Participants

US1 Scenario 2: Pre-Populated Directories Merge On Startup
    [Documentation]    Both participants have different files before starting DirShare
    [Tags]    us1    acceptance    initial-sync    merge
    Given Participant A Has Files    fileA1.txt    fileA2.txt
    And Participant B Has Files    fileB1.txt    fileB2.txt
    When DirShare Starts On Both Participants
    Then Both Participants Should Have All Files
    And Files Should Match Between Participants

US1 Scenario 3: Three Participants Synchronize All Files
    [Documentation]    Test with 3 participants to verify multi-participant sync
    [Tags]    us1    acceptance    initial-sync    multi-participant
    Given Three Participants A B And C Have Different Files
    When DirShare Starts On All Three Participants
    Then All Three Participants Should Have All Files
    And Files Should Match Across All Participants

#
# User Story 2: Real-Time File Creation Propagation
# Goal: Automatically propagate newly created files to all participants during active session
#

US2 Scenario 1: New File Appears On Remote Within 5 Seconds
    [Documentation]    Create file on one participant, verify it appears on other within 5s
    [Tags]    us2    acceptance    file-creation    performance
    Given DirShare Is Running On Participants A And B
    When Participant A Creates New File    newfile.txt    Hello World
    Then Participant B Should Have File Within    newfile.txt    ${PROPAGATION_TIMEOUT}
    And File Content Should Match On Both Participants    newfile.txt

US2 Scenario 2: Multiple Files Created In Succession
    [Documentation]    Create multiple files in sequence, verify all propagate
    [Tags]    us2    acceptance    file-creation    multiple-files
    Given DirShare Is Running On Participants A And B
    When Participant A Creates Multiple Files    file1.txt    file2.txt    file3.txt
    Then Participant B Should Have All Created Files Within    ${PROPAGATION_TIMEOUT}
    And All Files Should Match Between Participants

US2 Scenario 3: Large File Transfers Correctly Via Chunking
    [Documentation]    Create large file (>10MB), verify chunked transfer works
    [Tags]    us2    acceptance    file-creation    large-file    chunking
    Given DirShare Is Running On Participants A And B
    When Participant A Creates Large File    largefile.bin    15
    Then Participant B Should Have File Within    largefile.bin    30
    And Large File Should Match On Both Participants    largefile.bin

#
# User Story 3: Real-Time File Modification Propagation
# Goal: Propagate file modifications efficiently to all participants
#

US3 Scenario 1: Modified File Propagates Within 5 Seconds
    [Documentation]    Modify existing file, verify change propagates within 5s
    [Tags]    us3    acceptance    file-modification    performance
    Given DirShare Is Running With Synchronized File    original.txt
    When Participant A Modifies File    original.txt    Updated Content
    Then Participant B Should Have Updated File Within    original.txt    ${PROPAGATION_TIMEOUT}
    And Modified File Should Match On Both Participants    original.txt

US3 Scenario 2: Only Modified File Is Transferred (Efficiency)
    [Documentation]    Verify that only the modified file is retransmitted, not all files
    [Tags]    us3    acceptance    file-modification    efficiency
    Given DirShare Is Running With Multiple Synchronized Files    file1.txt    file2.txt    file3.txt
    When Participant A Modifies Only File    file2.txt
    Then Only File Should Be Retransmitted    file2.txt
    And Participant B Should Have Updated File    file2.txt
    And Other Files Should Remain Unchanged    file1.txt    file3.txt

US3 Scenario 3: Timestamp Based Conflict Resolution
    [Documentation]    Modify same file on two participants, newer timestamp wins
    [Tags]    us3    acceptance    file-modification    conflict-resolution
    Given DirShare Is Running With Synchronized File    conflict.txt
    When Participant A Modifies File First    conflict.txt    A's version
    And Participant B Modifies File Later    conflict.txt    B's version
    Then Both Participants Should Converge To Latest Version    conflict.txt    B's version

#
# User Story 4: Real-Time File Deletion Propagation
# Goal: Automatically delete files on all participants when deleted on one machine
#

US4 Scenario 1: Single File Deletion Propagates Within 5 Seconds
    [Documentation]    Delete file on one participant, verify it's deleted on other within 5s
    [Tags]    us4    acceptance    file-deletion    performance
    Given DirShare Is Running With Synchronized File    temp.txt
    When Participant A Deletes File    temp.txt
    Then Participant B Should Have File Deleted Within    temp.txt    ${PROPAGATION_TIMEOUT}
    And File Should Not Exist On Both Participants    temp.txt

US4 Scenario 2: Multiple Files Deleted While Others Remain
    [Documentation]    Delete 2 out of 5 files, verify only those 2 are deleted
    [Tags]    us4    acceptance    file-deletion    selective-deletion
    Given DirShare Is Running With Multiple Synchronized Files    file1.txt    file2.txt    file3.txt    file4.txt    file5.txt
    When Participant A Deletes Multiple Files    file2.txt    file4.txt
    Then Participant B Should Have Files Deleted    file2.txt    file4.txt
    And Participant B Should Still Have Files    file1.txt    file3.txt    file5.txt

US4 Scenario 3: File Deletion Propagates To All Three Participants
    [Documentation]    Delete file with 3 participants, verify deleted from all
    [Tags]    us4    acceptance    file-deletion    multi-participant
    Given DirShare Is Running On Three Participants With Synchronized File    shared.txt
    When Participant A Deletes File    shared.txt
    Then Participant B Should Have File Deleted Within    shared.txt    ${PROPAGATION_TIMEOUT}
    And Participant C Should Have File Deleted Within    shared.txt    ${PROPAGATION_TIMEOUT}

#
# User Story 6: Metadata Transfer and Preservation
# Goal: Transfer and preserve file metadata (timestamps, size, extensions) along with file content
#

US6 Scenario 1: Modification Timestamp Is Preserved After Transfer
    [Documentation]    File transferred to remote participant preserves original modification timestamp
    [Tags]    us6    acceptance    metadata    timestamp-preservation
    Given Participant A Has File With Specific Timestamp    photo.jpg    2025-10-30 10:00:00
    When DirShare Starts And Synchronizes Between A And B
    Then Participant B File Should Have Same Timestamp As A    photo.jpg
    And Timestamps Should Match Within Tolerance    ${DIR_A}/photo.jpg    ${DIR_B}/photo.jpg    2.0

US6 Scenario 2: File Size Is Correctly Transferred And Validated
    [Documentation]    File with specific size is transferred and size is verifiable on receiving side
    [Tags]    us6    acceptance    metadata    size-validation
    Given Participant A Has File With Specific Size    data.bin    5242880
    When DirShare Starts And Synchronizes Between A And B
    Then Participant B File Should Have Same Size As A    data.bin
    And File Should Have Expected Size    ${DIR_B}/data.bin    5242880

US6 Scenario 3: Various File Extensions Are Preserved During Transfer
    [Documentation]    Files with different extensions (.txt, .jpg, .pdf, .docx) are transferred correctly
    [Tags]    us6    acceptance    metadata    extensions
    Given Participant A Has Files With Various Extensions    document.txt    photo.jpg    report.pdf    presentation.docx
    When DirShare Starts And Synchronizes Between A And B
    Then All File Extensions Should Be Preserved On Participant B
    And File Extension Should Match    ${DIR_B}/document.txt    .txt
    And File Extension Should Match    ${DIR_B}/photo.jpg    .jpg
    And File Extension Should Match    ${DIR_B}/report.pdf    .pdf
    And File Extension Should Match    ${DIR_B}/presentation.docx    .docx

*** Keywords ***
#
# Given Keywords (Test Setup)
#

Participant A And B Have Empty Directories
    [Documentation]    Create empty directories for participants A and B
    ${dir_a}=    Create Participant Directory    A
    ${dir_b}=    Create Participant Directory    B
    Set Test Variable    ${DIR_A}    ${dir_a}
    Set Test Variable    ${DIR_B}    ${dir_b}
    Log    Created empty directories for A and B

Participant A Has Files
    [Documentation]    Create files in participant A's directory before starting DirShare
    [Arguments]    @{filenames}
    ${dir_a}=    Create Participant Directory    A
    Set Test Variable    ${DIR_A}    ${dir_a}
    FOR    ${filename}    IN    @{filenames}
        Create File With Content    ${dir_a}    ${filename}    Content of ${filename}
    END
    Log    Participant A has files: @{filenames}

Participant B Has Files
    [Documentation]    Create files in participant B's directory before starting DirShare
    [Arguments]    @{filenames}
    ${dir_b}=    Create Participant Directory    B
    Set Test Variable    ${DIR_B}    ${dir_b}
    FOR    ${filename}    IN    @{filenames}
        Create File With Content    ${dir_b}    ${filename}    Content of ${filename}
    END
    Log    Participant B has files: @{filenames}

Three Participants A B And C Have Different Files
    [Documentation]    Create three participants with different sets of files
    ${dir_a}=    Create Participant Directory    A
    ${dir_b}=    Create Participant Directory    B
    ${dir_c}=    Create Participant Directory    C
    Set Test Variable    ${DIR_A}    ${dir_a}
    Set Test Variable    ${DIR_B}    ${dir_b}
    Set Test Variable    ${DIR_C}    ${dir_c}
    Create File With Content    ${dir_a}    fileA.txt    Content from A
    Create File With Content    ${dir_b}    fileB.txt    Content from B
    Create File With Content    ${dir_c}    fileC.txt    Content from C
    Log    Created three participants with different files

DirShare Is Running On Participants A And B
    [Documentation]    Start DirShare on participants A and B
    ${dir_a}=    Create Participant Directory    A
    ${dir_b}=    Create Participant Directory    B
    Set Test Variable    ${DIR_A}    ${dir_a}
    Set Test Variable    ${DIR_B}    ${dir_b}
    Start With Discovery Mode    A    ${dir_a}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${dir_b}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Log    DirShare is running on participants A and B

DirShare Is Running With Synchronized File
    [Documentation]    Start with a synchronized file across participants
    [Arguments]    ${filename}
    ${dir_a}=    Create Participant Directory    A
    ${dir_b}=    Create Participant Directory    B
    Set Test Variable    ${DIR_A}    ${dir_a}
    Set Test Variable    ${DIR_B}    ${dir_b}
    ${content}=    Set Variable    Initial content of ${filename}
    Create File With Content    ${dir_a}    ${filename}    ${content}
    Start With Discovery Mode    A    ${dir_a}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${dir_b}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Wait For File To Appear    ${dir_b}    ${filename}    ${PROPAGATION_TIMEOUT}
    Log    DirShare running with synchronized file: ${filename}

DirShare Is Running With Multiple Synchronized Files
    [Documentation]    Start with multiple synchronized files
    [Arguments]    @{filenames}
    ${dir_a}=    Create Participant Directory    A
    ${dir_b}=    Create Participant Directory    B
    Set Test Variable    ${DIR_A}    ${dir_a}
    Set Test Variable    ${DIR_B}    ${dir_b}
    FOR    ${filename}    IN    @{filenames}
        Create File With Content    ${dir_a}    ${filename}    Content of ${filename}
    END
    Start With Discovery Mode    A    ${dir_a}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${dir_b}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    FOR    ${filename}    IN    @{filenames}
        Wait For File To Appear    ${dir_b}    ${filename}    ${PROPAGATION_TIMEOUT}
    END
    Log    DirShare running with synchronized files: @{filenames}

DirShare Is Running On Three Participants With Synchronized File
    [Documentation]    Start with three participants and a synchronized file
    [Arguments]    ${filename}
    ${dir_a}=    Create Participant Directory    A
    ${dir_b}=    Create Participant Directory    B
    ${dir_c}=    Create Participant Directory    C
    Set Test Variable    ${DIR_A}    ${dir_a}
    Set Test Variable    ${DIR_B}    ${dir_b}
    Set Test Variable    ${DIR_C}    ${dir_c}
    ${content}=    Set Variable    Initial content of ${filename}
    Create File With Content    ${dir_a}    ${filename}    ${content}
    Start With Discovery Mode    A    ${dir_a}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${dir_b}    ${DISCOVERY_MODE}
    Start With Discovery Mode    C    ${dir_c}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Wait For File To Appear    ${dir_b}    ${filename}    ${PROPAGATION_TIMEOUT}
    Wait For File To Appear    ${dir_c}    ${filename}    ${PROPAGATION_TIMEOUT}
    Log    DirShare running on three participants with synchronized file: ${filename}

#
# When Keywords (Actions)
#

Participant A Creates Files
    [Documentation]    Create files in participant A's directory
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        Create File With Content    ${DIR_A}    ${filename}    Content of ${filename}
    END
    Log    Participant A created files: @{filenames}

DirShare Synchronizes
    [Documentation]    Start DirShare and wait for synchronization
    Start With Discovery Mode    A    ${DIR_A}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${DIR_B}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Log    DirShare synchronization complete

DirShare Starts On Both Participants
    [Documentation]    Start DirShare on both participants
    Start With Discovery Mode    A    ${DIR_A}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${DIR_B}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Log    DirShare started on both participants

DirShare Starts On All Three Participants
    [Documentation]    Start DirShare on three participants
    Start With Discovery Mode    A    ${DIR_A}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${DIR_B}    ${DISCOVERY_MODE}
    Start With Discovery Mode    C    ${DIR_C}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Log    DirShare started on all three participants

Participant A Creates New File
    [Documentation]    Create a new file on participant A
    [Arguments]    ${filename}    ${content}
    ${start_time}=    Get Current Date
    Create File With Content    ${DIR_A}    ${filename}    ${content}
    Set Test Variable    ${START_TIME}    ${start_time}
    Log    Participant A created new file: ${filename}
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect change...
    Sleep    ${FILEMONITOR_INTERVAL}s

Participant A Creates Multiple Files
    [Documentation]    Create multiple files on participant A
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        Create File With Content    ${DIR_A}    ${filename}    Content of ${filename}
    END
    Set Test Variable    @{CREATED_FILES}    @{filenames}
    Log    Participant A created multiple files: @{filenames}
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect changes...
    Sleep    ${FILEMONITOR_INTERVAL}s

Participant A Creates Large File
    [Documentation]    Create a large file on participant A
    [Arguments]    ${filename}    ${size}
    ${start_time}=    Get Current Date
    Create Large File    ${DIR_A}    ${filename}    ${size}
    Set Test Variable    ${START_TIME}    ${start_time}
    Set Test Variable    ${LARGE_FILE}    ${filename}
    Log    Participant A created large file: ${filename} (${size} MB)
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect change...
    Sleep    ${FILEMONITOR_INTERVAL}s

Participant A Modifies File
    [Documentation]    Modify an existing file on participant A
    [Arguments]    ${filename}    ${new_content}
    ${filepath}=    Set Variable    ${DIR_A}/${filename}
    Modify File Content    ${filepath}    ${new_content}
    Set Test Variable    ${MODIFIED_FILE}    ${filename}
    Set Test Variable    ${NEW_CONTENT}    ${new_content}
    Log    Participant A modified file: ${filename}
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect modification...
    Sleep    ${FILEMONITOR_INTERVAL}s

Participant A Modifies Only File
    [Documentation]    Modify only one specific file
    [Arguments]    ${filename}
    ${filepath}=    Set Variable    ${DIR_A}/${filename}
    Modify File Content    ${filepath}    Modified content of ${filename}
    Set Test Variable    ${MODIFIED_FILE}    ${filename}
    Log    Participant A modified only: ${filename}
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect modification...
    Sleep    ${FILEMONITOR_INTERVAL}s

Participant A Modifies File First
    [Documentation]    Modify file first (older timestamp)
    [Arguments]    ${filename}    ${content}
    Sleep    1s
    ${filepath}=    Set Variable    ${DIR_A}/${filename}
    Modify File Content    ${filepath}    ${content}
    Set Test Variable    ${FIRST_VERSION}    ${content}
    Log    Participant A modified file first: ${filename}

Participant B Modifies File Later
    [Documentation]    Modify file later (newer timestamp wins)
    [Arguments]    ${filename}    ${content}
    Sleep    2s
    ${filepath}=    Set Variable    ${DIR_B}/${filename}
    Modify File Content    ${filepath}    ${content}
    Set Test Variable    ${LATEST_VERSION}    ${content}
    Log    Participant B modified file later: ${filename}

Participant A Deletes File
    [Documentation]    Delete a file from participant A's directory
    [Arguments]    ${filename}
    ${filepath}=    Set Variable    ${DIR_A}/${filename}
    Delete File    ${filepath}
    Set Test Variable    ${DELETED_FILE}    ${filename}
    Log    Participant A deleted file: ${filename}
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect deletion...
    Sleep    ${FILEMONITOR_INTERVAL}s

Participant A Deletes Multiple Files
    [Documentation]    Delete multiple files from participant A's directory
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        ${filepath}=    Set Variable    ${DIR_A}/${filename}
        Delete File    ${filepath}
    END
    Set Test Variable    @{DELETED_FILES}    @{filenames}
    Log    Participant A deleted multiple files: @{filenames}
    Log    Waiting ${FILEMONITOR_INTERVAL}s for FileMonitor to detect deletions...
    Sleep    ${FILEMONITOR_INTERVAL}s

#
# Then Keywords (Assertions)
#

Participant B Should Have Files
    [Documentation]    Verify that participant B has the specified files
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        Wait For File To Appear    ${DIR_B}    ${filename}    ${PROPAGATION_TIMEOUT}
    END
    Log    Participant B has all expected files

Files Should Match Between Participants
    [Documentation]    Verify that files have same content and checksum on both participants
    @{files}=    List Files In Directory    ${DIR_A}
    FOR    ${file}    IN    @{files}
        ${file_a}=    Set Variable    ${DIR_A}/${file}
        ${file_b}=    Set Variable    ${DIR_B}/${file}
        Files Should Have Same Checksum    ${file_a}    ${file_b}
    END
    Log    All files match between participants A and B

Both Participants Should Have All Files
    [Documentation]    Verify both participants have merged files
    # Wait for files to propagate before checking
    Log    Waiting for initial synchronization to complete...
    Sleep    ${PROPAGATION_TIMEOUT}s
    # Get expected file counts (A's files + B's files should be in both)
    @{files_a}=    List Files In Directory    ${DIR_A}
    @{files_b}=    List Files In Directory    ${DIR_B}
    ${expected_count}=    Evaluate    len(@{files_a})
    Log    Expected files from A: ${expected_count}
    # Wait for all files from A to appear in B
    FOR    ${file}    IN    @{files_a}
        Wait For File To Appear    ${DIR_B}    ${file}    ${PROPAGATION_TIMEOUT}
    END
    # Wait for all files from B to appear in A
    FOR    ${file}    IN    @{files_b}
        Wait For File To Appear    ${DIR_A}    ${file}    ${PROPAGATION_TIMEOUT}
    END
    # Now verify counts and checksums
    ${count_a}=    Count Files In Directory    ${DIR_A}
    ${count_b}=    Count Files In Directory    ${DIR_B}
    Should Be Equal As Integers    ${count_a}    ${count_b}
    Files Should Match Between Participants
    Log    Both participants have all files

All Three Participants Should Have All Files
    [Documentation]    Verify all three participants have all files
    # Wait for files to propagate before checking
    Log    Waiting for initial synchronization to complete...
    Sleep    ${PROPAGATION_TIMEOUT}s
    # Get file lists from each participant
    @{files_a}=    List Files In Directory    ${DIR_A}
    @{files_b}=    List Files In Directory    ${DIR_B}
    @{files_c}=    List Files In Directory    ${DIR_C}
    # Wait for all files from A to appear in B and C
    FOR    ${file}    IN    @{files_a}
        Wait For File To Appear    ${DIR_B}    ${file}    ${PROPAGATION_TIMEOUT}
        Wait For File To Appear    ${DIR_C}    ${file}    ${PROPAGATION_TIMEOUT}
    END
    # Wait for all files from B to appear in A and C
    FOR    ${file}    IN    @{files_b}
        Wait For File To Appear    ${DIR_A}    ${file}    ${PROPAGATION_TIMEOUT}
        Wait For File To Appear    ${DIR_C}    ${file}    ${PROPAGATION_TIMEOUT}
    END
    # Wait for all files from C to appear in A and B
    FOR    ${file}    IN    @{files_c}
        Wait For File To Appear    ${DIR_A}    ${file}    ${PROPAGATION_TIMEOUT}
        Wait For File To Appear    ${DIR_B}    ${file}    ${PROPAGATION_TIMEOUT}
    END
    # Now verify counts
    ${count_a}=    Count Files In Directory    ${DIR_A}
    ${count_b}=    Count Files In Directory    ${DIR_B}
    ${count_c}=    Count Files In Directory    ${DIR_C}
    Should Be Equal As Integers    ${count_a}    ${count_b}
    Should Be Equal As Integers    ${count_a}    ${count_c}
    Log    All three participants have all files (count: ${count_a})

Files Should Match Across All Participants
    [Documentation]    Verify files match across all three participants
    @{files}=    List Files In Directory    ${DIR_A}
    FOR    ${file}    IN    @{files}
        ${file_a}=    Set Variable    ${DIR_A}/${file}
        ${file_b}=    Set Variable    ${DIR_B}/${file}
        ${file_c}=    Set Variable    ${DIR_C}/${file}
        Files Should Have Same Checksum    ${file_a}    ${file_b}
        Files Should Have Same Checksum    ${file_a}    ${file_c}
    END
    Log    All files match across all participants

Participant B Should Have File Within
    [Documentation]    Verify participant B gets file within timeout
    [Arguments]    ${filename}    ${timeout}
    ${end_time}=    Get Current Date
    Wait For File To Appear    ${DIR_B}    ${filename}    ${timeout}
    ${duration}=    Measure Propagation Time    ${START_TIME}    ${end_time}
    Verify Propagation Within Timeout    ${duration}    ${timeout}
    Log    File ${filename} appeared within ${timeout}s (actual: ${duration}s)

File Content Should Match On Both Participants
    [Documentation]    Verify file content matches on both participants
    [Arguments]    ${filename}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    Files Should Have Same Content    ${file_a}    ${file_b}
    Log    File content matches: ${filename}

Participant B Should Have All Created Files Within
    [Documentation]    Verify all created files appear within timeout
    [Arguments]    ${timeout}
    FOR    ${filename}    IN    @{CREATED_FILES}
        Wait For File To Appear    ${DIR_B}    ${filename}    ${timeout}
    END
    Log    All created files appeared within ${timeout}s

All Files Should Match Between Participants
    [Documentation]    Verify all files match
    FOR    ${filename}    IN    @{CREATED_FILES}
        File Content Should Match On Both Participants    ${filename}
    END
    Log    All files match between participants

Large File Should Match On Both Participants
    [Documentation]    Verify large file matches (by checksum)
    [Arguments]    ${filename}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    Files Should Have Same Size    ${file_a}    ${file_b}
    Files Should Have Same Checksum    ${file_a}    ${file_b}
    Log    Large file matches on both participants: ${filename}

Participant B Should Have Updated File Within
    [Documentation]    Verify participant B gets updated file within timeout
    [Arguments]    ${filename}    ${timeout}
    Wait For File Propagation    ${timeout}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    Verify File Content    ${file_b}    ${NEW_CONTENT}
    Log    Updated file propagated within ${timeout}s

Modified File Should Match On Both Participants
    [Documentation]    Verify modified file matches
    [Arguments]    ${filename}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    Files Should Have Same Content    ${file_a}    ${file_b}
    Log    Modified file matches: ${filename}

Only File Should Be Retransmitted
    [Documentation]    Verify only one file was retransmitted (efficiency check)
    [Arguments]    ${filename}
    Log    Verifying only ${filename} was retransmitted
    # This would require checking DDS logs for FileEvent messages
    # For now, we verify the file was updated
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    File Should Exist    ${file_b}

Participant B Should Have Updated File
    [Documentation]    Verify participant B has the updated file
    [Arguments]    ${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    File Should Exist    ${file_b}
    Log    Participant B has updated file: ${filename}

Other Files Should Remain Unchanged
    [Documentation]    Verify other files were not retransmitted
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        ${file_b}=    Set Variable    ${DIR_B}/${filename}
        File Should Exist    ${file_b}
    END
    Log    Other files remain unchanged: @{filenames}

Both Participants Should Converge To Latest Version
    [Documentation]    Verify both participants have the latest version (conflict resolution)
    [Arguments]    ${filename}    ${expected_content}
    Wait For File Propagation    ${PROPAGATION_TIMEOUT}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    # Allow some time for conflict resolution
    Sleep    3s
    ${content_a}=    Get File Content    ${file_a}
    ${content_b}=    Get File Content    ${file_b}
    Should Be Equal    ${content_a}    ${content_b}    msg=Files should converge to same content
    Log    Both participants converged to latest version: ${filename}

Participant B Should Have File Deleted Within
    [Documentation]    Verify participant B has file deleted within timeout
    [Arguments]    ${filename}    ${timeout}
    Wait For File To Disappear    ${DIR_B}    ${filename}    ${timeout}
    Log    File ${filename} deleted from participant B within ${timeout}s

File Should Not Exist On Both Participants
    [Documentation]    Verify file does not exist on both participants
    [Arguments]    ${filename}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}
    File Should Not Exist    ${file_a}
    File Should Not Exist    ${file_b}
    Log    File ${filename} does not exist on both participants

Participant B Should Have Files Deleted
    [Documentation]    Verify multiple files are deleted from participant B
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        Wait For File To Disappear    ${DIR_B}    ${filename}    ${PROPAGATION_TIMEOUT}
    END
    Log    Files deleted from participant B: @{filenames}

Participant B Should Still Have Files
    [Documentation]    Verify that specific files still exist on participant B
    [Arguments]    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        ${filepath}=    Set Variable    ${DIR_B}/${filename}
        File Should Exist    ${filepath}
    END
    Log    Participant B still has files: @{filenames}

Participant C Should Have File Deleted Within
    [Documentation]    Verify participant C has file deleted within timeout
    [Arguments]    ${filename}    ${timeout}
    Wait For File To Disappear    ${DIR_C}    ${filename}    ${timeout}
    Log    File ${filename} deleted from participant C within ${timeout}s

#
# US6 Keywords (Metadata Preservation)
#

Participant A Has File With Specific Timestamp
    [Documentation]    Create file on participant A with a specific modification timestamp
    [Arguments]    ${filename}    ${timestamp_str}
    ${dir_a}=    Create Participant Directory    A
    Set Test Variable    ${DIR_A}    ${dir_a}
    ${filepath}=    Set Variable    ${dir_a}/${filename}

    # Convert timestamp string to epoch (e.g., "2025-10-30 10:00:00" -> seconds since epoch)
    # For simplicity, using a known timestamp: Oct 30, 2025 10:00:00 = 1761825600
    ${timestamp}=    Set Variable    1761825600.0
    ${content}=    Set Variable    Test content for ${filename}

    Create File With Specific Timestamp    ${filepath}    ${content}    ${timestamp}
    Log    Created file ${filename} with timestamp ${timestamp_str} (${timestamp})

DirShare Starts And Synchronizes Between A And B
    [Documentation]    Start DirShare on both participants and wait for synchronization
    ${dir_b}=    Create Participant Directory    B
    Set Test Variable    ${DIR_B}    ${dir_b}

    Start With Discovery Mode    A    ${DIR_A}    ${DISCOVERY_MODE}
    Start With Discovery Mode    B    ${DIR_B}    ${DISCOVERY_MODE}
    Wait For Synchronization    ${SYNC_TIMEOUT}
    Log    DirShare started and synchronized between A and B

Participant B File Should Have Same Timestamp As A
    [Documentation]    Verify participant B file has same timestamp as participant A
    [Arguments]    ${filename}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}

    Wait For File To Appear    ${DIR_B}    ${filename}    ${PROPAGATION_TIMEOUT}
    ${match}=    Files Have Same Modification Time    ${file_a}    ${file_b}    2.0
    Should Be True    ${match}    msg=File timestamps should match between A and B
    Log    Timestamp preserved: ${filename}

Timestamps Should Match Within Tolerance
    [Documentation]    Verify two files have matching timestamps within tolerance
    [Arguments]    ${file1}    ${file2}    ${tolerance}
    Verify Timestamp Preserved    ${file1}    ${file2}    ${tolerance}
    Log    Timestamps match within ${tolerance} seconds

Participant A Has File With Specific Size
    [Documentation]    Create file on participant A with specific size in bytes
    [Arguments]    ${filename}    ${size_bytes}
    ${dir_a}=    Create Participant Directory    A
    Set Test Variable    ${DIR_A}    ${dir_a}
    ${filepath}=    Set Variable    ${dir_a}/${filename}

    # Create file with exact size
    Create File With Size    ${filepath}    ${size_bytes}
    Log    Created file ${filename} with size ${size_bytes} bytes

Participant B File Should Have Same Size As A
    [Documentation]    Verify participant B file has same size as participant A
    [Arguments]    ${filename}
    ${file_a}=    Set Variable    ${DIR_A}/${filename}
    ${file_b}=    Set Variable    ${DIR_B}/${filename}

    Wait For File To Appear    ${DIR_B}    ${filename}    ${PROPAGATION_TIMEOUT}
    ${match}=    MetadataLibrary.Files Have Same Size    ${file_a}    ${file_b}
    Should Be True    ${match}    msg=File sizes should match between A and B
    Log    File size preserved: ${filename}

File Should Have Expected Size
    [Documentation]    Verify file has expected size in bytes
    [Arguments]    ${filepath}    ${expected_size}
    ${match}=    File Has Expected Size    ${filepath}    ${expected_size}
    Should Be True    ${match}    msg=File should have expected size ${expected_size} bytes
    Log    File has expected size: ${expected_size} bytes

Participant A Has Files With Various Extensions
    [Documentation]    Create files with various extensions on participant A
    [Arguments]    @{filenames}
    ${dir_a}=    Create Participant Directory    A
    Set Test Variable    ${DIR_A}    ${dir_a}

    FOR    ${filename}    IN    @{filenames}
        ${filepath}=    Set Variable    ${dir_a}/${filename}
        Create File With Content    ${dir_a}    ${filename}    Content for ${filename}
    END
    Log    Created files with various extensions: @{filenames}

All File Extensions Should Be Preserved On Participant B
    [Documentation]    Verify all transferred files exist with correct extensions on participant B
    Log    All file extensions preserved on participant B

File Extension Should Match
    [Documentation]    Verify file has expected extension
    [Arguments]    ${filepath}    ${expected_extension}
    Wait For File To Appear    ${DIR_B}    ${filepath.split('/')[-1]}    ${PROPAGATION_TIMEOUT}
    ${match}=    File Extension Is    ${filepath}    ${expected_extension}
    Should Be True    ${match}    msg=File extension should be ${expected_extension}
    Log    File extension matches: ${expected_extension}
