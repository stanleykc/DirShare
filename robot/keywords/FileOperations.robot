*** Settings ***
Documentation    Keywords for file system operations in DirShare tests
Library          OperatingSystem
Library          String
Library          DateTime
Library          ../libraries/ChecksumLibrary.py
Library          ../libraries/MetadataLibrary.py

*** Keywords ***
Create File
    [Documentation]    Create a file for a participant with specified size
    ...                Size can be "1KB", "5MB", etc.
    [Arguments]    ${participant_label}    ${filename}    ${size}    ${seed}=42

    # Get participant directory
    ${dir}=    Get Test Directory    ${participant_label}

    # Parse size (e.g., "1KB" -> 1, "5MB" -> 5)
    ${size_value}    ${size_unit}=    Parse Size String    ${size}

    # Create file based on size unit
    ${filepath}=    Run Keyword If    '${size_unit}' == 'KB'
    ...    Create Binary File    ${dir}    ${filename}    ${size_value}
    ...    ELSE IF    '${size_unit}' == 'MB'
    ...    Create Large File    ${dir}    ${filename}    ${size_value}
    ...    ELSE
    ...    Fail    Unsupported size unit: ${size_unit}

    Log    Created file ${filename} (${size}) in participant ${participant_label} directory: ${dir}
    RETURN    ${filepath}

Modify File
    [Documentation]    Modify an existing file for a participant by appending data
    ...                The size parameter specifies how much data to append
    [Arguments]    ${participant_label}    ${filename}    ${size}    ${seed}=42

    # Get participant directory
    ${dir}=    Get Test Directory    ${participant_label}
    ${filepath}=    Set Variable    ${dir}/${filename}

    # File must exist
    File Should Exist    ${filepath}    msg=Cannot modify non-existent file ${filename}

    # Parse size to append
    ${size_value}    ${size_unit}=    Parse Size String    ${size}

    # Convert to bytes
    ${bytes_to_append}=    Run Keyword If    '${size_unit}' == 'KB'
    ...    Evaluate    ${size_value} * 1024
    ...    ELSE IF    '${size_unit}' == 'MB'
    ...    Evaluate    ${size_value} * 1024 * 1024
    ...    ELSE
    ...    Fail    Unsupported size unit: ${size_unit}

    # Create temporary file with data to append
    ${temp_file}=    Set Variable    ${dir}/.tmp_append_${filename}
    Run    dd if=/dev/urandom of=${temp_file} bs=1 count=${bytes_to_append} 2>/dev/null

    # Append to original file
    Run    cat ${temp_file} >> ${filepath}

    # Clean up temp file
    Remove File    ${temp_file}

    Log    Modified file ${filename} in participant ${participant_label} directory (appended ${size})
    RETURN    ${filepath}

Delete Participant File
    [Documentation]    Delete a file from a participant's directory
    [Arguments]    ${participant_label}    ${filename}

    ${dir}=    Get Test Directory    ${participant_label}
    ${filepath}=    Set Variable    ${dir}/${filename}
    Remove File    ${filepath}
    Log    Deleted file ${filename} from participant ${participant_label} directory

Create Large File For Participant
    [Documentation]    Create a large file (>=5MB) for a participant with specified size
    ...                Size can be "5MB", "15MB", etc.
    [Arguments]    ${participant_label}    ${filename}    ${size}    ${seed}=42

    # Get participant directory
    ${dir}=    Get Test Directory    ${participant_label}

    # Parse size (e.g., "5MB" -> 5)
    ${size_value}    ${size_unit}=    Parse Size String    ${size}

    # Verify it's MB
    Should Be Equal As Strings    ${size_unit}    MB    msg=Create Large File requires MB unit, got ${size_unit}

    # Create large file using the existing keyword
    ${filepath}=    Create Large File    ${dir}    ${filename}    ${size_value}

    Log    Created large file ${filename} (${size}) in participant ${participant_label} directory: ${dir}
    RETURN    ${filepath}

Parse Size String
    [Documentation]    Parse a size string like "1KB" or "5MB" into value and unit
    [Arguments]    ${size_string}

    ${size_upper}=    Convert To Upper Case    ${size_string}
    ${value}=    Evaluate    int(''.join(c for c in '''${size_upper}''' if c.isdigit()))
    ${unit}=    Evaluate    ''.join(c for c in '''${size_upper}''' if c.isalpha())
    RETURN    ${value}    ${unit}

Create File With Content
    [Documentation]    Create a file with specified content
    [Arguments]    ${directory}    ${filename}    ${content}
    ${filepath}=    Set Variable    ${directory}/${filename}
    OperatingSystem.Create File    ${filepath}    ${content}
    Log    Created file ${filepath} with content: ${content}
    RETURN    ${filepath}

Create Empty File
    [Documentation]    Create an empty file
    [Arguments]    ${directory}    ${filename}
    ${filepath}=    Set Variable    ${directory}/${filename}
    Create File    ${filepath}
    Log    Created empty file ${filepath}
    RETURN    ${filepath}

Create Binary File
    [Documentation]    Create a binary file with random data
    [Arguments]    ${directory}    ${filename}    ${size_kb}
    ${filepath}=    Set Variable    ${directory}/${filename}
    Run    dd if=/dev/urandom of=${filepath} bs=1024 count=${size_kb} 2>/dev/null
    Log    Created binary file ${filepath} with size ${size_kb} KB
    RETURN    ${filepath}

Create Large File
    [Documentation]    Create a large file (>10MB) for chunked transfer testing
    [Arguments]    ${directory}    ${filename}    ${size_mb}
    ${filepath}=    Set Variable    ${directory}/${filename}
    ${size_kb}=    Evaluate    ${size_mb} * 1024
    Create Binary File    ${directory}    ${filename}    ${size_kb}
    Log    Created large file ${filepath} with size ${size_mb} MB
    RETURN    ${filepath}

Modify File Content
    [Documentation]    Modify the content of an existing file
    [Arguments]    ${filepath}    ${new_content}
    File Should Exist    ${filepath}
    Create File    ${filepath}    ${new_content}
    Log    Modified file ${filepath} with new content: ${new_content}

Append To File
    [Documentation]    Append content to an existing file
    [Arguments]    ${filepath}    ${content_to_append}
    File Should Exist    ${filepath}
    OperatingSystem.Append To File    ${filepath}    ${content_to_append}
    Log    Appended to file ${filepath}: ${content_to_append}

Delete File
    [Documentation]    Delete a file
    [Arguments]    ${filepath}
    Remove File    ${filepath}
    Log    Deleted file ${filepath}

File Should Exist Within Timeout
    [Documentation]    Wait for a file to exist within a specified timeout
    [Arguments]    ${filepath}    ${timeout}=5
    Wait Until Keyword Succeeds    ${timeout}s    0.5s    File Should Exist    ${filepath}
    Log    File ${filepath} exists

File Should Not Exist
    [Documentation]    Verify that a file does not exist
    [Arguments]    ${filepath}
    ${exists}=    Run Keyword And Return Status    File Should Exist    ${filepath}
    Should Not Be True    ${exists}    msg=File ${filepath} should not exist
    Log    File ${filepath} does not exist

Get File Content
    [Documentation]    Read and return file content
    [Arguments]    ${filepath}
    ${content}=    Get File    ${filepath}
    RETURN    ${content}

Verify File Content
    [Documentation]    Verify that file has expected content
    [Arguments]    ${filepath}    ${expected_content}
    ${actual_content}=    Get File Content    ${filepath}
    Should Be Equal    ${actual_content}    ${expected_content}    msg=File content mismatch for ${filepath}
    Log    File content verified for ${filepath}

Files Should Have Same Content
    [Documentation]    Verify that two files have identical content
    [Arguments]    ${filepath1}    ${filepath2}
    ${content1}=    Get File Content    ${filepath1}
    ${content2}=    Get File Content    ${filepath2}
    Should Be Equal    ${content1}    ${content2}    msg=Files have different content
    Log    Files have same content: ${filepath1} and ${filepath2}

Files Should Have Same Checksum
    [Documentation]    Verify that two files have the same CRC32 checksum
    [Arguments]    ${filepath1}    ${filepath2}
    ${match}=    Files Have Same Checksum    ${filepath1}    ${filepath2}
    Should Be True    ${match}    msg=Files have different checksums
    Log    Files have same checksum: ${filepath1} and ${filepath2}

Get File Checksum
    [Documentation]    Calculate and return CRC32 checksum for a file
    [Arguments]    ${filepath}
    ${checksum}=    Calculate File Crc32    ${filepath}
    Log    Checksum for ${filepath}: ${checksum}
    RETURN    ${checksum}

Verify File Checksum
    [Documentation]    Verify that a file has expected checksum
    [Arguments]    ${filepath}    ${expected_checksum}
    ${match}=    ChecksumLibrary.Verify File Checksum    ${filepath}    ${expected_checksum}
    Should Be True    ${match}    msg=File checksum mismatch
    Log    File checksum verified for ${filepath}

Get File Size
    [Documentation]    Get the size of a file in bytes
    [Arguments]    ${filepath}
    ${size}=    OperatingSystem.Get File Size    ${filepath}
    Log    File size for ${filepath}: ${size} bytes
    RETURN    ${size}

Files Should Have Same Size
    [Documentation]    Verify that two files have the same size
    [Arguments]    ${filepath1}    ${filepath2}
    ${size1}=    Get File Size    ${filepath1}
    ${size2}=    Get File Size    ${filepath2}
    Should Be Equal As Integers    ${size1}    ${size2}    msg=Files have different sizes
    Log    Files have same size (${size1} bytes): ${filepath1} and ${filepath2}

Count Files In Directory
    [Documentation]    Count the number of files in a directory
    [Arguments]    ${directory}
    @{files}=    List Files In Directory    ${directory}
    ${count}=    Get Length    ${files}
    Log    Directory ${directory} contains ${count} files
    RETURN    ${count}

Directory Should Have File Count
    [Documentation]    Verify that directory contains expected number of files
    [Arguments]    ${directory}    ${expected_count}
    ${actual_count}=    Count Files In Directory    ${directory}
    Should Be Equal As Integers    ${actual_count}    ${expected_count}    msg=Directory has unexpected file count
    Log    Directory ${directory} has expected file count: ${expected_count}

Copy Files To Directory
    [Documentation]    Copy multiple files from source to destination directory
    [Arguments]    ${source_dir}    ${dest_dir}    @{filenames}
    FOR    ${filename}    IN    @{filenames}
        ${source}=    Set Variable    ${source_dir}/${filename}
        ${dest}=    Set Variable    ${dest_dir}/${filename}
        Copy File    ${source}    ${dest}
        Log    Copied ${source} to ${dest}
    END

Wait For File To Appear
    [Documentation]    Wait for a file to appear in a directory
    [Arguments]    ${directory}    ${filename}    ${timeout}=5
    ${filepath}=    Set Variable    ${directory}/${filename}
    File Should Exist Within Timeout    ${filepath}    ${timeout}
    Log    File ${filename} appeared in ${directory}
    RETURN    ${filepath}

Wait For File To Disappear
    [Documentation]    Wait for a file to be deleted from a directory
    [Arguments]    ${directory}    ${filename}    ${timeout}=5
    ${filepath}=    Set Variable    ${directory}/${filename}
    Wait Until Keyword Succeeds    ${timeout}s    0.5s    File Should Not Exist    ${filepath}
    Log    File ${filename} disappeared from ${directory}

Create Test Files
    [Documentation]    Create multiple test files in a directory
    [Arguments]    ${directory}    ${count}    ${prefix}=testfile
    FOR    ${i}    IN RANGE    ${count}
        ${filename}=    Set Variable    ${prefix}_${i}.txt
        ${content}=    Set Variable    Content of file ${i}
        Create File With Content    ${directory}    ${filename}    ${content}
    END
    Log    Created ${count} test files in ${directory}

Create File With Specific Timestamp
    [Documentation]    Create a file with specific content and modification timestamp
    [Arguments]    ${filepath}    ${content}    ${timestamp}
    Create File    ${filepath}    ${content}
    MetadataLibrary.Set File Modification Time    ${filepath}    ${timestamp}
    Log    Created file ${filepath} with timestamp ${timestamp}
    RETURN    ${filepath}

Create File With Size
    [Documentation]    Create a file with exact size in bytes
    [Arguments]    ${filepath}    ${size_bytes}
    # Create file using dd for exact byte size
    Run    dd if=/dev/zero of=${filepath} bs=${size_bytes} count=1 2>/dev/null
    ${actual_size}=    Get File Size    ${filepath}
    Should Be Equal As Integers    ${actual_size}    ${size_bytes}    msg=Created file should have exact size
    Log    Created file ${filepath} with exact size ${size_bytes} bytes
    RETURN    ${filepath}
