"""
Configuration constants for DirShare robustness testing.

This module defines timeout values, file sizes, and test parameters
based on the feature specification (specs/002-robustness-testing/spec.md).

All timeout values are per FR-017:
- Single participant restart: 10 seconds
- Multiple/rolling restarts: 30 seconds
- Sender restart during large file transfer: 60 seconds
"""

# Timeout values (in seconds) per FR-017
TIMEOUTS = {
    'single_restart': 10,        # Single participant restart (US1.1, US1.2)
    'multiple_restart': 30,      # Multiple participants or rolling restarts (US1.3, US2.x, US3.x)
    'large_file_transfer': 60,   # Sender restart during large file transfer (US4.3)
    'receiver_restart': 15,      # Receiver restart during large file transfer (US4.1, US4.2)
}

# File sizes for test scenarios (in MB)
FILE_SIZES = {
    'small': 5,          # US1.2 - 5MB file
    'medium': 15,        # US4.1 - 15MB file (chunked transfer)
    'large': 20,         # US4.2 - 20MB file (chunked transfer)
    'extra_large': 25,   # US4.3 - 25MB file (chunked transfer)
}

# Test file generation parameters
TEST_FILE_CHUNK_SIZE = 1024  # Bytes per chunk for reproducible content
DEFAULT_CONTENT_SEED = 42     # Default seed for content generation

# DirShare configuration
DIRSHARE_EXECUTABLE = './dirshare'
RTPS_CONFIG = 'rtps.ini'

# Test directory parameters
TEST_DIR_PREFIX = 'dirshare_robustness_test'
CLEANUP_ON_SUCCESS = True    # Clean up test directories after successful tests
CLEANUP_ON_FAILURE = False   # Preserve test directories on failure for debugging

# Participant labels
PARTICIPANT_LABELS = ['A', 'B', 'C']

# Polling intervals (in seconds)
SYNC_POLL_INTERVAL = 1       # Poll every 1 second when waiting for synchronization
FILE_CHECK_INTERVAL = 0.5    # Check for file existence every 0.5 seconds

# Process management
SHUTDOWN_TIMEOUT = 5         # Maximum seconds to wait for graceful shutdown
STARTUP_WAIT = 2             # Seconds to wait after starting a participant
DISCOVERY_WAIT = 3           # Seconds to wait for DDS discovery

# Validation thresholds
TIMESTAMP_TOLERANCE = 5      # Maximum seconds difference for modification time comparison
PASS_RATE_THRESHOLD = 0.95   # 95% pass rate requirement (SC-004)

# Performance targets
MAX_SUITE_DURATION = 900     # 15 minutes (900 seconds) for complete test suite (SC-006)
