# Quickstart: Multi-Participant Robustness Testing

**Feature**: 002-robustness-testing | **Date**: 2025-11-12

## Overview

This guide walks you through running the DirShare multi-participant robustness test suite in under 5 minutes.

## Prerequisites

### 1. OpenDDS Environment

```bash
# Source OpenDDS environment (required)
source $DDS_ROOT/setenv.sh

# Verify environment
echo $DDS_ROOT
echo $ACE_ROOT
```

### 2. DirShare Application Built

```bash
# Navigate to DirShare root
cd /Users/stanleyk/dev/DirShare

# Build DirShare (if not already built)
mwc.pl -type gnuace
make

# Verify executable exists
ls -lh dirshare
```

### 3. Python 3 and Virtual Environment

```bash
# Check Python version (3.8+ required)
python3 --version

# Navigate to robot test directory
cd robot

# Create virtual environment (first time only)
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate

# Install Robot Framework dependencies
python3 -m pip install -r requirements.txt
```

## Running Tests

### Quick Run: All Robustness Tests

```bash
# From robot/ directory with venv activated
source venv/bin/activate

# Run complete robustness test suite
robot --outputdir results RobustnessTests.robot

# View HTML report
open results/report.html
```

**Expected Duration**: 10-12 minutes for all 12 scenarios

### Run Specific User Story

```bash
# Run only US1 (Participant Recovery) tests
robot --include US1 --outputdir results RobustnessTests.robot

# Run only US4 (Large File Transfer) tests
robot --include US4 --outputdir results RobustnessTests.robot
```

### Run Specific Priority Level

```bash
# Run only P1 (highest priority) tests
robot --include priority-P1 --outputdir results RobustnessTests.robot

# Run P1 and P2 tests
robot --include priority-P1 --include priority-P2 --outputdir results RobustnessTests.robot
```

### Run Single Test Case

```bash
# Run specific test by name
robot --test "US1.1: Single Participant Restart With File Creation" \
      --outputdir results RobustnessTests.robot
```

## Understanding Test Output

### Success Output

```
==============================================================================
RobustnessTests :: Multi-Participant Robustness Test Suite
==============================================================================
US1.1: Single Participant Restart With File Creation          | PASS |
Synchronization achieved in 7.2 seconds
------------------------------------------------------------------------------
US1.2: Mixed File Operations During Downtime                  | PASS |
Synchronization achieved in 8.5 seconds
------------------------------------------------------------------------------
RobustnessTests                                               | PASS |
12 tests, 12 passed, 0 failed
==============================================================================
```

### Timing Analysis

Each test logs actual synchronization time:
```
Synchronization achieved in 7.2 seconds (timeout: 10s)
```

If timing approaches timeout, investigate:
- System load (other processes consuming CPU)
- Network latency (if running on VMs)
- Disk I/O performance

### Failure Output

```
US2.2: Conflicting Modify and Delete Operations               | FAIL |
Timeout 10 seconds exceeded. Directories not synchronized:
  Participant A: test.txt present (checksum: 0x12345678)
  Participant B: test.txt absent
  Participant C: test.txt absent
Expected: test.txt absent on all participants (DELETE should win)
```

## Test Validation Checklist

After running the complete suite:

- [ ] **Pass Rate**: At least 95% of tests pass (11+ out of 12)
- [ ] **Execution Time**: Complete suite finishes under 15 minutes
- [ ] **Timing Margins**: No tests consistently near timeout limits
- [ ] **Clean Logs**: No ERROR messages in participant logs (only INFO/DEBUG)

## Troubleshooting

### Test Fails: "Participant did not start"

**Cause**: DirShare executable not found or OpenDDS environment not sourced

**Fix**:
```bash
# Re-source OpenDDS environment
source $DDS_ROOT/setenv.sh

# Verify dirshare executable exists and is executable
ls -l dirshare
chmod +x dirshare  # if needed
```

### Test Fails: "Timeout exceeded"

**Cause**: Synchronization took longer than expected (10/30/60s)

**Fix**:
1. Check system load: `top` or `htop`
2. Review participant logs in `results/` directory
3. Enable DDS debug logging:
   ```bash
   export DCPS_debug_level=4
   robot --outputdir results RobustnessTests.robot
   ```
4. If consistently failing, may indicate real synchronization bug

### Test Fails: "Checksum mismatch"

**Cause**: Files have different content across participants

**Fix**:
1. This indicates a real synchronization bug
2. Check participant logs for file transfer errors
3. Verify test file generation is deterministic (same seed = same content)
4. Manual verification:
   ```bash
   # Compare files directly
   diff /tmp/dirshare_test_A/file.txt /tmp/dirshare_test_B/file.txt

   # Check checksums
   crc32 /tmp/dirshare_test_*/file.txt
   ```

### Test Fails: "Process crashed"

**Cause**: DirShare participant terminated unexpectedly

**Fix**:
1. Check for core dumps: `ls -l core*`
2. Review stderr logs in `results/` directory
3. Run participant manually to reproduce:
   ```bash
   ./dirshare -DCPSConfigFile rtps.ini /tmp/test_dir
   ```
4. Check system resources (memory, disk space)

### Python Import Errors

**Cause**: Virtual environment not activated or dependencies not installed

**Fix**:
```bash
cd robot
source venv/bin/activate
python3 -m pip install -r requirements.txt
```

## Performance Validation

### Run 10 Consecutive Executions

To validate 95% pass rate requirement (SC-004):

```bash
# Script to run 10 times and collect statistics
for i in {1..10}; do
    echo "=== Run $i/10 ==="
    robot --outputdir results/run_$i RobustnessTests.robot
done

# Count passes
grep -h "12 tests, 12 passed" results/run_*/output.xml | wc -l
```

**Expected**: At least 9 out of 10 runs pass all tests (90% minimum, targeting 95%)

### Timing Statistics

```bash
# Extract synchronization times from logs
grep "Synchronization achieved in" results/log.html | \
  sed 's/.*achieved in \([0-9.]*\) seconds.*/\1/' | \
  awk '{sum+=$1; count++} END {print "Average:", sum/count, "seconds"}'
```

## Advanced Usage

### Enable Verbose Logging

```bash
# Robot Framework verbose mode + DDS debug logging
export DCPS_debug_level=4
export DCPS_transport_debug_level=2
robot --loglevel DEBUG --outputdir results RobustnessTests.robot
```

### Preserve Test Directories on Failure

By default, test directories are cleaned up. To preserve for debugging:

```bash
# Set environment variable
export DIRSHARE_TEST_KEEP_DIRS=1
robot --outputdir results RobustnessTests.robot

# Test directories remain in /tmp/dirshare_robustness_test_*
ls -l /tmp/dirshare_robustness_test_*
```

### Run with Different RTPS Configuration

```bash
# Create custom rtps_debug.ini with debug settings
cp rtps.ini rtps_debug.ini
echo "[common]" >> rtps_debug.ini
echo "DCPSDebugLevel=4" >> rtps_debug.ini

# Run tests with custom config
export DIRSHARE_RTPS_CONFIG=rtps_debug.ini
robot --outputdir results RobustnessTests.robot
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Robustness Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup OpenDDS
        run: |
          source $DDS_ROOT/setenv.sh

      - name: Build DirShare
        run: |
          mwc.pl -type gnuace
          make

      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'

      - name: Install Robot Framework
        run: |
          cd robot
          python3 -m pip install -r requirements.txt

      - name: Run Robustness Tests
        run: |
          cd robot
          robot --outputdir results RobustnessTests.robot

      - name: Upload Test Results
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: robot-results
          path: robot/results/
```

## Next Steps

After running tests successfully:

1. **Review Test Report**: Open `results/report.html` for detailed test execution summary
2. **Analyze Timing**: Identify tests near timeout limits for potential optimization
3. **Read Logs**: Review `results/log.html` for detailed participant interactions
4. **Run Repeatedly**: Execute 10 times to validate 95% pass rate requirement
5. **Integrate CI/CD**: Add tests to continuous integration pipeline

## Reference Documentation

- **Feature Specification**: `specs/002-robustness-testing/spec.md`
- **Test Scenarios**: `specs/002-robustness-testing/contracts/test-scenarios.md`
- **Data Model**: `specs/002-robustness-testing/data-model.md`
- **Robot Framework User Guide**: https://robotframework.org/robotframework/latest/RobotFrameworkUserGuide.html

## Support

If tests fail consistently (< 95% pass rate), this may indicate:
- Real synchronization bugs in DirShare
- Environmental issues (system load, network, disk I/O)
- Test infrastructure issues (timing assumptions, race conditions)

Review participant logs and DDS debug output to diagnose root cause.
