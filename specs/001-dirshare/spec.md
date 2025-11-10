# Feature Specification: DirShare - Distributed File Synchronization

**Feature Branch**: `001-dirshare`
**Created**: 2025-10-30
**Status**: Draft
**Input**: User description: "DirShare is a simple example that demonstrates how to share files between DDS participants. This example consists of a publisher that shares files from a specified directory and a subscriber that receives and saves those files to a local directory. It supports establishing a directory sharing session, during which the folder in which the DirShare application/utility on your local workstation to share with the remote participants who have also run on their systems. Changes to the folder on either the remote desktop or your workstation are reflected on both machines for the duration of the session. So whenever a file is created, modified, or deleted in the shared directory on one side, the change is propagated to the other side. If a file is modified simultaneously on both sides, the last modification takes precedence. It should manage file transfers efficiently, ensuring that only changed files are transmitted to minimize bandwidth usage. It should support the transfer of metadata associated with the files, such as file names, sizes, and timestamps as well as the file contents. This will allow for future features where users can filter or sort files based on their metadata before synchronizing them."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Initial Directory Synchronization (Priority: P1)

A user starts DirShare on two machines (local and remote) and establishes a sharing session for a specific directory. Upon connection, all existing files in the shared directory on both sides are synchronized so both directories contain the same set of files.

**Why this priority**: This is the core functionality that enables basic file sharing between participants. Without this, no file synchronization can occur. This represents the minimum viable product.

**Independent Test**: Can be fully tested by starting two DirShare instances pointing to different local directories with pre-existing files, and verifying that both directories contain all files from both sources after synchronization completes.

**Acceptance Scenarios**:

1. **Given** Machine A has a directory with 3 files (A1.txt, A2.txt, A3.txt) and Machine B has a directory with 2 files (B1.txt, B2.txt), **When** both machines start DirShare and establish a sharing session, **Then** both directories contain all 5 files (A1.txt, A2.txt, A3.txt, B1.txt, B2.txt) with identical content
2. **Given** Machine A has an empty directory and Machine B has a directory with 5 files, **When** sharing session is established, **Then** Machine A's directory contains all 5 files from Machine B
3. **Given** both machines have identical files in their directories, **When** sharing session is established, **Then** no file transfers occur and both directories remain unchanged

---

### User Story 2 - Real-Time File Creation Propagation (Priority: P1)

During an active sharing session, when a user creates a new file in the shared directory on one machine, the file is automatically transferred to all other participants' shared directories.

**Why this priority**: This is essential for real-time collaboration and demonstrates the publish-subscribe nature of DDS. It builds directly on P1 by adding dynamic behavior.

**Independent Test**: Can be tested independently by establishing a sharing session between two DirShare instances, creating a new file in one directory, and verifying it appears in the other directory within a reasonable timeframe (e.g., under 5 seconds).

**Acceptance Scenarios**:

1. **Given** an active sharing session between Machine A and Machine B, **When** a user creates a new file "document.txt" in Machine A's shared directory, **Then** the file appears in Machine B's shared directory within 5 seconds with identical content
2. **Given** an active sharing session with 3 participants (A, B, C), **When** Machine A creates a new file, **Then** both Machine B and Machine C receive the file
3. **Given** an active sharing session, **When** a user creates a large file (100MB) in the shared directory, **Then** the file is transferred completely and verifiable via checksum
4. **Given** an active sharing session between Machine A and Machine B, **When** Machine A creates a file and Machine B receives and applies the change, **Then** Machine B does NOT send a new FileEvent notification back to Machine A (preventing notification loops)

---

### User Story 3 - Real-Time File Modification Propagation (Priority: P1)

During an active sharing session, when a user modifies an existing file in the shared directory on one machine, only the changed file is retransmitted to all other participants, minimizing bandwidth usage.

**Why this priority**: This is critical for efficiency and practical usability. Without delta detection, every change would require full file retransmission, making the system impractical for larger files or frequent updates.

**Independent Test**: Can be tested by establishing a sharing session, modifying a file on one machine, and verifying (via network monitoring or logs) that only the changed file is transmitted, not all files.

**Acceptance Scenarios**:

1. **Given** an active sharing session with a synchronized file "data.txt", **When** a user modifies "data.txt" on Machine A, **Then** only "data.txt" is retransmitted to Machine B (not other unchanged files)
2. **Given** an active sharing session with 10 files, **When** a user modifies 1 file, **Then** network transfer occurs only for that 1 file
3. **Given** an active sharing session, **When** a file is modified on Machine A at timestamp T1, **Then** Machine B receives the updated file with the modification timestamp preserved
4. **Given** an active sharing session, **When** Machine A modifies a file and Machine B receives and applies the modification, **Then** Machine B does NOT publish this change (preventing notification loops and additional unnecessary propagation)

---

### User Story 4 - Real-Time File Deletion Propagation (Priority: P2)

During an active sharing session, when a user deletes a file from the shared directory on one machine, the file is automatically deleted from all other participants' shared directories.

**Why this priority**: This completes the CRUD (Create, Read, Update, Delete) operations for file synchronization. While important for complete functionality, the system can still provide value with just create/modify operations.

**Independent Test**: Can be tested by establishing a sharing session with synchronized files, deleting a file on one machine, and verifying it is removed from the other machine's directory.

**Acceptance Scenarios**:

1. **Given** an active sharing session with synchronized file "temp.txt", **When** a user deletes "temp.txt" from Machine A's shared directory, **Then** "temp.txt" is deleted from Machine B's shared directory within 5 seconds
2. **Given** an active sharing session with 5 synchronized files, **When** a user deletes 2 files from Machine A, **Then** only those 2 files are deleted from Machine B (the other 3 remain)
3. **Given** an active sharing session with 3 participants, **When** Machine A deletes a file, **Then** the file is deleted from both Machine B and Machine C

---

### User Story 5 - Concurrent Modification Conflict Resolution (Priority: P2)

When the same file is modified simultaneously on multiple machines, the system resolves the conflict by keeping the version with the latest timestamp, ensuring eventual consistency across all participants.

**Why this priority**: This handles an important edge case for real-world usage where multiple users might edit the same file. While not core to basic functionality, it prevents data loss and confusion.

**Independent Test**: Can be tested by modifying the same file on two machines at nearly the same time, and verifying that all participants converge to the same version (the one with the later timestamp).

**Acceptance Scenarios**:

1. **Given** an active sharing session with file "shared.txt", **When** Machine A modifies "shared.txt" at T1 and Machine B modifies "shared.txt" at T2 (where T2 > T1), **Then** all participants end up with Machine B's version
2. **Given** an active sharing session, **When** both machines modify the same file within a very short time window (< 1 second apart), **Then** the system deterministically selects one version based on timestamp (to millisecond precision) and all participants converge to that version
3. **Given** a conflict scenario where Machine A's version wins, **When** synchronization completes, **Then** Machine B's local modifications are overwritten and both machines have identical file content

---

### User Story 6 - Metadata Transfer and Preservation (Priority: P3)

When files are transferred between participants, their metadata (filename, size, timestamps, file type) is transferred along with the file content and preserved on the receiving side.

**Why this priority**: This enables future extensibility (filtering, sorting) and provides better user experience by preserving file properties. However, the core file-sharing functionality works without this.

**Independent Test**: Can be tested by creating a file with specific metadata (modification timestamp, etc.) on one machine, transferring it via DirShare, and verifying the metadata is identical on the receiving machine.

**Acceptance Scenarios**:

1. **Given** Machine A has a file "photo.jpg" with modification timestamp "2025-10-30 10:00:00", **When** the file is transferred to Machine B, **Then** Machine B's copy has the same modification timestamp "2025-10-30 10:00:00"
2. **Given** a file with a specific size (e.g., 5,242,880 bytes), **When** the file is transferred, **Then** the receiving side can verify the file size before accepting the transfer
3. **Given** files with various extensions (.txt, .jpg, .pdf, .docx), **When** transferred, **Then** all filename extensions are preserved correctly

---

### Edge Cases

- **What happens when network connection is lost during file transfer?** The partially transferred file should be discarded or marked as incomplete, and the transfer should be retried when connection is re-established.
- **What happens when the shared directory is deleted during an active session?** The application should detect the missing directory and either gracefully terminate the session or notify the user with an error message.
- **What happens when disk space is insufficient on the receiving side?** The application should detect insufficient space before attempting the transfer and notify the user, skipping that file transfer.
- **What happens when a file is locked or in use on the receiving side?** The application should retry the operation after a short delay or notify the user that the file cannot be updated.
- **What happens when a file exceeds maximum file size limit?** The system should either reject files above a certain threshold (assume 1GB for this example) or support chunked transfer for very large files.
- **What happens when a participant joins an active session after files have been modified?** The new participant should receive the current state of all files in the shared directory.
- **What happens when files contain special characters or Unicode in filenames?** The system should properly encode and preserve Unicode filenames across transfers.
- **What happens when symbolic links exist in the shared directory?** Symbolic links should be ignored completely and not synchronized. This avoids potential security risks (following links outside the shared directory) and complexity (circular links, cross-platform compatibility issues).
- **What happens when a participant receives a file change and applies it locally?** The system must distinguish this from a user-initiated local change. When applying a remotely-received change, the local file system monitor may detect the change, but the system MUST NOT republish this as a new FileEvent, preventing infinite notification loops between participants.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST detect and publish all file system events (CREATE, MODIFY, DELETE) in the shared directory to all DDS participants via the FileEvents topic
- **FR-002**: System MUST subscribe to file events from other DDS participants and apply changes to the local shared directory
- **FR-003**: System MUST transfer file content along with file metadata (name, size, modification timestamp) when sharing files
- **FR-004**: System MUST perform initial full synchronization via DirectorySnapshot when a new participant joins, then use delta synchronization (only changed files) for ongoing changes during the session
- **FR-005**: System MUST resolve concurrent modification conflicts by applying the change with the latest timestamp
- **FR-006**: System MUST support multiple simultaneous participants in a sharing session (minimum 2, recommended 10+)
- **FR-007**: System MUST preserve file modification timestamps when transferring files between participants
- **FR-008**: System MUST support both RTPS and InfoRepo discovery mechanisms (as per OpenDDS standard patterns)
- **FR-009**: System MUST provide automated testing at three levels: (1) unit tests via Boost.Test, (2) integration tests via run_test.pl for both discovery modes, (3) acceptance tests via Robot Framework mapping to user stories
- **FR-010**: Users MUST be able to specify the directory to share via command-line argument
- **FR-011**: System MUST validate that the specified directory exists before starting the sharing session
- **FR-012**: System MUST gracefully handle cleanup when the session terminates (close file handles, cleanup DDS entities)
- **FR-013**: System MUST support binary file transfers (not just text files)
- **FR-014**: System MUST verify file integrity after transfer using CRC32 checksums
- **FR-015**: System MUST handle file paths with spaces, Unicode characters, and special symbols correctly
- **FR-016**: System MUST detect errors (disk full, permission denied, file locked, etc.) and report them via ACE_ERROR logging to stderr without crashing the application
- **FR-017**: System MUST distinguish between locally-initiated file changes and remotely-received file changes to prevent notification loops (when a participant applies a file change received from a remote source, it MUST NOT republish that change as a new FileEvent)

### Key Entities *(include if feature involves data)*

- **FileEvent**: Represents a file system event that needs to be published to other participants
  - Type (CREATE, MODIFY, DELETE)
  - File path relative to shared directory
  - Timestamp of the event
  - Associated file metadata

- **FileMetadata**: Information about a file being shared
  - Filename (including extension)
  - File size in bytes
  - Modification timestamp
  - File type/extension
  - Checksum or hash (for integrity verification)

- **FileContent**: The actual binary content of a file being transferred
  - Binary data payload
  - Chunk sequence number (for large file support)
  - Total chunks (for reassembly)
  - Associated FileMetadata reference

- **DirectorySnapshot**: State of the shared directory at a point in time
  - List of all files with their metadata
  - Directory path
  - Snapshot timestamp
  - Used for initial synchronization when a participant joins

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can establish a sharing session and synchronize an initial set of files (up to 100 files) within 30 seconds
- **SC-002**: File creation on one machine appears on other participants' machines within 5 seconds under normal network conditions
- **SC-003**: File modifications propagate to other participants within 5 seconds for files up to 10MB under normal network conditions (defined as: <50ms latency, >10Mbps bandwidth, <1% packet loss)
- **SC-004**: System efficiently handles file synchronization by transmitting only changed files, reducing bandwidth by at least 80% compared to re-transmitting all files (measurement: in a scenario with 10 synchronized files where 1 file is modified, verify only 1 file is transferred, not all 10)
- **SC-005**: System successfully resolves 100% of concurrent modification conflicts without data corruption or crashes
- **SC-006**: System supports at least 10 simultaneous participants in a sharing session while maintaining <5 second file propagation time (no significant performance degradation defined as: propagation time remains within P1 requirements)
- **SC-007**: File integrity is maintained with 100% accuracy - all transferred files have identical content and metadata to the source
- **SC-008**: System handles at least 1000 file operations (create, modify, delete) during a session without memory leaks or crashes
- **SC-009**: Users can successfully share files with names containing Unicode characters and special symbols without corruption
- **SC-010**: System operates correctly with both RTPS and InfoRepo discovery mechanisms, passing all test scenarios in both modes
- **SC-011**: System prevents notification loops - when changes are propagated from a source to participants, zero duplicate FileEvent notifications are sent back to the originator (measurable via DDS traffic monitoring or debug logging)

## Assumptions *(mandatory)*

1. **Network Connectivity**: Participants are connected on the same network or have routable network paths (LAN or properly configured WAN)
2. **File System Support**: All participants use file systems that support standard file operations (create, read, write, delete) and metadata (timestamps)
3. **Permissions**: DirShare application has read/write permissions for the specified shared directory on all machines
4. **Synchronous Clocks**: Participant machines have reasonably synchronized system clocks (within a few seconds) for timestamp-based conflict resolution
5. **DDS Infrastructure**: OpenDDS environment is properly configured with ACE/TAO dependencies available
6. **File System Monitoring**: The underlying operating system provides file system change notification mechanisms (or polling is acceptable for this example)
7. **Storage Capacity**: Receiving participants have sufficient disk space to accommodate all shared files
8. **Default File Size Limit**: Individual files are assumed to be under 1GB unless chunked transfer is implemented
9. **Session Duration**: Sharing sessions are intended for temporary synchronization (minutes to hours), not permanent long-running services
10. **File Types**: All file types are supported as binary transfers without content inspection or transformation
11. **Directory Structure**: Only a single directory level is shared (no recursive subdirectory synchronization) unless explicitly specified
12. **Discovery Mechanism**: Participants will use the same discovery mechanism (all RTPS or all InfoRepo) within a session

## Out of Scope *(mandatory)*

- Recursive subdirectory synchronization (only top-level directory)
- Encryption or authentication of file transfers (security features)
- Compression of file content before transmission
- Bandwidth throttling or QoS prioritization
- Versioning or history of file changes
- Selective file synchronization based on filters (e.g., "only sync .txt files")
- Graphical user interface (CLI/command-line only)
- Conflict resolution strategies beyond "last-write-wins"
- Cross-platform path handling for deeply nested directories (focus on simple paths)
- Integration with existing file synchronization services (Dropbox, Google Drive, etc.)

## Dependencies *(mandatory)*

- **OpenDDS**: Core DDS implementation for publish-subscribe messaging
- **ACE/TAO**: Platform abstraction and CORBA ORB (auto-configured via OpenDDS)
- **Boost**: Boost.Test framework for unit testing
- **Perl**: Required for integration test runner scripts (run_test.pl)
- **Python 3.8+**: Required for Robot Framework acceptance testing
- **Robot Framework 6.0+**: Acceptance testing framework with Process and OperatingSystem libraries
- **File System API**: Standard OS file I/O capabilities
- **File System Monitoring**: OS-specific APIs or polling mechanisms to detect file changes (inotify on Linux, FSEvents on macOS, ReadDirectoryChangesW on Windows, or simple polling)
- **MPC Build System**: For generating makefiles and build configuration
- **CMake** (optional): Alternative build system support

## Technical Constraints *(if applicable)*

- Must follow OpenDDS example patterns and conventions (similar to Messenger example)
- Must use IDL to define data types for FileEvent, FileMetadata, and FileContent
- Must support both InfoRepo and RTPS discovery mechanisms with configuration via INI files
- Must implement proper DDS cleanup (delete_contained_entities, shutdown) to avoid resource leaks
- Must provide both MPC and CMake build configurations
- Should use ACE logging macros for error handling and debug output
- Must follow OpenDDS DevGuideExamples directory structure and naming conventions
- File transfer payload size must be compatible with DDS message size limits: files >=10MB MUST use chunking (1MB chunks), with 1GB maximum file size per Assumption #8
