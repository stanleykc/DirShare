# DirShare - Distributed File Synchronization Example

## Overview

DirShare is an OpenDDS example demonstrating real-time file synchronization between multiple DDS participants. Each participant monitors a local directory and uses DDS publish-subscribe to propagate file changes (create, modify, delete) to other participants.

## Features

### Core Synchronization
- **Initial Directory Synchronization**: When participants join, existing files are automatically synchronized via DirectorySnapshot topic
- **Real-Time File Propagation**: File creation, modification, and deletion events propagate automatically within 5 seconds
- **Conflict Resolution**: Last-write-wins based on timestamps with millisecond precision
- **Notification Loop Prevention**: FileChangeTracker prevents infinite republishing loops when applying remote changes
- **Multi-Participant Support**: Supports 10+ simultaneous participants in a sharing session

### File Transfer
- **Large File Support**: Files up to 1GB with automatic chunking (1MB chunks for files >=10MB)
- **Small File Optimization**: Files <10MB transferred via FileContent topic (single message)
- **Integrity Verification**: CRC32 checksums ensure file integrity after transfer
- **Metadata Preservation**: File modification timestamps preserved across transfers
- **Binary File Support**: All file types supported via binary transfer

### Infrastructure
- **Dual Discovery Support**: Both InfoRepo and RTPS discovery mechanisms
- **Cross-Platform**: Works on Linux, macOS, and Windows
- **Polling-Based Monitoring**: FileMonitor polls directory every 1-2 seconds for changes

### Testing
- **Unit Tests**: Comprehensive Boost.Test coverage for all core components
- **Integration Tests**: Perl-based test runner for InfoRepo and RTPS modes
- **Acceptance Tests**: Robot Framework tests mapping to user stories

## Prerequisites

1. **OpenDDS Installed and Configured**: You need OpenDDS installed on your system
   - Download from https://opendds.org/
   - Run `./configure` from OpenDDS repository root
   - Build OpenDDS core: `make`

2. **Environment Configured**: Set up your environment to use OpenDDS
   ```bash
   # From your OpenDDS installation directory
   source setenv.sh  # Or setenv.cmd on Windows
   ```

3. **Perl Installed**: Required for test scripts and MPC build system

## Building

### Option 1: MPC Build (Primary)

```bash
# Ensure OpenDDS environment is sourced
# source <OpenDDS-install-dir>/setenv.sh

# Generate makefiles (first time or after .mpc changes)
mwc.pl -type gnuace

# Build
make

# Verify build
ls -lh dirshare
```

### Option 2: CMake Build (Alternative)

```bash
# Ensure OpenDDS environment is sourced
# source <OpenDDS-install-dir>/setenv.sh

mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Verify build
ls -lh dirshare
```

## Running Tests

DirShare includes three levels of testing:

### Unit Tests (Boost.Test)

Test individual components in isolation:

```bash
cd tests
make  # Build test executables

# Run all unit tests
perl run_tests.pl

# Run specific test suite
./ChecksumBoostTest
./FileUtilsBoostTest
./FileMonitorBoostTest
./FileChangeTrackerBoostTest
```

**Coverage**:
- **Checksum**: CRC32 calculation, incremental hashing, file-based checksums
- **FileUtils**: File I/O, timestamp preservation, error handling
- **FileMonitor**: Change detection, metadata extraction, polling behavior
- **FileChangeTracker**: Notification loop prevention, thread-safe operations

### Integration Tests (run_test.pl)

Test complete DirShare functionality with DDS:

#### InfoRepo Mode (Default)

```bash
perl run_test.pl
```

#### RTPS Mode

```bash
perl run_test.pl --rtps
```

### Acceptance Tests (Robot Framework)

Test user stories end-to-end:

```bash
cd robot

# Install dependencies (first time only)
python3 -m venv venv
source venv/bin/activate
python3 -m pip install -r requirements.txt

# Run all acceptance tests
robot UserStories.robot

# Run specific user story tests
robot --include us1 UserStories.robot  # Initial synchronization
robot --include us2 UserStories.robot  # File creation
robot --include us3 UserStories.robot  # File modification

# Run with RTPS discovery
robot --variable DISCOVERY_MODE:rtps UserStories.robot
```

**Test Coverage**:
- **US1**: Initial Directory Synchronization (3 scenarios)
- **US2**: Real-Time File Creation Propagation (3 scenarios)
- **US3**: Real-Time File Modification Propagation (3 scenarios)
- **US4**: Real-Time File Deletion Propagation (3 scenarios)
- **US6**: Metadata Transfer and Preservation (3 scenarios)

## Usage

### Basic Usage (RTPS Mode - Recommended)

RTPS mode is the simplest way to get started as it doesn't require a central InfoRepo server.

**Terminal 1: Start first participant**
```bash
mkdir /tmp/dirshare_a
echo "Hello from A" > /tmp/dirshare_a/fileA.txt
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
```

**Terminal 2: Start second participant**
```bash
mkdir /tmp/dirshare_b
echo "Hello from B" > /tmp/dirshare_b/fileB.txt
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_b
```

**Result**: Both directories will contain both files within a few seconds.

### InfoRepo Mode

**Terminal 1: Start DCPSInfoRepo**
```bash
$DDS_ROOT/bin/DCPSInfoRepo -o repo.ior
```

**Terminal 2: Start first participant**
```bash
./dirshare -DCPSInfoRepo file://repo.ior /tmp/dirshare_a
```

**Terminal 3: Start second participant**
```bash
./dirshare -DCPSInfoRepo file://repo.ior /tmp/dirshare_b
```

### RTPS Mode (No Central Server)

**Terminal 1: First participant**
```bash
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
```

**Terminal 2: Second participant**
```bash
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_b
```

## Command-Line Options

```
Usage: dirshare [OPTIONS] <shared_directory>

Arguments:
  shared_directory      Path to directory to synchronize

OpenDDS Options:
  -DCPSInfoRepo <ior>   InfoRepo IOR (file://path or corbaloc://...)
  -DCPSConfigFile <ini> Configuration file (e.g., rtps.ini for RTPS mode)
  -d <domain_id>        DDS domain ID (default: 42)
  -ORBDebugLevel <n>    ORB debug level (0-10)

DirShare Options:
  -v, --verbose         Enable verbose logging
  -h, --help            Show this help message

Examples:
  # InfoRepo mode
  dirshare -DCPSInfoRepo file://repo.ior /tmp/myshare

  # RTPS mode
  dirshare -DCPSConfigFile rtps.ini /tmp/myshare

  # Custom domain
  dirshare -DCPSConfigFile rtps.ini -d 50 /tmp/myshare
```

## Testing Real-Time Synchronization

### File Creation
```bash
# With DirShare running in /tmp/dirshare_a and /tmp/dirshare_b
echo "New file" > /tmp/dirshare_a/newfile.txt

# Check propagation (should appear within 5 seconds)
ls -l /tmp/dirshare_b/newfile.txt
```

### File Modification
```bash
echo "Updated content" >> /tmp/dirshare_a/fileA.txt

# Verify propagation
tail /tmp/dirshare_b/fileA.txt
```

### File Deletion
```bash
rm /tmp/dirshare_a/fileA.txt

# Verify propagation (should disappear within 5 seconds)
ls /tmp/dirshare_b/fileA.txt  # Should not exist
```

### Large File Transfer
```bash
# Create 50MB test file
dd if=/dev/urandom of=/tmp/dirshare_a/large.bin bs=1M count=50

# Monitor transfer (should complete within 30 seconds)
watch ls -lh /tmp/dirshare_b/large.bin
```

## Project Structure

```
DirShare/
├── DirShare.idl              # IDL data type definitions
├── DirShare.mpc              # MPC build configuration
├── CMakeLists.txt            # CMake build configuration
├── rtps.ini                  # RTPS discovery configuration
├── DirShare.cpp              # Main application
├── FileMonitor.h/cpp         # Directory polling and change detection
├── FileChangeTracker.h/cpp   # Notification loop prevention
├── Checksum.h/cpp            # CRC32 integrity verification
├── FileEventListenerImpl.h/cpp        # FileEvent listener
├── FileContentListenerImpl.h/cpp      # FileContent listener
├── FileChunkListenerImpl.h/cpp        # FileChunk listener
├── SnapshotListenerImpl.h/cpp         # DirectorySnapshot listener
├── tests/                    # Unit tests (Boost.Test)
│   ├── ChecksumBoostTest.cpp
│   ├── FileUtilsBoostTest.cpp
│   ├── FileMonitorBoostTest.cpp
│   ├── FileChangeTrackerBoostTest.cpp
│   ├── tests.mpc             # Test build configuration
│   └── run_tests.pl          # Test runner
├── robot/                    # Acceptance tests (Robot Framework)
│   ├── UserStories.robot     # User story test cases
│   ├── keywords/             # Robot Framework keywords
│   ├── libraries/            # Python support libraries
│   └── requirements.txt      # Python dependencies
├── specs/                    # Design documentation
│   └── 001-dirshare/        # Feature specifications and planning
├── LICENSE                   # OpenDDS license
├── README.md                 # This file
└── run_test.pl               # Integration test runner
```

## Architecture

### Data Types (IDL)

- **FileMetadata**: File properties (name, size, timestamp, checksum)
- **FileEvent**: File operation notifications (CREATE/MODIFY/DELETE)
- **FileContent**: Small file content (<10MB)
- **FileChunk**: Large file chunks (1MB chunks for files >=10MB)
- **DirectorySnapshot**: Initial directory state for synchronization

### DDS Topics

- `DirShare_FileEvents`: File operation notifications (QoS: Reliable, TransientLocal)
- `DirShare_FileContent`: Small file transfers (QoS: Reliable, Volatile)
- `DirShare_FileChunks`: Large file chunked transfers (QoS: Reliable, Volatile)
- `DirShare_DirectorySnapshot`: Initial directory snapshots (QoS: Reliable, TransientLocal)

### Components

#### Core Components
- **FileMonitor** (`FileMonitor.h/cpp`): Polls directory for changes (1-2 second interval)
  - Detects file creation, modification, and deletion
  - Extracts file metadata (size, timestamp)
  - Works with FileChangeTracker to prevent notification loops

- **FileChangeTracker** (`FileChangeTracker.h/cpp`): Prevents infinite notification loops
  - Tracks files being updated from remote sources
  - Thread-safe using ACE_Thread_Mutex
  - Integrated with FileMonitor and listeners

- **Checksum** (`Checksum.h/cpp`): CRC32 integrity verification
  - File-based and data-based checksum calculation
  - Incremental hashing support
  - Used for file integrity validation

- **FileUtils**: File I/O and timestamp preservation (embedded in DirShare.cpp)
  - Read/write operations with error handling
  - Modification timestamp preservation
  - Binary file support

#### DDS Listeners
- **FileEventListenerImpl** (`FileEventListenerImpl.h/cpp`): Receives file operation notifications
  - Handles CREATE, MODIFY, DELETE events
  - Coordinates with FileChangeTracker
  - Triggers appropriate file transfers

- **FileContentListenerImpl** (`FileContentListenerImpl.h/cpp`): Receives small file transfers
  - Handles files <10MB
  - Single-message transfer
  - Validates checksums

- **FileChunkListenerImpl** (`FileChunkListenerImpl.h/cpp`): Receives chunked file transfers
  - Handles files >=10MB in 1MB chunks
  - Reassembles chunks in sequence
  - Validates final checksum

- **SnapshotListenerImpl** (`SnapshotListenerImpl.h/cpp`): Receives initial directory snapshots
  - Processes DirectorySnapshot messages
  - Synchronizes existing files on startup
  - Coordinates initial state propagation

## Implementation Status

### Completed Features ✅
- ✅ **Phase 1-2: Foundation** - IDL, build system, DDS infrastructure, core utilities
- ✅ **US1: Initial Directory Synchronization** - DirectorySnapshot, FileContent, FileChunk transfer
- ✅ **US2: Real-Time File Creation** - FileEvent CREATE handling, notification loop prevention
- ✅ **US3: Real-Time File Modification** - FileEvent MODIFY, timestamp-based conflict resolution
- ✅ **US4: Real-Time File Deletion** - FileEvent DELETE propagation
- ✅ **US6: Metadata Preservation** - Timestamp preservation, size validation, extension handling

### Testing Coverage ✅
- ✅ **Unit Tests**: 100+ Boost.Test cases covering all core components
  - Checksum, FileUtils, FileMonitor, FileChangeTracker
  - FileEvent, FileContent, FileChunk, DirectorySnapshot
  - MetadataPreservation, TimestampComparison
- ✅ **Robot Framework**: 15+ acceptance test scenarios for US1-US4, US6
- ✅ **Integration Tests**: Perl-based test runner for InfoRepo and RTPS modes

### In Progress 🚧
- 🚧 **US5: Concurrent Modification Conflict Resolution** - Tie-breaker logic for simultaneous edits
- 🚧 **Phase 9: Polish** - Integration test runner, performance tests, documentation finalization

### Known Limitations
- **File Size**: Maximum 1GB per file
- **Directory Depth**: Single directory level (no recursive subdirectories)
- **Propagation Latency**: Target 5 seconds for files up to 10MB
- **Symbolic Links**: Ignored (not synchronized)
- **US5 Implementation**: Basic last-write-wins implemented, tie-breaker for identical timestamps pending

## Limitations

- **File Size**: Maximum 1GB per file
- **Directory Depth**: Single directory level (no recursive subdirectories)
- **Propagation Latency**: Target 5 seconds for files up to 10MB
- **Symbolic Links**: Ignored (not synchronized)

## Troubleshooting

### Files Not Synchronizing

1. Enable debug logging:
   ```bash
   export DCPS_debug_level=4
   ./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
   ```

2. Check for "publication matched" and "subscription matched" messages

3. Verify directory permissions:
   ```bash
   ls -ld /tmp/dirshare_a /tmp/dirshare_b
   ```

### Checksum Errors

- Ensure file is not being modified during transfer
- Check network quality (packet loss)
- Retry transfer (delete and recreate file)

### High Memory Usage

- Limit concurrent transfers of large files
- Restart DirShare if memory leak suspected

## Performance Expectations (LAN, 1 Gbps)

| Operation | File Size | Expected Time |
|-----------|-----------|---------------|
| Create/Propagate | 1 MB | < 1 second |
| Create/Propagate | 10 MB | < 5 seconds |
| Create/Propagate | 100 MB | < 30 seconds |
| Modify/Propagate | 1 MB | < 2 seconds |
| Delete/Propagate | Any | < 1 second |
| Initial Sync | 100 files (1 MB each) | < 30 seconds |

## Development

### Adding New Features

1. **Update IDL** (`DirShare.idl`): Add new data types if needed
2. **Implement Component**: Create new .h/.cpp files following OpenDDS conventions
3. **Add Unit Tests**: Create Boost.Test file in `tests/` directory
4. **Update MPC**: Add new files to `DirShare.mpc` and `tests/tests.mpc`
5. **Test**: Run unit tests, integration tests, and acceptance tests

### Code Style

- Follow OpenDDS coding conventions
- Use ACE logging macros (`ACE_ERROR`, `ACE_DEBUG`)
- Include proper error handling and cleanup
- Document public interfaces with comments
- Use `ACE_Thread_Mutex` for thread safety

### Debugging

Enable verbose DDS logging:
```bash
export DCPS_debug_level=4
export DCPS_transport_debug_level=4
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
```

### Running Linter

OpenDDS provides a code linter:
```bash
perl $DDS_ROOT/tools/scripts/lint.pl --path .
```

## Design Documents

Detailed design documentation is available in `specs/001-dirshare/`:
- **spec.md**: Feature specification with user stories and requirements
- **plan.md**: Implementation plan and architecture decisions
- **data-model.md**: IDL data structures and relationships
- **contracts/topics.md**: DDS topic definitions and QoS policies
- **research.md**: Technology research and design decisions
- **tasks.md**: Implementation task breakdown

## Further Reading

- **OpenDDS Developer's Guide**: `$DDS_ROOT/docs/OpenDDS_Developer_Guide.pdf` (from your OpenDDS installation)
- **Design Documents**: `specs/001-dirshare/` in this repository
- **OpenDDS Website**: https://opendds.org/
- **DDS Specification**: https://www.omg.org/spec/DDS/
- **Boost.Test Documentation**: https://www.boost.org/doc/libs/release/libs/test/
- **Robot Framework**: https://robotframework.org/

## License

This project is derived from OpenDDS examples and uses the OpenDDS license. See the LICENSE file for details.

DirShare demonstrates real-time file synchronization using the OpenDDS implementation of the Data Distribution Service (DDS) specification.
