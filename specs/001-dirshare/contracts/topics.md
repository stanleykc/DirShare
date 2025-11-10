# DDS Topic Contracts: DirShare

**Feature**: DirShare - Distributed File Synchronization
**Date**: 2025-10-30
**Purpose**: Define DDS topics, QoS policies, and communication contracts

## Overview

DirShare uses 4 DDS topics for file synchronization communication:
1. **DirShare_FileEvents** - File operation notifications (CREATE/MODIFY/DELETE)
2. **DirShare_FileContent** - Small file content transfer (<10MB)
3. **DirShare_FileChunks** - Large file chunked transfer (>=10MB)
4. **DirShare_DirectorySnapshot** - Initial directory state synchronization

All topics use **domain ID 42** (configurable via command-line).

---

## Topic 1: DirShare_FileEvents

**Data Type**: `DirShare::FileEvent`
**Purpose**: Notify all participants about file operations in shared directory

### QoS Policies

| Policy | Value | Rationale |
|--------|-------|-----------|
| **Reliability** | RELIABLE_RELIABILITY_QOS | File operations must not be missed; ensures all participants see all events |
| **Durability** | TRANSIENT_LOCAL_DURABILITY_QOS | Late-joining participants receive recent events to catch up |
| **History** | KEEP_LAST_HISTORY_QOS (depth=100) | Keep last 100 events for late joiners; bounded memory usage |
| **Lifespan** | 300 seconds (5 minutes) | Events older than 5 minutes are considered stale |
| **Ownership** | SHARED_OWNERSHIP_QOS | Multiple participants can publish events for different files |

### Key Structure

- **Key Field**: `filename`
- **Keying Strategy**: One instance per file path; allows per-file ordering and conflict resolution
- **Instance Lifecycle**: Instance created on first event for a file, disposed on DELETE event

### Usage Pattern

**Publisher**:
```cpp
DirShare::FileEventDataWriter_var writer = /* ... */;
DirShare::FileEvent event;
event.filename = "example.txt";
event.operation = DirShare::MODIFY;
event.timestamp_sec = /* current time */;
event.timestamp_nsec = /* nanoseconds */;
event.metadata = /* file metadata */;

DDS::ReturnCode_t ret = writer->write(event, DDS::HANDLE_NIL);
if (ret != DDS::RETCODE_OK) {
  ACE_ERROR((LM_ERROR, "Failed to write FileEvent\n"));
}
```

**Subscriber**:
```cpp
// In DataReaderListener::on_data_available()
DirShare::FileEvent event;
DDS::SampleInfo info;

DDS::ReturnCode_t ret = reader->take_next_sample(event, info);
if (ret == DDS::RETCODE_OK && info.valid_data) {
  // Check source timestamp for conflict resolution
  if (compareTimestamps(info.source_timestamp, getLocalModTime(event.filename)) > 0) {
    // IMPORTANT: Suppress notifications before applying remote change
    // to prevent notification loops (FR-017, SC-011)
    change_tracker_.suppress_notifications(event.filename);
    applyFileOperation(event);
    change_tracker_.resume_notifications(event.filename);
  }
}
```

---

## Topic 2: DirShare_FileContent

**Data Type**: `DirShare::FileContent`
**Purpose**: Transfer small file content (<10MB) as single message

### QoS Policies

| Policy | Value | Rationale |
|--------|-------|-----------|
| **Reliability** | RELIABLE_RELIABILITY_QOS | File content must arrive intact; retransmit on loss |
| **Durability** | VOLATILE_DURABILITY_QOS | Content only relevant during active transfer; no persistence needed |
| **History** | KEEP_LAST_HISTORY_QOS (depth=1) | Only latest version of file matters; conserves memory |
| **Deadline** | 30 seconds | File transfer should complete within 30 seconds or fail |
| **DestinationOrder** | BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS | Ensure latest version wins on conflicts |
| **Ownership** | EXCLUSIVE_OWNERSHIP_QOS | Only one participant publishes each file |

### Key Structure

- **Key Field**: `filename`
- **Keying Strategy**: One instance per file; ensures only latest content is kept
- **Instance Lifecycle**: Instance created when file is shared, updated on modifications

### Transport Considerations

- **Max Message Size**: Must be < 16MB (configured in transport INI)
- **Recommended File Size**: < 10MB (threshold enforced by application)
- **Memory Usage**: Entire file loaded into memory on both sender and receiver

### Usage Pattern

**Publisher**:
```cpp
DirShare::FileContentDataWriter_var writer = /* ... */;
DirShare::FileContent content;
content.filename = "small_file.txt";

// Read entire file into content.data
std::ifstream file("small_file.txt", std::ios::binary);
file.seekg(0, std::ios::end);
size_t size = file.tellg();
file.seekg(0, std::ios::beg);

content.data.length(size);
file.read(reinterpret_cast<char*>(content.data.get_buffer()), size);

content.size = size;
content.checksum = calculate_crc32(content.data.get_buffer(), size);
content.timestamp_sec = /* file mod time */;
content.timestamp_nsec = /* nanoseconds */;

writer->write(content, DDS::HANDLE_NIL);
```

**Subscriber**:
```cpp
// In DataReaderListener::on_data_available()
DirShare::FileContent content;
DDS::SampleInfo info;

if (reader->take_next_sample(content, info) == DDS::RETCODE_OK && info.valid_data) {
  // Verify checksum
  unsigned long calculated_crc = calculate_crc32(content.data.get_buffer(), content.size);
  if (calculated_crc == content.checksum) {
    // Write to file
    std::ofstream outfile(content.filename.in(), std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(content.data.get_buffer()), content.size);

    // Preserve timestamp
    setFileModTime(content.filename.in(), content.timestamp_sec, content.timestamp_nsec);
  } else {
    ACE_ERROR((LM_ERROR, "Checksum mismatch for %C\n", content.filename.in()));
  }
}
```

---

## Topic 3: DirShare_FileChunks

**Data Type**: `DirShare::FileChunk`
**Purpose**: Transfer large files (>=10MB) in 1MB chunks

### QoS Policies

| Policy | Value | Rationale |
|--------|-------|-----------|
| **Reliability** | RELIABLE_RELIABILITY_QOS | Every chunk must arrive; missing chunk prevents reassembly |
| **Durability** | VOLATILE_DURABILITY_QOS | Chunks only relevant during transfer; receiver buffers until complete |
| **History** | KEEP_ALL_HISTORY_QOS | All chunks must be retained until reassembly complete |
| **ResourceLimits** | max_samples=1000, max_instances=100, max_samples_per_instance=1000 | Support up to 100 concurrent large files, 1000 chunks each (1GB files) |
| **Deadline** | 60 seconds per chunk | Each chunk should arrive within 60 seconds |
| **Ownership** | EXCLUSIVE_OWNERSHIP_QOS | Only one participant publishes each file |

### Key Structure

- **Key Fields**: `filename`, `chunk_id`
- **Keying Strategy**: Composite key creates one instance per chunk; allows out-of-order delivery with reassembly
- **Instance Lifecycle**: All chunk instances for a file created during transfer, disposed after reassembly

### Chunking Strategy

- **Chunk Size**: 1MB (1024 * 1024 bytes)
- **Threshold**: Files >= 10MB are chunked
- **Max Chunks**: 1000 (1GB max file size)
- **Ordering**: Chunks can arrive out-of-order; receiver reassembles by chunk_id

### Usage Pattern

**Publisher**:
```cpp
DirShare::FileChunkDataWriter_var writer = /* ... */;

// Calculate chunking parameters
const size_t CHUNK_SIZE = 1024 * 1024; // 1MB
size_t file_size = getFileSize("large_file.bin");
unsigned long total_chunks = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
unsigned long file_checksum = calculate_file_crc32("large_file.bin");

std::ifstream file("large_file.bin", std::ios::binary);
std::vector<char> buffer(CHUNK_SIZE);

for (unsigned long chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
  DirShare::FileChunk chunk;
  chunk.filename = "large_file.bin";
  chunk.chunk_id = chunk_id;
  chunk.total_chunks = total_chunks;
  chunk.file_size = file_size;
  chunk.file_checksum = file_checksum;

  // Read chunk data
  file.read(buffer.data(), CHUNK_SIZE);
  size_t bytes_read = file.gcount();

  chunk.data.length(bytes_read);
  memcpy(chunk.data.get_buffer(), buffer.data(), bytes_read);

  chunk.chunk_checksum = calculate_crc32(buffer.data(), bytes_read);
  chunk.timestamp_sec = /* file mod time */;
  chunk.timestamp_nsec = /* nanoseconds */;

  writer->write(chunk, DDS::HANDLE_NIL);
}
```

**Subscriber**:
```cpp
// In DataReaderListener::on_data_available()
// Maintain map of in-progress reassemblies: std::map<std::string, FileReassembly>

DirShare::FileChunk chunk;
DDS::SampleInfo info;

if (reader->take_next_sample(chunk, info) == DDS::RETCODE_OK && info.valid_data) {
  // Verify chunk checksum
  unsigned long calculated_crc = calculate_crc32(chunk.data.get_buffer(), chunk.data.length());
  if (calculated_crc != chunk.chunk_checksum) {
    ACE_ERROR((LM_ERROR, "Chunk checksum mismatch for %C chunk %d\n",
               chunk.filename.in(), chunk.chunk_id));
    return;
  }

  // Add chunk to reassembly buffer
  FileReassembly& reassembly = reassembly_map[chunk.filename.in()];
  reassembly.chunks[chunk.chunk_id] = chunk;

  // Check if all chunks received
  if (reassembly.chunks.size() == chunk.total_chunks) {
    // Reassemble file
    std::ofstream outfile(chunk.filename.in(), std::ios::binary);
    for (unsigned long i = 0; i < chunk.total_chunks; ++i) {
      const DirShare::FileChunk& c = reassembly.chunks[i];
      outfile.write(reinterpret_cast<const char*>(c.data.get_buffer()), c.data.length());
    }
    outfile.close();

    // Verify file checksum
    unsigned long file_crc = calculate_file_crc32(chunk.filename.in());
    if (file_crc == chunk.file_checksum) {
      // Preserve timestamp
      setFileModTime(chunk.filename.in(), chunk.timestamp_sec, chunk.timestamp_nsec);
      ACE_DEBUG((LM_INFO, "File %C reassembled successfully\n", chunk.filename.in()));
    } else {
      ACE_ERROR((LM_ERROR, "File checksum mismatch for %C\n", chunk.filename.in()));
      unlink(chunk.filename.in()); // Delete corrupted file
    }

    // Clean up reassembly buffer
    reassembly_map.erase(chunk.filename.in());
  }
}
```

---

## Topic 4: DirShare_DirectorySnapshot

**Data Type**: `DirShare::DirectorySnapshot`
**Purpose**: Enable initial synchronization when participant joins session

### QoS Policies

| Policy | Value | Rationale |
|--------|-------|-----------|
| **Reliability** | RELIABLE_RELIABILITY_QOS | New participants must receive snapshot to synchronize |
| **Durability** | TRANSIENT_LOCAL_DURABILITY_QOS | Late joiners receive last snapshot from each participant |
| **History** | KEEP_LAST_HISTORY_QOS (depth=1) | Only latest snapshot per participant matters |
| **Lifespan** | 600 seconds (10 minutes) | Snapshots older than 10 minutes are stale |
| **Ownership** | SHARED_OWNERSHIP_QOS | Each participant publishes own snapshot |

### Key Structure

- **Key Field**: `participant_id`
- **Keying Strategy**: One instance per participant; allows each to publish directory state
- **Instance Lifecycle**: Instance created when participant joins, updated periodically

### Usage Pattern

**Publisher** (on startup):
```cpp
DirShare::DirectorySnapshotDataWriter_var writer = /* ... */;
DirShare::DirectorySnapshot snapshot;

snapshot.participant_id = generate_participant_id(); // e.g., hostname + PID

// Scan local directory
std::vector<DirShare::FileMetadata> local_files = scan_directory(shared_dir);
snapshot.files.length(local_files.size());
for (size_t i = 0; i < local_files.size(); ++i) {
  snapshot.files[i] = local_files[i];
}

snapshot.file_count = local_files.size();
snapshot.snapshot_time_sec = /* current time */;
snapshot.snapshot_time_nsec = /* nanoseconds */;

writer->write(snapshot, DDS::HANDLE_NIL);
```

**Subscriber** (on receiving snapshot):
```cpp
// In DataReaderListener::on_data_available()
DirShare::DirectorySnapshot snapshot;
DDS::SampleInfo info;

if (reader->take_next_sample(snapshot, info) == DDS::RETCODE_OK && info.valid_data) {
  // Ignore own snapshot
  if (snapshot.participant_id == my_participant_id) {
    return;
  }

  // Compare with local directory
  for (unsigned long i = 0; i < snapshot.files.length(); ++i) {
    const DirShare::FileMetadata& remote_file = snapshot.files[i];

    if (!fileExistsLocally(remote_file.filename)) {
      // File missing locally, request transfer
      request_file_transfer(remote_file.filename);
    } else {
      // File exists, check if remote is newer
      FileMetadata local_file = getLocalFileMetadata(remote_file.filename);
      if (remote_file.timestamp_sec > local_file.timestamp_sec ||
          (remote_file.timestamp_sec == local_file.timestamp_sec &&
           remote_file.timestamp_nsec > local_file.timestamp_nsec)) {
        // Remote version is newer
        request_file_transfer(remote_file.filename);
      } else if (remote_file.checksum != local_file.checksum) {
        // Same timestamp but different content (rare), use checksum tie-breaker
        request_file_transfer(remote_file.filename);
      }
    }
  }
}
```

---

## Domain and Partition Configuration

### Domain ID

- **Default Domain**: 42
- **Configurable**: Via command-line argument `-d <domain_id>`
- **Isolation**: Each domain creates isolated DDS communication space

### Partitions

- **Default**: No partition (empty string)
- **Optional**: Support partition-based isolation via command-line `-p <partition_name>`
- **Use Case**: Multiple independent DirShare sessions on same domain

```cpp
// Setting partition in QoS
DDS::PublisherQos pub_qos;
participant->get_default_publisher_qos(pub_qos);
pub_qos.partition.name.length(1);
pub_qos.partition.name[0] = "my_partition";
DDS::Publisher_var publisher = participant->create_publisher(pub_qos, 0, 0);
```

---

## Discovery Configuration

### InfoRepo Mode (Default)

**Configuration**:
- DCPSInfoRepo IOR specified via `-DCPSInfoRepo file://repo.ior` command-line arg
- Test script (`run_test.pl`) starts DCPSInfoRepo automatically

**Advantages**:
- Centralized discovery, simple debugging
- Fast discovery for small deployments

**Transport**: TCP

### RTPS Mode

**Configuration File** (`rtps.ini`):
```ini
[common]
DCPSGlobalTransportConfig=rtps_config
DCPSDefaultDiscovery=DEFAULT_RTPS

[domain/42]
DiscoveryConfig=DEFAULT_RTPS

[rtps_discovery/DEFAULT_RTPS]
ResendPeriod=2
SedpMulticast=1

[config/rtps_config]
transports=rtps_udp

[transport/rtps_udp]
transport_type=rtps_udp
local_address=0.0.0.0:0
```

**Usage**: `./dirshare -DCPSConfigFile rtps.ini <shared_dir>`

**Advantages**:
- Peer-to-peer discovery, no central server
- DDS-standard interoperability
- Scales better for larger deployments

**Transport**: RTPS/UDP

---

## Transport Configuration

### TCP Transport (InfoRepo Mode)

```ini
[config/tcp_config]
transports=tcp
max_message_size=16777216  # 16MB

[transport/tcp]
transport_type=tcp
max_packet_size=8192       # 8KB packets
enable_nagle_algorithm=0   # Disable for lower latency
```

### RTPS/UDP Transport (RTPS Mode)

```ini
[config/rtps_config]
transports=rtps_udp
max_message_size=16777216  # 16MB

[transport/rtps_udp]
transport_type=rtps_udp
local_address=0.0.0.0:0    # Bind to any available port
send_buffer_size=2097152   # 2MB send buffer
rcv_buffer_size=2097152    # 2MB receive buffer
```

---

## Performance and Scalability

### Message Size Limits

| Data Type | Typical Size | Max Size | Notes |
|-----------|--------------|----------|-------|
| FileEvent | <1 KB | 10 KB | Metadata only |
| FileContent | Variable | 10 MB | Threshold for chunking |
| FileChunk | 1 MB | 1 MB | Fixed chunk size |
| DirectorySnapshot | Variable | 10 MB | Depends on file count |

### Resource Limits

**Publishers**:
- Max concurrent file transfers: 100 (limited by available instances)
- Max chunk buffer: 1GB (1000 chunks per file)

**Subscribers**:
- Max reassembly buffers: 100 concurrent files
- Memory per large file: ~file_size (buffered until reassembly complete)

### Scalability Targets

- **Participants**: 10+ simultaneous participants
- **Files**: 1000+ files in shared directory
- **File Size**: Up to 1GB per file
- **Transfer Rate**: 5 seconds for 10MB files on LAN

---

## Error Handling and Reliability

### Checksums

- **FileContent/FileChunk**: CRC32 verified on receipt
- **Mismatch Action**: Log error, discard data, optionally request retransmission

### DDS Reliability

- **RELIABLE_RELIABILITY_QOS**: Automatic retransmission on packet loss
- **Heartbeats**: Publisher sends periodic heartbeats, subscriber requests missing samples
- **Acknowledgments**: Subscriber acknowledges received samples

### Timeouts

- **FileEvent**: 5-minute lifespan (stale events discarded)
- **FileContent**: 30-second deadline (transfer must complete)
- **FileChunk**: 60-second per-chunk deadline
- **DirectorySnapshot**: 10-minute lifespan

### Conflict Resolution

- **Timestamp Comparison**: DDS source_timestamp used for ordering
- **Last-Write-Wins**: Newer timestamp always wins
- **Tie-breaker**: If timestamps equal, use checksum or participant GUID

---

## Testing Contracts

### Test Scenarios

1. **Basic Sync**: 2 participants, create file on A → appears on B within 5 seconds
2. **Modify Sync**: 2 participants, modify file on A → propagates to B within 5 seconds
3. **Delete Sync**: 2 participants, delete file on A → deleted on B within 5 seconds
4. **Large File**: Transfer 100MB file → reassembles correctly, checksum validates
5. **Conflict**: Modify same file on A and B simultaneously → last timestamp wins
6. **Late Joiner**: Start A and B, create files, then start C → C receives snapshot and synchronizes
7. **RTPS Mode**: All above scenarios with `--rtps` flag

### Success Criteria

- All DDS return codes == RETCODE_OK
- All checksums verify successfully
- All timestamps preserved correctly
- No memory leaks (valgrind clean)
- Both InfoRepo and RTPS modes pass

---

## Version History

- **v1.0** (2025-10-30): Initial topic contract definition
  - 4 topics: FileEvents, FileContent, FileChunks, DirectorySnapshot
  - QoS policies for reliability, durability, history
  - Support for both InfoRepo and RTPS discovery
  - CRC32 checksums for integrity
