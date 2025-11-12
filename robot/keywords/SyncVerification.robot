*** Settings ***
Documentation    Keywords for verifying directory synchronization in robustness tests
Library          ../resources/SyncVerifier.py    WITH NAME    SyncLib
Library          ../resources/config.py
Library          OperatingSystem
Library          DateTime
Library          Collections
Resource         FileOperations.robot    # For Get File Checksum

*** Keywords ***
Wait For Synchronization
    [Documentation]    Wait for directories to synchronize within timeout
    ...                Arguments can be participant labels (A, B, C) or directory paths
    [Arguments]    ${timeout}    @{labels}

    Log    Waiting for synchronization of ${labels} (timeout: ${timeout}s)

    # Convert labels to directories
    @{directories}=    Create List
    FOR    ${label}    IN    @{labels}
        ${dir}=    Get Test Directory    ${label}
        Append To List    ${directories}    ${dir}
    END

    ${start_time}=    Get Time    epoch
    ${success}=    SyncLib.Wait For Synchronization    @{directories}    timeout=${timeout}    poll_interval=1

    ${end_time}=    Get Time    epoch
    ${actual_time}=    Evaluate    ${end_time} - ${start_time}

    Should Be True    ${success}    msg=Directories did not synchronize within ${timeout} seconds

    Log    ✓ Synchronization achieved in ${actual_time} seconds (timeout was ${timeout}s)
    RETURN    ${actual_time}

Directories Are Synchronized
    [Documentation]    Check if directories are currently synchronized
    [Arguments]    @{directories}

    ${is_synchronized}=    Directories Are Synchronized    @{directories}
    RETURN    ${is_synchronized}

Directories Should Be Synchronized
    [Documentation]    Assert that directories are synchronized
    [Arguments]    @{directories}

    ${is_synchronized}=    Directories Are Synchronized    @{directories}
    Should Be True    ${is_synchronized}    msg=Directories are not synchronized: ${directories}
    Log    ✓ Directories are synchronized

Verify File Checksums Match
    [Documentation]    Verify that a file has matching checksums across participants
    ...                Arguments can be participant labels (A, B, C) or directory paths
    [Arguments]    ${filename}    @{labels}

    Log    Verifying checksums for ${filename} across ${labels}

    # Convert labels to directories
    @{directories}=    Create List
    FOR    ${label}    IN    @{labels}
        ${dir}=    Get Test Directory    ${label}
        Append To List    ${directories}    ${dir}
    END

    # Build full paths
    @{filepaths}=    Create List
    FOR    ${dir}    IN    @{directories}
        ${filepath}=    Set Variable    ${dir}/${filename}
        File Should Exist    ${filepath}    msg=File ${filename} not found in ${dir}
        Append To List    ${filepaths}    ${filepath}
    END

    # Get checksums
    @{checksums}=    Create List
    FOR    ${filepath}    IN    @{filepaths}
        ${checksum}=    Get File Checksum    ${filepath}
        Append To List    ${checksums}    ${checksum}
        Log    ${filepath}: checksum=${checksum}
    END

    # Verify all checksums match
    ${first_checksum}=    Get From List    ${checksums}    0
    FOR    ${checksum}    IN    @{checksums}
        Should Be Equal As Integers    ${checksum}    ${first_checksum}
        ...    msg=Checksum mismatch for ${filename}
    END

    Log    ✓ All checksums match for ${filename}

Get File Size From Participant
    [Documentation]    Get the size of a file from a participant's directory
    [Arguments]    ${participant_label}    ${filename}

    ${dir}=    Get Test Directory    ${participant_label}
    ${filepath}=    Set Variable    ${dir}/${filename}
    ${size}=    OperatingSystem.Get File Size    ${filepath}
    RETURN    ${size}

Get Directory Differences
    [Documentation]    Get detailed differences between two directories
    [Arguments]    ${dir1}    ${dir2}

    ${differences}=    Get Directory Differences    ${dir1}    ${dir2}
    RETURN    ${differences}

Log Directory State
    [Documentation]    Log the current state of a directory for debugging
    [Arguments]    ${directory}    ${label}=${EMPTY}

    ${prefix}=    Set Variable If    '${label}' != ''    ${label}:    Directory:
    Log    ${prefix} ${directory}

    ${file_count}=    Get File Count    ${directory}
    Log    ${prefix} Contains ${file_count} files

    @{files}=    List Files In Directory    ${directory}
    FOR    ${file}    IN    @{files}
        ${filepath}=    Set Variable    ${directory}/${file}
        ${size}=    OperatingSystem.Get File Size    ${filepath}
        Log    ${prefix}   - ${file} (${size} bytes)
    END

Compare Three Directories
    [Documentation]    Compare three directories and verify they are synchronized
    [Arguments]    ${dir_A}    ${dir_B}    ${dir_C}

    Log    Comparing three directories for synchronization
    Log Directory State    ${dir_A}    A
    Log Directory State    ${dir_B}    B
    Log Directory State    ${dir_C}    C

    Directories Should Be Synchronized    ${dir_A}    ${dir_B}    ${dir_C}
    Log    ✓ All three directories are synchronized

File Should Exist In All Directories
    [Documentation]    Verify that a file exists in all specified directories
    [Arguments]    ${filename}    @{directories}

    FOR    ${dir}    IN    @{directories}
        ${filepath}=    Set Variable    ${dir}/${filename}
        File Should Exist    ${filepath}    msg=File ${filename} not found in ${dir}
    END

    Log    ✓ File ${filename} exists in all ${len(${directories})} directories

File Should Not Exist In Any Directory
    [Documentation]    Verify that a file does not exist in any specified directory
    [Arguments]    ${filename}    @{directories}

    FOR    ${dir}    IN    @{directories}
        ${filepath}=    Set Variable    ${dir}/${filename}
        File Should Not Exist    ${filepath}    msg=File ${filename} should not exist in ${dir}
    END

    Log    ✓ File ${filename} does not exist in any directory

Wait For File To Appear In All Directories
    [Documentation]    Wait for a file to appear in all directories within timeout
    [Arguments]    ${filename}    ${timeout}    @{directories}

    Log    Waiting for ${filename} to appear in all directories (timeout: ${timeout}s)

    FOR    ${dir}    IN    @{directories}
        ${filepath}=    Set Variable    ${dir}/${filename}
        Wait Until Keyword Succeeds    ${timeout}s    0.5s
        ...    File Should Exist    ${filepath}
        Log    ✓ File ${filename} appeared in ${dir}
    END

    Log    ✓ File ${filename} appeared in all directories
