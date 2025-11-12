"""
SyncVerifier - Python library for verifying directory synchronization.

This library provides utilities to compare directories and verify that
DirShare participants have synchronized correctly. It checks:
- File names match across directories
- File sizes match
- CRC32 checksums match
- Modification times are within tolerance

Author: Generated for DirShare robustness testing
Feature: 002-robustness-testing
"""

import os
import zlib
from pathlib import Path
from typing import Dict, Tuple, List, Set


class SyncVerifier:
    """
    Robot Framework library for verifying directory synchronization.

    This library compares multiple directories to ensure they contain
    identical files with matching content (checksums).
    """

    ROBOT_LIBRARY_SCOPE = 'TEST'
    ROBOT_LIBRARY_VERSION = '1.0'

    def __init__(self, timestamp_tolerance: int = 5):
        """
        Initialize the SyncVerifier.

        Args:
            timestamp_tolerance: Maximum seconds difference for modification time comparison
        """
        self.timestamp_tolerance = timestamp_tolerance

    def _checksum(self, filepath: str) -> int:
        """
        Calculate CRC32 checksum for a file.

        This matches the DirShare Checksum.cpp implementation (CRC32).

        Args:
            filepath: Path to file

        Returns:
            CRC32 checksum as unsigned 32-bit integer

        Raises:
            FileNotFoundError: If file doesn't exist
            IOError: If file cannot be read
        """
        crc = 0
        with open(filepath, 'rb') as f:
            while True:
                chunk = f.read(8192)
                if not chunk:
                    break
                crc = zlib.crc32(chunk, crc)

        # Return as unsigned 32-bit integer (matches DirShare)
        return crc & 0xFFFFFFFF

    def _build_file_map(self, directory: str) -> Dict[str, Tuple[int, int, float]]:
        """
        Build a map of files in a directory.

        Args:
            directory: Path to directory

        Returns:
            Dictionary mapping relative_path -> (size, checksum, mtime)
        """
        file_map = {}
        dir_path = Path(directory)

        if not dir_path.exists():
            print(f"Warning: Directory does not exist: {directory}")
            return file_map

        # Recursively find all files
        for filepath in dir_path.rglob('*'):
            if filepath.is_file():
                try:
                    rel_path = filepath.relative_to(dir_path)
                    stat = filepath.stat()

                    file_map[str(rel_path)] = (
                        stat.st_size,                    # Size in bytes
                        self._checksum(str(filepath)),   # CRC32 checksum
                        stat.st_mtime                    # Modification time
                    )
                except Exception as e:
                    print(f"Warning: Error processing file {filepath}: {e}")
                    continue

        return file_map

    def directories_are_synchronized(self, *directories: str) -> bool:
        """
        Check if multiple directories are synchronized (have identical files).

        Args:
            *directories: Variable number of directory paths to compare

        Returns:
            True if all directories have identical files and checksums, False otherwise
        """
        if len(directories) < 2:
            print("Warning: Need at least 2 directories to compare")
            return True

        print(f"Comparing {len(directories)} directories for synchronization:")
        for d in directories:
            print(f"  - {d}")

        # Build file maps for all directories
        file_maps = [self._build_file_map(d) for d in directories]

        # Use first directory as reference
        reference_map = file_maps[0]
        reference_dir = directories[0]

        # Check if all other directories match the reference
        for i, (file_map, directory) in enumerate(zip(file_maps[1:], directories[1:]), start=1):
            if not self._maps_equal(reference_map, file_map, reference_dir, directory):
                return False

        print(f"✓ All {len(directories)} directories are synchronized")
        return True

    def _maps_equal(self, map1: Dict[str, Tuple], map2: Dict[str, Tuple],
                    dir1: str, dir2: str) -> bool:
        """
        Compare two file maps for equality.

        Args:
            map1: File map from first directory
            map2: File map from second directory
            dir1: Path to first directory (for logging)
            dir2: Path to second directory (for logging)

        Returns:
            True if maps are equal, False otherwise
        """
        # Check file sets match
        files1 = set(map1.keys())
        files2 = set(map2.keys())

        if files1 != files2:
            missing_in_2 = files1 - files2
            missing_in_1 = files2 - files1

            if missing_in_2:
                print(f"✗ Files in {dir1} but not in {dir2}: {missing_in_2}")
            if missing_in_1:
                print(f"✗ Files in {dir2} but not in {dir1}: {missing_in_1}")

            return False

        # Check file properties match
        for filename in files1:
            size1, checksum1, mtime1 = map1[filename]
            size2, checksum2, mtime2 = map2[filename]

            # Check size
            if size1 != size2:
                print(f"✗ Size mismatch for {filename}: {size1} vs {size2}")
                return False

            # Check checksum (most important for content verification)
            if checksum1 != checksum2:
                print(f"✗ Checksum mismatch for {filename}: {checksum1:08x} vs {checksum2:08x}")
                return False

            # Check modification time (within tolerance)
            time_diff = abs(mtime1 - mtime2)
            if time_diff > self.timestamp_tolerance:
                print(f"✗ Timestamp difference for {filename}: {time_diff:.1f}s (tolerance: {self.timestamp_tolerance}s)")
                return False

        return True

    def get_file_count(self, directory: str) -> int:
        """
        Get the number of files in a directory.

        Args:
            directory: Path to directory

        Returns:
            Number of files
        """
        file_map = self._build_file_map(directory)
        return len(file_map)

    def get_directory_differences(self, dir1: str, dir2: str) -> Dict[str, List[str]]:
        """
        Get detailed differences between two directories.

        Args:
            dir1: First directory path
            dir2: Second directory path

        Returns:
            Dictionary with keys: 'missing_in_dir2', 'missing_in_dir1', 'size_mismatch', 'checksum_mismatch'
        """
        map1 = self._build_file_map(dir1)
        map2 = self._build_file_map(dir2)

        files1 = set(map1.keys())
        files2 = set(map2.keys())

        differences = {
            'missing_in_dir2': list(files1 - files2),
            'missing_in_dir1': list(files2 - files1),
            'size_mismatch': [],
            'checksum_mismatch': []
        }

        # Check common files for mismatches
        for filename in files1 & files2:
            size1, checksum1, _ = map1[filename]
            size2, checksum2, _ = map2[filename]

            if size1 != size2:
                differences['size_mismatch'].append(filename)
            elif checksum1 != checksum2:
                differences['checksum_mismatch'].append(filename)

        return differences

    def wait_for_synchronization(self, *directories: str, timeout: int = 10,
                                  poll_interval: float = 1.0) -> bool:
        """
        Wait for directories to synchronize within a timeout.

        Args:
            *directories: Variable number of directory paths
            timeout: Maximum seconds to wait
            poll_interval: Seconds between synchronization checks

        Returns:
            True if synchronized within timeout, False otherwise
        """
        import time
        start_time = time.time()

        while (time.time() - start_time) < timeout:
            if self.directories_are_synchronized(*directories):
                elapsed = time.time() - start_time
                print(f"✓ Synchronization achieved in {elapsed:.2f} seconds")
                return True

            time.sleep(poll_interval)

        elapsed = time.time() - start_time
        print(f"✗ Synchronization timeout after {elapsed:.2f} seconds")
        return False
