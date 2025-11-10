# Data Model: DirShare IDL Definitions

**Feature**: DirShare - Distributed File Synchronization
**Date**: 2025-10-30
**Purpose**: Define IDL data structures for file synchronization

## Overview

The DirShare data model consists of three primary data types for file synchronization:
1. **FileMetadata**: Describes file properties (name, size, timestamps, checksum)
2. **FileContent**: Contains small file data (<10MB) as single message
3. **FileChunk**: Contains large file data (>=10MB) split into 1MB chunks

All types use OMG IDL syntax and follow OpenDDS conventions with `@topic` and `@key` annotations.

---

## IDL Definitions

### DirShare.idl

```idl
// DirShare.idl - Data types for distributed file synchronization

module DirShare {

  // Enumeration for file operation types
  enum OperationType {
    CREATE,   // File was created
    MODIFY,   // File was modified
    DELETE    // File was deleted
  };

  // File metadata structure
  // Contains file properties without the actual content
  @topic
  struct FileMetadata {
    @key string filename;              // Relative path within shared directory
    unsigned long long size;           // File size in bytes
    unsigned long long timestamp_sec;  // Modification time (seconds since epoch)
    unsigned long timestamp_nsec;      // Modification time (nanoseconds part)
    unsigned long checksum;            // CRC32 checksum of file content
  };

  // File event structure
  // Used to signal file operations (create, modify, delete)
  @topic
  struct FileEvent {
    @key string filename;              // Relative path within shared directory
    OperationType operation;           // Type of operation
    unsigned long long timestamp_sec;  // Event timestamp (seconds)
    unsigned long timestamp_nsec;      // Event timestamp (nanoseconds)
    FileMetadata metadata;             // Associated file metadata (empty for DELETE)
  };

  // File content structure (for small files < 10MB)
  // Contains the actual file data as a single message
  @topic
  struct FileContent {
    @key string filename;              // Relative path within shared directory
    sequence<octet> data;              // File content as binary data
    unsigned long long size;           // File size (redundant with data.length, for validation)
    unsigned long checksum;            // CRC32 checksum for integrity verification
    unsigned long long timestamp_sec;  // Modification time (seconds)
    unsigned long timestamp_nsec;      // Modification time (nanoseconds)
  };

  // File chunk structure (for large files >= 10MB)
  // Large files are split into 1MB chunks for efficient transfer
  @topic
  struct FileChunk {
    @key string filename;              // Relative path within shared directory
    @key unsigned long chunk_id;       // Chunk sequence number (0-based)
    sequence<octet> data;              // Chunk data (max 1MB)
    unsigned long total_chunks;        // Total number of chunks for this file
    unsigned long long file_size;      // Total file size (all chunks)
    unsigned long file_checksum;       // CRC32 checksum of complete file
    unsigned long chunk_checksum;      // CRC32 checksum of this chunk
    unsigned long long timestamp_sec;  // File modification time (seconds)
    unsigned long timestamp_nsec;      // File modification time (nanoseconds)
  };

  // Directory snapshot structure
  // Used for initial synchronization when a participant joins
  @topic
  struct DirectorySnapshot {
    @key string participant_id;        // Unique ID of the participant sending snapshot
    sequence<FileMetadata> files;      // List of all files in shared directory
    unsigned long long snapshot_time_sec;   // Snapshot timestamp (seconds)
    unsigned long snapshot_time_nsec;       // Snapshot timestamp (nanoseconds)
    unsigned long file_count;          // Number of files in snapshot
  };

};
```

---

## Data Type Details

### FileMetadata

**Purpose**: Describe file properties without transferring content. Used for comparison and change detection.

**Key Fields**:
- `filename` (key): Uniquely identifies the file within the shared directory

**Attributes**:
- `size`: File size in bytes, used for validation and chunking decisions
- `timestamp_sec/nsec`: POSIX timestamp (seconds + nanoseconds) for modification time comparison
- `checksum`: CRC32 checksum for integrity verification

**Usage**:
- Embedded in `FileEvent` to communicate file properties during operations
- Used for initial directory comparison during synchronization
- Compared with local file metadata to detect changes

**Validation Rules**:
- `filename` MUST NOT be empty
- `filename` MUST NOT contain path traversal sequences ("../", "..\\")
- `size` MUST match actual file size
- `checksum` MUST be valid CRC32 value (computed before sending)

---

### FileEvent

**Purpose**: Signal file operations (create, modify, delete) to all participants.

**Key Fields**:
- `filename` (key): Identifies which file the operation applies to

**Attributes**:
- `operation`: Type of operation (CREATE, MODIFY, DELETE)
- `timestamp_sec/nsec`: When the operation occurred (for conflict resolution)
- `metadata`: File properties (empty/invalid for DELETE operations)

**Usage**:
- Published when local file system change is detected (but NOT when applying remote changes to prevent loops)
- Subscribed by all participants to apply remote changes
- Triggers `FileContent` or `FileChunk` transfer for CREATE/MODIFY operations
- Implementation must distinguish locally-initiated changes from remotely-received changes (FR-017, SC-011)

**Validation Rules**:
- `filename` MUST NOT be empty
- `operation` MUST be one of: CREATE, MODIFY, DELETE
- For CREATE/MODIFY: `metadata` MUST be populated with valid values
- For DELETE: `metadata` may be empty (only `filename` and `operation` matter)

**State Transitions**:
- Local file created → Publish FileEvent(CREATE) → Publish FileContent/FileChunk
- Local file modified → Publish FileEvent(MODIFY) → Publish FileContent/FileChunk
- Local file deleted → Publish FileEvent(DELETE)
- Remote FileEvent(CREATE) received → Request FileContent/FileChunk → Write local file
- Remote FileEvent(MODIFY) received → Compare timestamps → Apply if newer
- Remote FileEvent(DELETE) received → Compare timestamps → Delete local file if newer

---

### FileContent

**Purpose**: Transfer small files (<10MB) as a single DDS message.

**Key Fields**:
- `filename` (key): Identifies the file being transferred

**Attributes**:
- `data`: Actual file content as binary sequence (IDL sequence<octet>)
- `size`: File size for validation (should match `data.length()`)
- `checksum`: CRC32 for integrity verification
- `timestamp_sec/nsec`: Modification time (preserved from source)

**Usage**:
- Published after FileEvent(CREATE/MODIFY) for files < 10MB
- Subscribed and written to local file system
- Receiver verifies checksum before accepting

**Validation Rules**:
- `filename` MUST match associated FileEvent
- `data.length()` MUST equal `size`
- Computed CRC32 of `data` MUST equal `checksum`
- `size` MUST be less than 10MB threshold (configurable)

**Performance Considerations**:
- Max recommended size: 10MB (configurable via constant)
- DDS transport max_message_size must accommodate (set to 16MB in config)
- Memory usage: Single allocation for entire file content

---

### FileChunk

**Purpose**: Transfer large files (>=10MB) by splitting into manageable chunks.

**Key Fields**:
- `filename` (key): Identifies the file being transferred
- `chunk_id` (key): Identifies which chunk of the file (0-based sequence)

**Attributes**:
- `data`: Chunk content (max 1MB per chunk)
- `total_chunks`: Total number of chunks for this file
- `file_size`: Complete file size (for reassembly validation)
- `file_checksum`: CRC32 of complete file (verify after reassembly)
- `chunk_checksum`: CRC32 of this chunk only (verify individual chunk)
- `timestamp_sec/nsec`: File modification time

**Usage**:
- Published for files >= 10MB threshold
- All chunks published in sequence (chunk_id: 0, 1, 2, ..., total_chunks-1)
- Receiver collects chunks and reassembles into complete file
- Receiver verifies chunk_checksum for each chunk, then file_checksum for complete file

**Validation Rules**:
- `filename` MUST match associated FileEvent
- `chunk_id` MUST be in range [0, total_chunks-1]
- `data.length()` MUST be <= 1MB (except last chunk may be smaller)
- Last chunk: `chunk_id == total_chunks - 1`
- Sum of all chunk `data.length()` MUST equal `file_size`
- Computed CRC32 of reassembled file MUST equal `file_checksum`

**Reassembly Logic**:
```
1. Receive FileChunk with chunk_id=0 → Initialize reassembly buffer
2. Receive subsequent chunks → Append data to buffer in order
3. Verify chunk_checksum for each received chunk
4. When all chunks received (chunk_id == total_chunks-1) → Verify file_checksum
5. If valid → Write buffer to file, preserve timestamp
6. If invalid → Log error, discard buffer, request retransmission (optional future enhancement)
```

**Performance Considerations**:
- Chunk size: 1MB (1024 * 1024 bytes)
- Chunking threshold: 10MB
- Max file size: 1GB (1000 chunks)
- Memory usage: Receiver buffers chunks until reassembly complete

---

### DirectorySnapshot

**Purpose**: Facilitate initial synchronization when a new participant joins the session.

**Key Fields**:
- `participant_id` (key): Identifies which participant is sending the snapshot

**Attributes**:
- `files`: List of all files in the participant's shared directory (FileMetadata sequence)
- `snapshot_time_sec/nsec`: When the snapshot was taken
- `file_count`: Number of files (redundant with `files.length()`, for validation)

**Usage**:
- Published by each participant when joining the session or periodically
- Subscribed by all participants to discover missing files
- Triggers FileContent/FileChunk requests for files not present locally

**Validation Rules**:
- `participant_id` MUST be unique per participant (e.g., hostname + PID)
- `files.length()` MUST equal `file_count`
- Each `FileMetadata` in `files` MUST have valid `filename`, `size`, `checksum`

**Synchronization Flow**:
```
1. New participant joins → Publish DirectorySnapshot with local files
2. Existing participants receive snapshot → Compare with local directory
3. For each file in snapshot not present locally → Subscribe to FileContent/FileChunk
4. For each file present locally but not in snapshot → Publish FileEvent(CREATE) + FileContent/FileChunk
5. For each file present in both → Compare timestamps/checksums → Update if remote is newer
```

**Performance Considerations**:
- Snapshot sent once on startup and optionally every N minutes
- Large directories (>1000 files) may result in large snapshot messages
- Alternative: Send file list in batches if snapshot exceeds message size limits

---

## Relationships Between Types

```
FileEvent
  │
  ├─→ (CREATE/MODIFY) → FileContent (if file < 10MB)
  │                      └─→ Write local file
  │
  ├─→ (CREATE/MODIFY) → FileChunk[] (if file >= 10MB)
  │                      └─→ Reassemble → Write local file
  │
  └─→ (DELETE) → Delete local file

DirectorySnapshot
  └─→ FileMetadata[] → Compare with local files
                       └─→ Trigger FileEvent + FileContent/FileChunk for missing files
```

---

## DDS Topic Mapping

| IDL Type | DDS Topic Name | Purpose | QoS Profile |
|----------|----------------|---------|-------------|
| FileMetadata | N/A (embedded) | Not published directly | N/A |
| FileEvent | "DirShare_FileEvents" | Signal file operations | RELIABLE, TRANSIENT_LOCAL |
| FileContent | "DirShare_FileContent" | Transfer small files | RELIABLE, VOLATILE |
| FileChunk | "DirShare_FileChunks" | Transfer large files | RELIABLE, VOLATILE |
| DirectorySnapshot | "DirShare_DirectorySnapshot" | Initial sync | RELIABLE, TRANSIENT_LOCAL |

See `contracts/topics.md` for detailed QoS policies and topic configuration.

---

## Type Generation

OpenDDS IDL compiler (`opendds_idl`) generates the following from `DirShare.idl`:

**Generated Files**:
- `DirShareTypeSupport.idl` - DDS-standard TypeSupport interfaces
- `DirShareTypeSupportImpl.h/cpp` - OpenDDS TypeSupport implementation
- `DirShareC.h/cpp` - CORBA client stubs (data type definitions)
- `DirShareS.h/cpp` - CORBA server skeletons (not used for data types)

**Usage in Application**:
```cpp
#include "DirShareTypeSupportImpl.h"

// Register types
DirShare::FileEventTypeSupport_var ts_event = new DirShare::FileEventTypeSupportImpl;
ts_event->register_type(participant, "");

DirShare::FileContentTypeSupport_var ts_content = new DirShare::FileContentTypeSupportImpl;
ts_content->register_type(participant, "");

// ... similar for FileChunk and DirectorySnapshot
```

---

## Constraints and Limits

| Constraint | Value | Rationale |
|------------|-------|-----------|
| Max file size | 1GB | Spec requirement, practical limit for example |
| Chunking threshold | 10MB | Balance between simplicity and scalability |
| Chunk size | 1MB | Network efficiency, memory usage |
| Max filename length | 255 characters | Filesystem compatibility |
| Max path depth | Single level | Spec: no recursive subdirectories |
| Timestamp precision | Nanoseconds | DDS Time_t standard (seconds + nanoseconds) |
| Checksum algorithm | CRC32 (32-bit) | Fast, sufficient for error detection |

---

## Version History

- **v1.0** (2025-10-30): Initial data model definition
  - FileMetadata, FileEvent, FileContent, FileChunk, DirectorySnapshot
  - CRC32 checksums for integrity
  - Timestamp-based conflict resolution
  - 10MB threshold, 1MB chunks, 1GB max file size
