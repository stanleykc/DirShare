# Quickstart Guide: DirShare

**Feature**: DirShare - Distributed File Synchronization
**Purpose**: Get DirShare built and running quickly

## Prerequisites

### Environment Setup

1. **OpenDDS Configured**: Run `./configure` from OpenDDS repository root
2. **Environment Sourced**: Run `source setenv.sh` (or `setenv.cmd` on Windows)
3. **ACE/TAO Available**: Automatically configured by OpenDDS configure script
4. **Perl Installed**: Required for test scripts

```bash
# From OpenDDS repository root
./configure
source setenv.sh
make  # Build OpenDDS core (first time only)
```

### Verify Environment

```bash
echo $DDS_ROOT      # Should point to OpenDDS directory
echo $ACE_ROOT      # Should point to ACE_wrappers directory
which opendds_idl   # Should find OpenDDS IDL compiler
```

---

## Build Instructions

### Option 1: MPC Build (Primary)

```bash
cd DevGuideExamples/DCPS/DirShare

# Generate makefiles (first time or after .mpc changes)
mwc.pl -type gnuace

# Build all targets
make

# Verify build
ls -lh dirshare     # Should see executable
```

**Build Targets**:
- `dirshare` - Main executable (acts as both publisher and subscriber)

### Option 2: CMake Build (Alternative)

```bash
cd DevGuideExamples/DCPS/DirShare
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Verify build
ls -lh dirshare     # Should see executable
```

---

## Quick Test

### Test 1: Basic File Synchronization (InfoRepo Mode)

Run automated test with default InfoRepo discovery:

```bash
cd DevGuideExamples/DCPS/DirShare
perl run_test.pl
```

**Expected Output**:
```
Test started...
Starting DCPSInfoRepo...
Starting subscriber in dir_sub...
Starting publisher in dir_pub...
Files synchronized successfully
Test PASSED
```

### Test 2: RTPS Mode

Run automated test with RTPS discovery:

```bash
perl run_test.pl --rtps
```

**Expected Output**:
```
Test started (RTPS mode)...
Starting subscriber in dir_sub...
Starting publisher in dir_pub...
Files synchronized successfully
Test PASSED
```

---

## Manual Usage

### Scenario 1: Two-Participant File Sync

**Terminal 1 (Participant A)**:
```bash
cd DevGuideExamples/DCPS/DirShare
mkdir /tmp/dirshare_a
echo "Hello from A" > /tmp/dirshare_a/fileA.txt

# Start with InfoRepo
./dirshare -DCPSInfoRepo file://repo.ior /tmp/dirshare_a
```

**Terminal 2 (DCPSInfoRepo)**:
```bash
$DDS_ROOT/bin/DCPSInfoRepo -o repo.ior
```

**Terminal 3 (Participant B)**:
```bash
cd DevGuideExamples/DCPS/DirShare
mkdir /tmp/dirshare_b
echo "Hello from B" > /tmp/dirshare_b/fileB.txt

# Start with InfoRepo
./dirshare -DCPSInfoRepo file://repo.ior /tmp/dirshare_b
```

**Expected Result**:
- `/tmp/dirshare_a` contains both `fileA.txt` and `fileB.txt`
- `/tmp/dirshare_b` contains both `fileA.txt` and `fileB.txt`
- Files synchronized within 5 seconds

### Scenario 2: RTPS Mode (No Central Server)

**Terminal 1 (Participant A)**:
```bash
mkdir /tmp/dirshare_a
echo "Hello from A" > /tmp/dirshare_a/fileA.txt
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
```

**Terminal 2 (Participant B)**:
```bash
mkdir /tmp/dirshare_b
echo "Hello from B" > /tmp/dirshare_b/fileB.txt
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_b
```

**Expected Result**:
- Same as Scenario 1, but without DCPSInfoRepo
- Participants discover each other via RTPS multicast

---

## Command-Line Options

### DirShare Executable

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

### Test Script Options

```
Usage: perl run_test.pl [OPTIONS]

Options:
  --rtps         Use RTPS discovery (default: InfoRepo)
  --debug <n>    Set DCPS debug level (0-10)
  --transport <n> Set DCPS transport debug level (0-6)

Examples:
  perl run_test.pl                  # Default InfoRepo test
  perl run_test.pl --rtps           # RTPS test
  perl run_test.pl --debug 4        # InfoRepo with debug output
```

---

## Testing Real-Time Sync

### Test File Creation

```bash
# With DirShare running in /tmp/dirshare_a and /tmp/dirshare_b
echo "New file" > /tmp/dirshare_a/newfile.txt

# Check propagation (should appear within 5 seconds)
ls -l /tmp/dirshare_b/newfile.txt
cat /tmp/dirshare_b/newfile.txt  # Should contain "New file"
```

### Test File Modification

```bash
# Modify existing file
echo "Updated content" >> /tmp/dirshare_a/fileA.txt

# Verify propagation
tail /tmp/dirshare_b/fileA.txt   # Should show updated content
```

### Test File Deletion

```bash
# Delete file
rm /tmp/dirshare_a/fileA.txt

# Verify propagation (should disappear within 5 seconds)
ls /tmp/dirshare_b/fileA.txt     # Should not exist
```

### Test Large File Transfer

```bash
# Create 50MB test file
dd if=/dev/urandom of=/tmp/dirshare_a/large.bin bs=1M count=50

# Monitor transfer (should complete within 30 seconds)
watch ls -lh /tmp/dirshare_b/large.bin
```

---

## Troubleshooting

### Issue: Executable Not Found

**Symptom**: `bash: dirshare: command not found`

**Solution**:
```bash
# Verify build completed
ls -l dirshare

# If not built, run make
make

# Check for build errors in output
```

### Issue: DDS Errors (InfoRepo Mode)

**Symptom**: `ERROR: unable to connect to DCPSInfoRepo`

**Solution**:
```bash
# Ensure DCPSInfoRepo is running
ps aux | grep DCPSInfoRepo

# If not running, start it
$DDS_ROOT/bin/DCPSInfoRepo -o repo.ior &

# Verify IOR file created
ls -l repo.ior
```

### Issue: Files Not Synchronizing

**Symptom**: Files created in one directory don't appear in the other

**Debug Steps**:
```bash
# 1. Enable debug logging
export DCPS_debug_level=4
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a

# 2. Check DDS discovery
# Look for "publication matched" and "subscription matched" messages in output

# 3. Verify directory permissions
ls -ld /tmp/dirshare_a /tmp/dirshare_b  # Should be writable

# 4. Check for error messages
grep ERROR dirshare_output.log
```

### Issue: Checksum Errors

**Symptom**: `ERROR: Checksum mismatch for <filename>`

**Causes**:
- File modified during transfer
- Network corruption (rare with reliable QoS)
- Bug in CRC32 calculation

**Solution**:
```bash
# 1. Verify file is not being modified during transfer
lsof /tmp/dirshare_a/problematic_file.txt

# 2. Check network quality (packet loss)
ping <remote_host>

# 3. Retry transfer (delete and recreate file)
rm /tmp/dirshare_a/problematic_file.txt
cp /backup/problematic_file.txt /tmp/dirshare_a/
```

### Issue: High Memory Usage

**Symptom**: DirShare process using excessive memory

**Causes**:
- Many large files being transferred simultaneously
- Incomplete chunk reassembly buffers

**Solution**:
```bash
# 1. Check memory usage
ps aux | grep dirshare

# 2. Limit concurrent transfers (future enhancement)
# For now, transfer large files sequentially

# 3. Restart DirShare if memory leak suspected
killall dirshare
./dirshare -DCPSConfigFile rtps.ini /tmp/dirshare_a
```

---

## Performance Benchmarks

### Expected Performance (LAN, 1 Gbps)

| Operation | File Size | Expected Time | Measured |
|-----------|-----------|---------------|----------|
| Create/Propagate | 1 MB | < 1 second | [TBD] |
| Create/Propagate | 10 MB | < 5 seconds | [TBD] |
| Create/Propagate | 100 MB | < 30 seconds | [TBD] |
| Modify/Propagate | 1 MB | < 2 seconds | [TBD] |
| Delete/Propagate | Any | < 1 second | [TBD] |
| Initial Sync | 100 files (1 MB each) | < 30 seconds | [TBD] |

### Performance Tips

1. **Use RTPS for large deployments**: Peer-to-peer scales better than centralized InfoRepo
2. **Limit file size**: Keep files under 1GB; split larger files externally
3. **Avoid frequent small changes**: Polling interval is 1-2 seconds
4. **Use SSD storage**: Faster disk I/O improves throughput
5. **Tune transport buffer sizes**: Increase send/receive buffers for high throughput

---

## Next Steps

### After Quickstart

1. **Review IDL Definitions**: See `data-model.md` for detailed data structures
2. **Understand DDS Topics**: See `contracts/topics.md` for QoS policies and communication patterns
3. **Implement Feature**: See `tasks.md` (generated by `/speckit.tasks`) for step-by-step implementation guide
4. **Extend Functionality**: Consider adding features like:
   - Subdirectory support (recursive sync)
   - File filtering (include/exclude patterns)
   - Bandwidth throttling
   - Compression
   - Encryption

### Additional Resources

- **OpenDDS Developer's Guide**: `$DDS_ROOT/docs/OpenDDS_Developer_Guide.pdf`
- **Example Code**: `DevGuideExamples/DCPS/Messenger/` (similar patterns)
- **OpenDDS Website**: https://opendds.org/
- **DDS Specification**: https://www.omg.org/spec/DDS/

---

## Cleanup

### Stop Running Processes

```bash
# Stop DirShare instances
killall dirshare

# Stop DCPSInfoRepo (if running)
killall DCPSInfoRepo

# Clean up IOR files
rm -f repo.ior
```

### Remove Test Directories

```bash
rm -rf /tmp/dirshare_a /tmp/dirshare_b
```

### Clean Build Artifacts

```bash
cd DevGuideExamples/DCPS/DirShare

# MPC build
make realclean

# CMake build
rm -rf build/
```

---

## Version History

- **v1.0** (2025-10-30): Initial quickstart guide
  - Build instructions for MPC and CMake
  - Basic and RTPS test scenarios
  - Manual usage examples
  - Troubleshooting tips
