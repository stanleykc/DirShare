"""
TestFileGenerator - Python library for creating test files with reproducible content.

This library provides utilities to create test files with:
- Predictable content based on seed values
- Exact sizes (bytes, KB, MB)
- Reproducible checksums for validation

Author: Generated for DirShare robustness testing
Feature: 002-robustness-testing
"""

import os
from pathlib import Path
from typing import Optional


class TestFileGenerator:
    """
    Robot Framework library for generating test files with reproducible content.

    Files are generated with predictable content based on seed values,
    allowing checksum verification across participants.
    """

    ROBOT_LIBRARY_SCOPE = 'TEST'
    ROBOT_LIBRARY_VERSION = '1.0'

    def __init__(self, chunk_size: int = 1024):
        """
        Initialize the TestFileGenerator.

        Args:
            chunk_size: Size of content chunks in bytes (default: 1024)
        """
        self.chunk_size = chunk_size

    def _generate_content(self, size_bytes: int, seed: int = 42) -> bytes:
        """
        Generate reproducible binary content.

        Args:
            size_bytes: Total size of content to generate
            seed: Seed value for reproducibility

        Returns:
            Binary content of exact size
        """
        # Generate a repeating pattern based on seed
        pattern = bytes([(seed + i) % 256 for i in range(self.chunk_size)])

        # Repeat pattern to fill size
        full_chunks = size_bytes // self.chunk_size
        remainder = size_bytes % self.chunk_size

        content = pattern * full_chunks
        if remainder > 0:
            content += pattern[:remainder]

        return content

    def create_test_file(self, directory: str, filename: str,
                        size_kb: int = 1, seed: int = 42) -> str:
        """
        Create a test file with reproducible content.

        Args:
            directory: Directory where file will be created
            filename: Name of the file
            size_kb: Size in kilobytes (default: 1)
            seed: Seed for content generation (default: 42)

        Returns:
            Absolute path to created file

        Raises:
            IOError: If file cannot be created
        """
        filepath = os.path.join(directory, filename)
        size_bytes = size_kb * 1024

        print(f"Creating test file: {filepath} ({size_kb} KB, seed={seed})")

        # Ensure directory exists
        os.makedirs(directory, exist_ok=True)

        # Generate and write content
        content = self._generate_content(size_bytes, seed)
        with open(filepath, 'wb') as f:
            f.write(content)

        # Verify size
        actual_size = os.path.getsize(filepath)
        if actual_size != size_bytes:
            raise IOError(f"File size mismatch: expected {size_bytes}, got {actual_size}")

        print(f"✓ Created: {filepath} ({actual_size} bytes)")
        return filepath

    def create_large_file(self, directory: str, filename: str,
                         size_mb: int, seed: int = 42) -> str:
        """
        Create a large file (>= 5MB) for chunked transfer testing.

        This method efficiently creates large files by writing in chunks
        to avoid memory issues.

        Args:
            directory: Directory where file will be created
            filename: Name of the file
            size_mb: Size in megabytes (5-25 MB)
            seed: Seed for content generation (default: 42)

        Returns:
            Absolute path to created file

        Raises:
            ValueError: If size is < 5 MB
            IOError: If file cannot be created
        """
        if size_mb < 5:
            raise ValueError(f"Large files must be >= 5 MB, got {size_mb} MB")

        filepath = os.path.join(directory, filename)
        size_bytes = size_mb * 1024 * 1024

        print(f"Creating large file: {filepath} ({size_mb} MB, seed={seed})")

        # Ensure directory exists
        os.makedirs(directory, exist_ok=True)

        # Generate content pattern once
        pattern = self._generate_content(self.chunk_size, seed)

        # Write file in chunks to avoid memory issues
        with open(filepath, 'wb') as f:
            bytes_written = 0
            while bytes_written < size_bytes:
                bytes_to_write = min(self.chunk_size, size_bytes - bytes_written)
                if bytes_to_write == self.chunk_size:
                    f.write(pattern)
                else:
                    f.write(pattern[:bytes_to_write])
                bytes_written += bytes_to_write

        # Verify size
        actual_size = os.path.getsize(filepath)
        if actual_size != size_bytes:
            raise IOError(f"File size mismatch: expected {size_bytes}, got {actual_size}")

        actual_mb = actual_size / (1024 * 1024)
        print(f"✓ Created: {filepath} ({actual_mb:.2f} MB)")
        return filepath

    def create_file_with_content(self, directory: str, filename: str,
                                 content: str) -> str:
        """
        Create a text file with specific content.

        Args:
            directory: Directory where file will be created
            filename: Name of the file
            content: Text content to write

        Returns:
            Absolute path to created file
        """
        filepath = os.path.join(directory, filename)

        print(f"Creating text file: {filepath} ({len(content)} characters)")

        # Ensure directory exists
        os.makedirs(directory, exist_ok=True)

        # Write content
        with open(filepath, 'w') as f:
            f.write(content)

        print(f"✓ Created: {filepath}")
        return filepath

    def create_empty_file(self, directory: str, filename: str) -> str:
        """
        Create an empty file.

        Args:
            directory: Directory where file will be created
            filename: Name of the file

        Returns:
            Absolute path to created file
        """
        filepath = os.path.join(directory, filename)

        print(f"Creating empty file: {filepath}")

        # Ensure directory exists
        os.makedirs(directory, exist_ok=True)

        # Create empty file
        Path(filepath).touch()

        print(f"✓ Created: {filepath}")
        return filepath

    def create_multiple_files(self, directory: str, count: int,
                            prefix: str = "testfile",
                            size_kb: int = 1,
                            seed_start: int = 42) -> list:
        """
        Create multiple test files with sequential naming.

        Args:
            directory: Directory where files will be created
            count: Number of files to create
            prefix: Filename prefix (default: "testfile")
            size_kb: Size of each file in KB (default: 1)
            seed_start: Starting seed value (increments for each file)

        Returns:
            List of created file paths
        """
        print(f"Creating {count} test files in {directory}")

        filepaths = []
        for i in range(count):
            filename = f"{prefix}_{i+1:03d}.txt"
            seed = seed_start + i
            filepath = self.create_test_file(directory, filename, size_kb, seed)
            filepaths.append(filepath)

        print(f"✓ Created {count} files")
        return filepaths

    def modify_file_content(self, filepath: str, seed: int = 100) -> str:
        """
        Modify an existing file by appending reproducible content.

        Args:
            filepath: Path to file to modify
            seed: Seed for new content (default: 100)

        Returns:
            Path to modified file

        Raises:
            FileNotFoundError: If file doesn't exist
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"File not found: {filepath}")

        print(f"Modifying file: {filepath} (seed={seed})")

        # Get current size
        current_size = os.path.getsize(filepath)

        # Append 1 KB of new content
        additional_content = self._generate_content(1024, seed)
        with open(filepath, 'ab') as f:
            f.write(additional_content)

        new_size = os.path.getsize(filepath)
        print(f"✓ Modified: {filepath} ({current_size} -> {new_size} bytes)")
        return filepath

    def get_file_info(self, filepath: str) -> dict:
        """
        Get information about a file.

        Args:
            filepath: Path to file

        Returns:
            Dictionary with size, modification time, etc.
        """
        if not os.path.exists(filepath):
            return None

        stat = os.stat(filepath)
        return {
            'path': filepath,
            'size': stat.st_size,
            'size_mb': stat.st_size / (1024 * 1024),
            'mtime': stat.st_mtime
        }
