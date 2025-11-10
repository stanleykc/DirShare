"""
ChecksumLibrary - Robot Framework library for file checksum verification

This library provides keywords for calculating and verifying file checksums
using CRC32 algorithm, matching the DirShare implementation.

Author: Generated for OpenDDS DirShare example
Date: 2025-10-31
"""

import os
import zlib
from pathlib import Path


class ChecksumLibrary:
    """Robot Framework library for file checksum operations."""

    ROBOT_LIBRARY_SCOPE = 'GLOBAL'
    ROBOT_LIBRARY_VERSION = '1.0'

    def calculate_file_crc32(self, filepath: str) -> int:
        """
        Calculate CRC32 checksum for a file.

        Args:
            filepath: Path to the file

        Returns:
            CRC32 checksum as unsigned 32-bit integer
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"File not found: {filepath}")

        crc = 0
        with open(filepath, 'rb') as f:
            while True:
                data = f.read(1024 * 1024)  # Read 1MB at a time
                if not data:
                    break
                crc = zlib.crc32(data, crc)

        # Return as unsigned 32-bit integer
        return crc & 0xFFFFFFFF

    def verify_file_checksum(self, filepath: str, expected_checksum: int) -> bool:
        """
        Verify that a file's checksum matches the expected value.

        Args:
            filepath: Path to the file
            expected_checksum: Expected CRC32 checksum

        Returns:
            True if checksums match, False otherwise
        """
        actual_checksum = self.calculate_file_crc32(filepath)
        matches = (actual_checksum == expected_checksum)

        if matches:
            print(f"Checksum verification PASSED for {filepath} (CRC32: {actual_checksum:08x})")
        else:
            print(f"Checksum verification FAILED for {filepath}")
            print(f"  Expected: {expected_checksum:08x}")
            print(f"  Actual:   {actual_checksum:08x}")

        return matches

    def files_have_same_checksum(self, filepath1: str, filepath2: str) -> bool:
        """
        Check if two files have the same checksum.

        Args:
            filepath1: Path to the first file
            filepath2: Path to the second file

        Returns:
            True if checksums match, False otherwise
        """
        checksum1 = self.calculate_file_crc32(filepath1)
        checksum2 = self.calculate_file_crc32(filepath2)

        matches = (checksum1 == checksum2)

        if matches:
            print(f"Files have same checksum (CRC32: {checksum1:08x})")
            print(f"  File 1: {filepath1}")
            print(f"  File 2: {filepath2}")
        else:
            print(f"Files have different checksums")
            print(f"  File 1: {filepath1} (CRC32: {checksum1:08x})")
            print(f"  File 2: {filepath2} (CRC32: {checksum2:08x})")

        return matches

    def calculate_string_crc32(self, data: str) -> int:
        """
        Calculate CRC32 checksum for a string.

        Args:
            data: String data

        Returns:
            CRC32 checksum as unsigned 32-bit integer
        """
        crc = zlib.crc32(data.encode('utf-8'))
        return crc & 0xFFFFFFFF

    def file_matches_content(self, filepath: str, expected_content: str) -> bool:
        """
        Check if a file's content matches the expected content.

        Args:
            filepath: Path to the file
            expected_content: Expected file content

        Returns:
            True if content matches, False otherwise
        """
        if not os.path.exists(filepath):
            print(f"File does not exist: {filepath}")
            return False

        with open(filepath, 'r') as f:
            actual_content = f.read()

        matches = (actual_content == expected_content)

        if matches:
            print(f"File content matches expected content for {filepath}")
        else:
            print(f"File content does NOT match for {filepath}")
            print(f"  Expected: {repr(expected_content)}")
            print(f"  Actual:   {repr(actual_content)}")

        return matches

    def get_file_size(self, filepath: str) -> int:
        """
        Get the size of a file in bytes.

        Args:
            filepath: Path to the file

        Returns:
            File size in bytes
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"File not found: {filepath}")

        return os.path.getsize(filepath)

    def files_have_same_size(self, filepath1: str, filepath2: str) -> bool:
        """
        Check if two files have the same size.

        Args:
            filepath1: Path to the first file
            filepath2: Path to the second file

        Returns:
            True if sizes match, False otherwise
        """
        size1 = self.get_file_size(filepath1)
        size2 = self.get_file_size(filepath2)

        matches = (size1 == size2)

        if matches:
            print(f"Files have same size ({size1} bytes)")
        else:
            print(f"Files have different sizes")
            print(f"  File 1: {filepath1} ({size1} bytes)")
            print(f"  File 2: {filepath2} ({size2} bytes)")

        return matches
