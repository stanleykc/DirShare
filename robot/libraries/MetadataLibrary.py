"""
MetadataLibrary - Robot Framework library for file metadata operations

This library provides keywords for working with file metadata (timestamps, sizes, extensions)
during Robot Framework acceptance tests for US6 (Metadata Transfer and Preservation).

Author: Generated for OpenDDS DirShare US6 testing
Date: 2025-11-11
"""

import os
import time
from pathlib import Path
from datetime import datetime
from typing import Tuple


class MetadataLibrary:
    """Robot Framework library for file metadata operations."""

    ROBOT_LIBRARY_SCOPE = 'TEST'
    ROBOT_LIBRARY_VERSION = '1.0'

    def __init__(self):
        """Initialize the MetadataLibrary."""
        pass

    def get_file_modification_time(self, filepath: str) -> float:
        """
        Get the modification time of a file.

        Args:
            filepath: Path to the file

        Returns:
            Modification time as a float (seconds since epoch)
        """
        if not os.path.exists(filepath):
            raise RuntimeError(f"File does not exist: {filepath}")

        mtime = os.path.getmtime(filepath)
        print(f"File {filepath} has modification time: {mtime} ({datetime.fromtimestamp(mtime)})")
        return mtime

    def get_file_size(self, filepath: str) -> int:
        """
        Get the size of a file in bytes.

        Args:
            filepath: Path to the file

        Returns:
            File size in bytes
        """
        if not os.path.exists(filepath):
            raise RuntimeError(f"File does not exist: {filepath}")

        size = os.path.getsize(filepath)
        print(f"File {filepath} has size: {size} bytes")
        return size

    def get_file_extension(self, filepath: str) -> str:
        """
        Get the extension of a file.

        Args:
            filepath: Path to the file

        Returns:
            File extension (including the dot, e.g., ".txt")
        """
        extension = os.path.splitext(filepath)[1]
        print(f"File {filepath} has extension: {extension}")
        return extension

    def files_have_same_modification_time(self, file1: str, file2: str, tolerance_seconds: float = 2.0) -> bool:
        """
        Check if two files have the same modification time (within tolerance).

        Args:
            file1: Path to first file
            file2: Path to second file
            tolerance_seconds: Allowed difference in seconds (default: 2.0)

        Returns:
            True if modification times are within tolerance, False otherwise
        """
        mtime1 = self.get_file_modification_time(file1)
        mtime2 = self.get_file_modification_time(file2)

        diff = abs(mtime1 - mtime2)
        match = diff <= tolerance_seconds

        print(f"Modification time difference: {diff:.6f} seconds (tolerance: {tolerance_seconds})")
        print(f"Timestamps match: {match}")

        return match

    def files_have_same_size(self, file1: str, file2: str) -> bool:
        """
        Check if two files have the same size.

        Args:
            file1: Path to first file
            file2: Path to second file

        Returns:
            True if sizes match, False otherwise
        """
        size1 = self.get_file_size(file1)
        size2 = self.get_file_size(file2)

        match = size1 == size2
        print(f"File sizes match: {match}")
        return match

    def file_has_expected_size(self, filepath: str, expected_size: int) -> bool:
        """
        Check if a file has the expected size.

        Args:
            filepath: Path to the file
            expected_size: Expected file size in bytes

        Returns:
            True if size matches expected, False otherwise
        """
        actual_size = self.get_file_size(filepath)
        match = actual_size == expected_size

        if not match:
            print(f"Size mismatch: expected {expected_size}, got {actual_size}")

        return match

    def file_extension_is(self, filepath: str, expected_extension: str) -> bool:
        """
        Check if a file has the expected extension.

        Args:
            filepath: Path to the file
            expected_extension: Expected extension (e.g., ".txt", ".jpg")

        Returns:
            True if extension matches, False otherwise
        """
        actual_extension = self.get_file_extension(filepath)

        # Normalize: ensure expected extension starts with dot
        if expected_extension and not expected_extension.startswith('.'):
            expected_extension = '.' + expected_extension

        match = actual_extension.lower() == expected_extension.lower()

        if not match:
            print(f"Extension mismatch: expected {expected_extension}, got {actual_extension}")

        return match

    def set_file_modification_time(self, filepath: str, timestamp: float):
        """
        Set the modification time of a file.

        Args:
            filepath: Path to the file
            timestamp: Modification time as seconds since epoch
        """
        if not os.path.exists(filepath):
            raise RuntimeError(f"File does not exist: {filepath}")

        os.utime(filepath, (timestamp, timestamp))
        print(f"Set modification time for {filepath} to {timestamp} ({datetime.fromtimestamp(timestamp)})")

    def create_file_with_specific_timestamp(self, filepath: str, content: str, timestamp: float):
        """
        Create a file with specific content and modification timestamp.

        Args:
            filepath: Path to the file to create
            content: File content
            timestamp: Modification time as seconds since epoch
        """
        # Create file with content
        with open(filepath, 'w') as f:
            f.write(content)

        # Set modification time
        self.set_file_modification_time(filepath, timestamp)

        print(f"Created file {filepath} with timestamp {timestamp} ({datetime.fromtimestamp(timestamp)})")

    def verify_timestamp_preserved(self, original_file: str, transferred_file: str, tolerance_seconds: float = 2.0):
        """
        Verify that a transferred file has preserved the original file's timestamp.

        Args:
            original_file: Path to original file
            transferred_file: Path to transferred file
            tolerance_seconds: Allowed difference in seconds (default: 2.0)

        Raises:
            AssertionError if timestamps don't match within tolerance
        """
        if not self.files_have_same_modification_time(original_file, transferred_file, tolerance_seconds):
            mtime1 = self.get_file_modification_time(original_file)
            mtime2 = self.get_file_modification_time(transferred_file)
            diff = abs(mtime1 - mtime2)
            raise AssertionError(
                f"Timestamp not preserved!\n"
                f"Original: {datetime.fromtimestamp(mtime1)} ({mtime1})\n"
                f"Transferred: {datetime.fromtimestamp(mtime2)} ({mtime2})\n"
                f"Difference: {diff:.6f} seconds (tolerance: {tolerance_seconds})"
            )

        print(f"✓ Timestamp successfully preserved between {original_file} and {transferred_file}")
