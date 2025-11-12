*** Settings ***
Documentation    Shared utility keywords for Robot Framework tests
...              This file contains reusable helper keywords that don't fit into
...              specific categories like file operations or participant control.
Library          String

*** Keywords ***
Parse Size String
    [Documentation]    Parse a size string like "1KB" or "5MB" into value and unit
    ...
    ...                Examples:
    ...                - "1KB" returns (1, "KB")
    ...                - "5MB" returns (5, "MB")
    ...                - "100kb" returns (100, "KB") - case insensitive
    ...
    ...                Returns two values: numeric value and unit string
    [Arguments]    ${size_string}

    ${size_upper}=    Convert To Upper Case    ${size_string}
    ${value}=    Evaluate    int(''.join(c for c in '''${size_upper}''' if c.isdigit()))
    ${unit}=    Evaluate    ''.join(c for c in '''${size_upper}''' if c.isalpha())
    RETURN    ${value}    ${unit}
