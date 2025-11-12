"""
ProcessManager - Python library for managing DirShare participant processes.

This library extends the existing DirShareLibrary with enhanced process control
capabilities needed for robustness testing, including:
- Graceful shutdown (SIGTERM)
- Force kill (SIGKILL)
- Process monitoring and status checking
- Restart orchestration

This complements DirShareLibrary by providing lower-level process management
for shutdown/restart test scenarios.

Author: Generated for DirShare robustness testing
Feature: 002-robustness-testing
"""

import os
import subprocess
import signal
import time
import psutil
from typing import Dict, Optional, List
from pathlib import Path


class ProcessManager:
    """
    Robot Framework library for advanced DirShare process management.

    This library provides process lifecycle control for robustness testing,
    including graceful shutdown, force kill, and restart operations.
    """

    ROBOT_LIBRARY_SCOPE = 'TEST'
    ROBOT_LIBRARY_VERSION = '1.0'

    def __init__(self):
        """Initialize the ProcessManager."""
        self.processes: Dict[str, subprocess.Popen] = {}
        self.process_info: Dict[str, dict] = {}  # Store additional process metadata
        self.dirshare_exe = self._find_dirshare_executable()

    def _find_dirshare_executable(self) -> str:
        """
        Find the DirShare executable using portable relative paths.

        Returns:
            Absolute path to dirshare executable

        Raises:
            RuntimeError: If executable not found
        """
        # Get the directory containing this file (robot/resources/)
        this_file = os.path.abspath(__file__)
        resources_dir = os.path.dirname(this_file)
        robot_dir = os.path.dirname(resources_dir)
        dirshare_root = os.path.dirname(robot_dir)

        # Primary: relative path from this file
        relative_exe_path = os.path.join(dirshare_root, "dirshare")

        candidates = [
            relative_exe_path,
            os.path.join(os.getcwd(), "dirshare"),
            os.path.join(os.getcwd(), "..", "dirshare"),
            "dirshare",
            "./dirshare",
        ]

        for candidate in candidates:
            if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                abs_path = os.path.abspath(candidate)
                print(f"Found DirShare executable at: {abs_path}")
                return abs_path

        raise RuntimeError(
            f"DirShare executable not found. Please build it first with 'make'.\n"
            f"Expected at: {relative_exe_path}\n"
            f"Searched: {', '.join(candidates)}"
        )

    def start_participant(self, label: str, directory: str, config_file: str = "rtps.ini") -> int:
        """
        Start a DirShare participant process.

        Args:
            label: Participant label (A, B, C, etc.)
            directory: Path to monitored directory
            config_file: DDS configuration file (default: rtps.ini)

        Returns:
            Process ID (PID) of started process

        Raises:
            RuntimeError: If process fails to start
        """
        if label in self.processes and self.is_running(label):
            raise RuntimeError(f"Participant {label} is already running")

        # Find config file (relative to dirshare executable)
        if not os.path.isabs(config_file):
            dirshare_dir = os.path.dirname(self.dirshare_exe)
            config_path = os.path.join(dirshare_dir, config_file)
            if not os.path.exists(config_path):
                raise RuntimeError(f"Config file not found: {config_path}")
        else:
            config_path = config_file

        # Build command
        cmd = [
            self.dirshare_exe,
            "-DCPSConfigFile", config_path,
            directory
        ]

        print(f"Starting participant {label}: {' '.join(cmd)}")

        # Start process with log capture
        logfile = open(f"dirshare_{label}.log", "w")
        process = subprocess.Popen(
            cmd,
            stdout=logfile,
            stderr=subprocess.STDOUT,
            text=True
        )

        self.processes[label] = process
        self.process_info[label] = {
            'directory': directory,
            'config_file': config_file,
            'start_time': time.time(),
            'logfile': logfile
        }

        # Wait for initialization
        time.sleep(2)

        # Verify process is still running
        if process.poll() is not None:
            raise RuntimeError(f"Participant {label} terminated unexpectedly (exit code: {process.returncode})")

        print(f"Participant {label} started successfully (PID: {process.pid})")
        return process.pid

    def shutdown_participant(self, label: str, timeout: int = 5) -> bool:
        """
        Gracefully shutdown a participant using SIGTERM.

        Args:
            label: Participant label
            timeout: Maximum seconds to wait for shutdown (default: 5)

        Returns:
            True if shutdown successful, False if timeout/failure
        """
        if label not in self.processes:
            print(f"Participant {label} not found")
            return False

        if not self.is_running(label):
            print(f"Participant {label} is not running")
            return True

        process = self.processes[label]
        print(f"Sending SIGTERM to participant {label} (PID: {process.pid})")

        try:
            process.terminate()  # Send SIGTERM
            process.wait(timeout=timeout)
            print(f"Participant {label} shutdown successfully (exit code: {process.returncode})")

            # Close logfile
            if label in self.process_info and 'logfile' in self.process_info[label]:
                self.process_info[label]['logfile'].close()

            return True

        except subprocess.TimeoutExpired:
            print(f"Participant {label} did not shutdown within {timeout} seconds")
            return False

    def kill_participant(self, label: str) -> bool:
        """
        Force kill a participant using SIGKILL.

        Args:
            label: Participant label

        Returns:
            True if kill successful, False if already stopped
        """
        if label not in self.processes:
            print(f"Participant {label} not found")
            return False

        if not self.is_running(label):
            print(f"Participant {label} is already stopped")
            return True

        process = self.processes[label]
        print(f"Sending SIGKILL to participant {label} (PID: {process.pid})")

        try:
            process.kill()  # Send SIGKILL
            process.wait(timeout=2)
            print(f"Participant {label} killed (exit code: {process.returncode})")

            # Close logfile
            if label in self.process_info and 'logfile' in self.process_info[label]:
                self.process_info[label]['logfile'].close()

            return True

        except Exception as e:
            print(f"Error killing participant {label}: {e}")
            return False

    def restart_participant(self, label: str, directory: str = None, config_file: str = "rtps.ini") -> int:
        """
        Restart a participant (shutdown then start).

        Args:
            label: Participant label
            directory: Path to monitored directory (uses previous if None)
            config_file: DDS configuration file (uses previous if None)

        Returns:
            Process ID (PID) of restarted process

        Raises:
            RuntimeError: If restart fails
        """
        # Get previous config if not specified
        if directory is None and label in self.process_info:
            directory = self.process_info[label]['directory']
        if directory is None:
            raise RuntimeError(f"No directory specified for participant {label} restart")

        if config_file is None and label in self.process_info:
            config_file = self.process_info[label]['config_file']

        print(f"Restarting participant {label}")

        # Shutdown if running
        if self.is_running(label):
            success = self.shutdown_participant(label)
            if not success:
                print(f"Graceful shutdown failed, force killing participant {label}")
                self.kill_participant(label)

        # Start participant
        return self.start_participant(label, directory, config_file)

    def is_running(self, label: str) -> bool:
        """
        Check if a participant is currently running.

        Args:
            label: Participant label

        Returns:
            True if process is running, False otherwise
        """
        if label not in self.processes:
            return False

        process = self.processes[label]
        return process.poll() is None

    def get_process_id(self, label: str) -> Optional[int]:
        """
        Get the process ID for a participant.

        Args:
            label: Participant label

        Returns:
            Process ID or None if not running
        """
        if label in self.processes and self.is_running(label):
            return self.processes[label].pid
        return None

    def get_exit_code(self, label: str) -> Optional[int]:
        """
        Get the exit code of a stopped participant.

        Args:
            label: Participant label

        Returns:
            Exit code or None if still running or never started
        """
        if label not in self.processes:
            return None

        process = self.processes[label]
        return process.poll()

    def cleanup_all(self):
        """
        Stop all managed participant processes.

        This is typically called in test teardown to ensure clean state.
        """
        for label in list(self.processes.keys()):
            if self.is_running(label):
                print(f"Stopping participant {label} during cleanup")
                self.shutdown_participant(label)
