"""
DirShareLibrary - Robot Framework library for DirShare process control

This library provides keywords for starting, stopping, and managing DirShare
instances during Robot Framework acceptance tests.

Author: Generated for OpenDDS DirShare example
Date: 2025-10-31
"""

import os
import subprocess
import time
import signal
import tempfile
import shutil
from pathlib import Path
from typing import Dict, Optional


class DirShareLibrary:
    """Robot Framework library for managing DirShare processes."""

    ROBOT_LIBRARY_SCOPE = 'TEST'
    ROBOT_LIBRARY_VERSION = '1.0'

    def __init__(self):
        """Initialize the DirShareLibrary."""
        self.processes: Dict[str, subprocess.Popen] = {}
        self.test_dirs: Dict[str, str] = {}
        self.inforepo_process: Optional[subprocess.Popen] = None
        self.inforepo_ior_file: Optional[str] = None

        # Find DirShare executable
        self.dirshare_exe = self._find_dirshare_executable()
        self.dcpsinfo_repo_exe = self._find_dcpsinforepo_executable()

    def _find_dirshare_executable(self) -> str:
        """Find the DirShare executable in the build directory."""
        # Try current directory first
        candidates = [
            "dirshare",
            "./dirshare",
            "../dirshare",
            "../../dirshare",
            os.path.join(os.getcwd(), "dirshare"),
        ]

        for candidate in candidates:
            if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                return os.path.abspath(candidate)

        # Try finding in DevGuideExamples
        dds_root = os.environ.get('DDS_ROOT')
        if dds_root:
            path = os.path.join(dds_root, 'DevGuideExamples', 'DCPS', 'DirShare', 'dirshare')
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path

        raise RuntimeError("DirShare executable not found. Please build it first with 'make' in DevGuideExamples/DCPS/DirShare/")

    def _find_dcpsinforepo_executable(self) -> str:
        """Find the DCPSInfoRepo executable."""
        # Check environment
        dds_root = os.environ.get('DDS_ROOT')
        if dds_root:
            path = os.path.join(dds_root, 'bin', 'DCPSInfoRepo')
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path

        # Try PATH
        result = shutil.which('DCPSInfoRepo')
        if result:
            return result

        raise RuntimeError("DCPSInfoRepo executable not found. Please source OpenDDS setenv.sh")

    def create_test_directory(self, participant_name: str) -> str:
        """
        Create a temporary test directory for a participant.

        Args:
            participant_name: Name of the participant (e.g., "A", "B", "C")

        Returns:
            Path to the created directory
        """
        test_dir = tempfile.mkdtemp(prefix=f"dirshare_test_{participant_name}_")
        self.test_dirs[participant_name] = test_dir
        print(f"Created test directory for participant {participant_name}: {test_dir}")
        return test_dir

    def cleanup_test_directory(self, participant_name: str):
        """
        Clean up the test directory for a participant.

        Args:
            participant_name: Name of the participant
        """
        if participant_name in self.test_dirs:
            test_dir = self.test_dirs[participant_name]
            if os.path.exists(test_dir):
                shutil.rmtree(test_dir)
                print(f"Cleaned up test directory for participant {participant_name}: {test_dir}")
            del self.test_dirs[participant_name]

    def cleanup_all_test_directories(self):
        """Clean up all test directories."""
        for participant_name in list(self.test_dirs.keys()):
            self.cleanup_test_directory(participant_name)

    def start_inforepo(self) -> str:
        """
        Start DCPSInfoRepo for InfoRepo discovery mode.

        Returns:
            Path to the IOR file
        """
        if self.inforepo_process and self.inforepo_process.poll() is None:
            print("DCPSInfoRepo already running")
            return self.inforepo_ior_file

        # Create IOR file in temp directory
        self.inforepo_ior_file = tempfile.mktemp(suffix=".ior", prefix="repo_")

        # Start DCPSInfoRepo
        cmd = [self.dcpsinfo_repo_exe, "-o", self.inforepo_ior_file]
        print(f"Starting DCPSInfoRepo: {' '.join(cmd)}")

        self.inforepo_process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Wait for IOR file to be created
        max_wait = 10
        waited = 0
        while not os.path.exists(self.inforepo_ior_file) and waited < max_wait:
            time.sleep(0.5)
            waited += 0.5

        if not os.path.exists(self.inforepo_ior_file):
            raise RuntimeError("DCPSInfoRepo failed to create IOR file")

        print(f"DCPSInfoRepo started, IOR file: {self.inforepo_ior_file}")
        return self.inforepo_ior_file

    def stop_inforepo(self):
        """Stop DCPSInfoRepo if running."""
        if self.inforepo_process:
            print("Stopping DCPSInfoRepo")
            self.inforepo_process.terminate()
            try:
                self.inforepo_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.inforepo_process.kill()
                self.inforepo_process.wait()

            self.inforepo_process = None

        if self.inforepo_ior_file and os.path.exists(self.inforepo_ior_file):
            os.remove(self.inforepo_ior_file)
            self.inforepo_ior_file = None

    def start_dirshare_inforepo(self, participant_name: str, shared_dir: str):
        """
        Start DirShare with InfoRepo discovery mode.

        Args:
            participant_name: Name of the participant
            shared_dir: Path to the shared directory
        """
        if participant_name in self.processes:
            raise RuntimeError(f"DirShare already running for participant {participant_name}")

        # Ensure InfoRepo is running
        ior_file = self.start_inforepo()

        # Build command
        cmd = [
            self.dirshare_exe,
            "-DCPSInfoRepo", f"file://{ior_file}",
            shared_dir
        ]

        print(f"Starting DirShare for participant {participant_name}: {' '.join(cmd)}")

        # Start DirShare
        logfile = open(f"dirshare_{participant_name}.log", "w")
        process = subprocess.Popen(
            cmd,
            stdout=logfile,
            stderr=subprocess.STDOUT,
            text=True
        )

        self.processes[participant_name] = process

        # Give it time to initialize
        time.sleep(2)

        # Check if process is still running
        if process.poll() is not None:
            raise RuntimeError(f"DirShare process for {participant_name} terminated unexpectedly")

        print(f"DirShare started for participant {participant_name} (PID: {process.pid})")

    def start_dirshare_rtps(self, participant_name: str, shared_dir: str, config_file: str = "rtps.ini"):
        """
        Start DirShare with RTPS discovery mode.

        Args:
            participant_name: Name of the participant
            shared_dir: Path to the shared directory
            config_file: Path to RTPS configuration file (default: rtps.ini)
        """
        if participant_name in self.processes:
            raise RuntimeError(f"DirShare already running for participant {participant_name}")

        # Find config file
        if not os.path.isabs(config_file):
            # Look for config file relative to DirShare directory
            dirshare_dir = os.path.dirname(self.dirshare_exe)
            config_path = os.path.join(dirshare_dir, config_file)
            if not os.path.exists(config_path):
                raise RuntimeError(f"RTPS config file not found: {config_path}")
        else:
            config_path = config_file

        # Build command
        cmd = [
            self.dirshare_exe,
            "-DCPSConfigFile", config_path,
            shared_dir
        ]

        print(f"Starting DirShare for participant {participant_name} (RTPS mode): {' '.join(cmd)}")

        # Start DirShare
        logfile = open(f"dirshare_{participant_name}.log", "w")
        process = subprocess.Popen(
            cmd,
            stdout=logfile,
            stderr=subprocess.STDOUT,
            text=True
        )

        self.processes[participant_name] = process

        # Give it time to initialize
        time.sleep(2)

        # Check if process is still running
        if process.poll() is not None:
            raise RuntimeError(f"DirShare process for {participant_name} terminated unexpectedly")

        print(f"DirShare started for participant {participant_name} (PID: {process.pid})")

    def stop_dirshare(self, participant_name: str):
        """
        Stop DirShare for a specific participant.

        Args:
            participant_name: Name of the participant
        """
        if participant_name not in self.processes:
            print(f"No DirShare process found for participant {participant_name}")
            return

        process = self.processes[participant_name]
        print(f"Stopping DirShare for participant {participant_name}")

        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            print(f"Force killing DirShare for participant {participant_name}")
            process.kill()
            process.wait()

        del self.processes[participant_name]
        print(f"DirShare stopped for participant {participant_name}")

    def stop_all_dirshare(self):
        """Stop all running DirShare processes."""
        for participant_name in list(self.processes.keys()):
            self.stop_dirshare(participant_name)

        self.stop_inforepo()

    def dirshare_is_running(self, participant_name: str) -> bool:
        """
        Check if DirShare is running for a participant.

        Args:
            participant_name: Name of the participant

        Returns:
            True if running, False otherwise
        """
        if participant_name not in self.processes:
            return False

        process = self.processes[participant_name]
        return process.poll() is None

    def wait_for_synchronization(self, timeout: int = 10):
        """
        Wait for DDS participants to discover and synchronize.

        Args:
            timeout: Maximum time to wait in seconds
        """
        print(f"Waiting {timeout} seconds for synchronization...")
        time.sleep(timeout)

    def get_test_directory(self, participant_name: str) -> str:
        """
        Get the test directory path for a participant.

        Args:
            participant_name: Name of the participant

        Returns:
            Path to the test directory
        """
        if participant_name not in self.test_dirs:
            raise RuntimeError(f"No test directory found for participant {participant_name}")
        return self.test_dirs[participant_name]
