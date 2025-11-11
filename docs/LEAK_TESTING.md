# Memory Leak Testing Guide

This guide explains how to perform memory leak testing on DirShare using AddressSanitizer and macOS leak detection tools.

## Quick Start

```bash
# 1. Source OpenDDS environment
source ~/dev/OpenDDS/setenv.sh

# 2. Build with AddressSanitizer
./build_with_asan.sh

# 3. Run leak tests
./run_leak_tests.sh
```

## Build Scripts

### build_with_asan.sh

Builds DirShare with AddressSanitizer instrumentation.

**Usage:**
```bash
./build_with_asan.sh           # Build with ASan
./build_with_asan.sh clean     # Clean ASan build
./build_with_asan.sh rebuild   # Clean and rebuild
```

**What it does:**
- Sets compiler flags: `-fsanitize=address -fno-omit-frame-pointer -g`
- Builds main executable and all test suites
- Reports build status and executable sizes

**Requirements:**
- OpenDDS environment must be sourced
- Clang compiler with ASan support (Apple Clang 17.0+ or LLVM)

### run_leak_tests.sh

Runs memory leak tests on DirShare components.

**Usage:**
```bash
./run_leak_tests.sh           # Run all tests
./run_leak_tests.sh unit      # Unit tests only
./run_leak_tests.sh dirshare  # Integration test only
```

**What it tests:**
- **Unit mode**: All 13 Boost.Test suites with ASan
- **DirShare mode**: Running dirshare process with macOS `leaks` tool
- **All mode**: Both unit and integration tests

## Understanding AddressSanitizer on macOS

### What ASan Detects

AddressSanitizer on macOS detects:
- ✅ Heap buffer overflows
- ✅ Stack buffer overflows
- ✅ Use-after-free
- ✅ Use-after-return
- ✅ Double-free
- ✅ Memory corruption

### macOS Limitations

⚠️ **LeakSanitizer is NOT supported on macOS**

When you see:
```
==12345==AddressSanitizer: detect_leaks is not supported on this platform.
```

This is **expected behavior** on macOS. It does NOT indicate a test failure.

### Alternative Leak Detection on macOS

Use the native `leaks` command:

```bash
# Start dirshare
./dirshare -DCPSConfigFile rtps.ini /tmp/test_dir &
DIRSHARE_PID=$!

# Wait for it to run
sleep 10

# Check for leaks
leaks $DIRSHARE_PID

# Cleanup
kill $DIRSHARE_PID
```

## Manual Testing Procedures

### Unit Test Memory Testing

```bash
# Source environment
source ~/dev/OpenDDS/setenv.sh

# Build with ASan
export CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
export LDFLAGS="-fsanitize=address"
make clean && make
cd tests && make clean && make

# Run tests
perl run_tests.pl

# Check for ASan errors in output
# Any memory errors will be reported immediately
```

### Integration Test Memory Testing

```bash
# Build with ASan (as above)

# Run integration test
perl run_test.pl --rtps

# Or manually with leaks:
mkdir /tmp/test_a /tmp/test_b

# Terminal 1
./dirshare -DCPSConfigFile rtps.ini /tmp/test_a &
PID_A=$!

# Terminal 2
./dirshare -DCPSConfigFile rtps.ini /tmp/test_b &
PID_B=$!

# After running for a while
leaks $PID_A > leaks_a.txt
leaks $PID_B > leaks_b.txt

# Cleanup
kill $PID_A $PID_B
```

## Interpreting Results

### AddressSanitizer Output

**Good (no errors):**
```
Running ChecksumBoostTest...
*** No errors detected
```

**Bad (memory error):**
```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
    #0 0x... in function_name file.cpp:123
    ...
```

### macOS Leaks Output

**Good (no leaks):**
```
Process 12345: 0 leaks for 0 total leaked bytes.
```

**Bad (leaks detected):**
```
Process 12345: 5 leaks for 240 total leaked bytes.
Leak: 0x12345678  size=48  zone: DefaultMallocZone
    Call stack: [...]
```

## Continuous Integration

### Adding to CI Pipeline

```yaml
# Example GitHub Actions
- name: Build with AddressSanitizer
  run: ./build_with_asan.sh

- name: Run Memory Tests
  run: ./run_leak_tests.sh unit
```

### Pre-commit Hook

Add to `.git/hooks/pre-commit`:
```bash
#!/bin/bash
if [ -f "./build_with_asan.sh" ]; then
    echo "Running memory leak tests..."
    ./build_with_asan.sh rebuild
    ./run_leak_tests.sh unit
    if [ $? -ne 0 ]; then
        echo "Memory tests failed. Commit aborted."
        exit 1
    fi
fi
```

## Linux Testing (Recommended for Complete Coverage)

For full LeakSanitizer support, test on Linux:

```bash
# On Linux
export CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
export LDFLAGS="-fsanitize=address"
export ASAN_OPTIONS=detect_leaks=1

make clean && make
cd tests && make clean && make
perl run_tests.pl

# Any leaks will be reported automatically on exit
```

## Troubleshooting

### "OpenDDS environment not sourced"
```bash
source ~/dev/OpenDDS/setenv.sh
```

### Build fails with sanitizer errors
- Check compiler version: `clang++ --version`
- Ensure Xcode Command Line Tools are installed
- Try without parallel build: `make` (not `make -j4`)

### Tests pass but you want more coverage
- Run longer integration tests (hours)
- Use Valgrind on Linux: `valgrind --leak-check=full ./dirshare ...`
- Enable OpenDDS debug logging to catch resource leaks

### False positives from OpenDDS/ACE
- Expected: Some OpenDDS/ACE static allocations may appear as leaks
- Build OpenDDS with ASan for clean results: `--sanitizer=address` in configure

## Best Practices

1. **Regular Testing**: Run leak tests before each commit
2. **Clean Builds**: Always `make clean` when switching sanitizer modes
3. **Long Runs**: Run integration tests for extended periods (1+ hours)
4. **Multiple Platforms**: Test on both macOS and Linux
5. **Document Issues**: Save leak reports with git commits

## Files and Artifacts

- `build_with_asan.sh` - Build script for ASan builds
- `run_leak_tests.sh` - Automated leak testing script
- `MEMORY_LEAK_TEST_REPORT.md` - Detailed test report
- `.gitignore` - Configured to ignore test artifacts
- `/tmp/leaks_report.txt` - Latest leak analysis
- `asan_unit*` - AddressSanitizer log files (if generated)

## Additional Resources

- [AddressSanitizer Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
- [macOS leaks command](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/FindingLeaks.html)
- [OpenDDS Performance Tuning](https://opendds.org/documents/OpenDDS-latest.pdf)

## Support

For issues or questions:
1. Check build output for specific errors
2. Review MEMORY_LEAK_TEST_REPORT.md
3. Consult OpenDDS documentation for DDS-specific issues
