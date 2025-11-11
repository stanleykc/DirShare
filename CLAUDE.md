# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DirShare is an OpenDDS demonstration of real-time distributed file synchronization using DDS (Data Distribution Service) publish-subscribe. It monitors a local directory and propagates file changes (create, modify, delete) to other DDS participants in real-time.

**Key Technology**: Built on OpenDDS - requires OpenDDS environment to be sourced before building or running.

## Environment Setup

### Required Before Any Work

```bash
# Source OpenDDS environment (CRITICAL - must be done first)
source $DDS_ROOT/setenv.sh

# Verify environment is configured
echo $DDS_ROOT
echo $ACE_ROOT
```

All build and test commands require the OpenDDS environment to be active.

## Build System

This project uses **MPC (Make Project Creator)**, OpenDDS's build configuration system, NOT traditional Makefiles.

### Primary Build Method (MPC)

```bash
# Generate makefiles from .mpc files (required after any .mpc changes)
mwc.pl -type gnuace

# Build everything (library + executable)
make

# Verify build
ls -lh dirshare libDirShare.dylib
```

### Alternative Build (CMake)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

**Important**: The `.mpc` files (`DirShare.mpc`, `tests/tests.mpc`) are the source of truth for build configuration. Generated `GNUmakefile.*` files should not be edited manually.

## Testing Strategy

DirShare has three distinct test levels - understand which to use:

### 1. Unit Tests (Boost.Test Framework)

Tests individual components in isolation. Located in `tests/` directory.

```bash
cd tests

# Build tests (if not already built)
make

# Run all unit tests
perl run_tests.pl

# Run specific test executable
./ChecksumBoostTest
./FileUtilsBoostTest
./FileMonitorBoostTest
./FileChangeTrackerBoostTest

# Run with detailed logging
./ChecksumBoostTest --log_level=all
```

**Covered Components**:
- `Checksum.h/cpp` - CRC32 integrity checking
- `FileUtils.h/cpp` - File I/O operations
- `FileMonitor.h/cpp` - Directory polling and change detection
- `FileChangeTracker.h/cpp` - Notification loop prevention

### 2. Robot Framework Acceptance Tests

End-to-end tests validating user stories with actual DDS communication.

```bash
cd robot

# One-time setup (if not done)
python3 -m venv venv
source venv/bin/activate
python3 -m pip install -r requirements.txt

# Before each test session (MUST activate venv)
source venv/bin/activate

# Run all acceptance tests
robot UserStories.robot

# Run specific user story
robot --test "US1*" UserStories.robot

# Run with RTPS discovery mode
robot --variable DISCOVERY_MODE:rtps UserStories.robot

# When done
deactivate
```

**Test Coverage**: US1 (initial sync), US2 (file creation), US3 (file modification)

### 3. Integration Tests

There is no `run_test.pl` in the root directory currently - integration testing is handled via Robot Framework.

## Architecture

### DDS Data Model (DirShare.idl)

Five core data structures define the synchronization protocol:

1. **FileMetadata** - File properties (name, size, timestamp, checksum)
2. **FileEvent** - Operation notifications (CREATE/MODIFY/DELETE)
3. **FileContent** - Complete file data for small files (<10MB)
4. **FileChunk** - 1MB chunks for large files (>=10MB)
5. **DirectorySnapshot** - Initial directory state for new participants

### DDS Topics

- `DirShare_FileEvents` - RELIABLE, TRANSIENT_LOCAL (persistent)
- `DirShare_FileContent` - RELIABLE, VOLATILE
- `DirShare_FileChunks` - RELIABLE, VOLATILE
- `DirShare_DirectorySnapshot` - RELIABLE, TRANSIENT_LOCAL

### Core Components

**FileMonitor** (`FileMonitor.h/cpp`)
- Polls directory every 2 seconds for changes
- Detects file creation, modification, deletion
- Works with FileChangeTracker to prevent notification loops

**FileChangeTracker** (`FileChangeTracker.h/cpp`)
- Prevents infinite notification loops when applying remote changes
- Thread-safe using ACE_Thread_Mutex
- Marks files as "being updated" to suppress local change detection

**Checksum** (`Checksum.h/cpp`)
- CRC32 integrity verification for file transfers
- Incremental hashing support

**FileUtils** (`FileUtils.h/cpp`)
- File I/O with timestamp preservation
- Binary file support
- Path validation (prevents traversal attacks)

**DDS Listeners**
- `FileEventListenerImpl` - Receives CREATE/MODIFY/DELETE notifications
- `FileContentListenerImpl` - Receives small file transfers
- `FileChunkListenerImpl` - Receives and reassembles large file chunks
- `SnapshotListenerImpl` - Processes initial directory snapshots

### Discovery Modes

**RTPS Mode (Recommended)**: Peer-to-peer discovery, no central server required
```bash
./dirshare -DCPSConfigFile rtps.ini /path/to/shared_dir
```

**InfoRepo Mode**: Centralized discovery via DCPSInfoRepo server
```bash
# Terminal 1: Start InfoRepo
$DDS_ROOT/bin/DCPSInfoRepo -o repo.ior

# Terminal 2+: Start participants
./dirshare -DCPSInfoRepo file://repo.ior /path/to/shared_dir
```

## Running DirShare

### Basic Usage

```bash
# Create test directories
mkdir /tmp/dirshare_a /tmp/dirshare_b

# Terminal 1: First participant (RTPS mode)
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a

# Terminal 2: Second participant
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_b

# Test synchronization
echo "Hello" > /tmp/dirshare_a/test.txt
# File should appear in /tmp/dirshare_b within 5 seconds
```

### Debugging

```bash
# Enable DDS debug logging
export DCPS_debug_level=4
export DCPS_transport_debug_level=4
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
```

## Code Style and Conventions

- Follow OpenDDS coding conventions (ACE/TAO style)
- Use ACE logging macros: `ACE_DEBUG`, `ACE_ERROR`, `ACE_ERROR_RETURN`
- Thread safety: Use `ACE_Thread_Mutex` for shared state
- Error handling: Always check return codes, use proper cleanup
- Documentation: Comment public interfaces and non-obvious logic

## Development Workflow

### Adding a New Feature

1. **Update IDL** (`DirShare.idl`) if new data types needed
2. **Regenerate build files**: `mwc.pl -type gnuace`
3. **Implement component** (create .h/.cpp files)
4. **Add to MPC**: Update `DirShare.mpc` to include new files
5. **Write unit tests**: Create Boost.Test file in `tests/`
6. **Update test build**: Add to `tests/tests.mpc`
7. **Build and test**: `make && cd tests && make && perl run_tests.pl`

### Modifying Existing Components

- Unit tests serve as specification - run them frequently
- FileMonitor, FileChangeTracker, and Checksum have comprehensive test coverage
- Always test both RTPS and InfoRepo modes for DDS changes

## Common Issues

### "DCPSInfoRepo not found"
OpenDDS environment not sourced. Run: `source $DDS_ROOT/setenv.sh`

### "error while loading shared libraries"
Build artifacts not found. Ensure you ran `make` successfully and `libDirShare.dylib` exists.

### Files Not Synchronizing
1. Enable debug logging: `export DCPS_debug_level=4`
2. Look for "publication matched" and "subscription matched" messages
3. Check firewall/network for RTPS multicast blocking
4. Verify directory permissions

### Build Fails After Changing .mpc
Must regenerate makefiles: `mwc.pl -type gnuace && make clean && make`

## Design Documentation

The `specs/001-dirshare/` directory contains:
- `spec.md` - Feature specification and user stories
- `plan.md` - Implementation plan and architecture
- `data-model.md` - IDL data structures
- `tasks.md` - Implementation task breakdown
- `contracts/topics.md` - DDS topic definitions

These documents are useful for understanding design decisions and requirements.

## Key Files

- `DirShare.idl` - Data type definitions (source of truth for data model)
- `DirShare.mpc` - Build configuration (source of truth for build)
- `DirShare.cpp` - Main application and DDS setup
- `rtps.ini` - RTPS discovery configuration
- `tests/tests.mpc` - Test build configuration
- `robot/UserStories.robot` - Acceptance test cases
