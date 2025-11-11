#!/bin/bash
# run_leak_tests.sh - Run memory leak tests on DirShare
#
# Usage:
#   ./run_leak_tests.sh           # Run all leak tests
#   ./run_leak_tests.sh unit      # Run unit tests only
#   ./run_leak_tests.sh dirshare  # Run dirshare leak test only

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if OpenDDS environment is sourced
if [ -z "$DDS_ROOT" ]; then
    echo -e "${RED}ERROR: OpenDDS environment not sourced${NC}"
    echo "Please run: source ~/dev/OpenDDS/setenv.sh"
    exit 1
fi

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}DirShare Memory Leak Testing${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if built with ASan
if ! otool -L dirshare 2>/dev/null | grep -q "AddressSanitizer" && ! ldd dirshare 2>/dev/null | grep -q "asan"; then
    echo -e "${YELLOW}WARNING: Executables may not be built with AddressSanitizer${NC}"
    echo "Consider running: ./build_with_asan.sh"
    echo ""
fi

MODE="${1:-all}"

run_unit_tests() {
    echo -e "${YELLOW}Running Unit Tests with AddressSanitizer...${NC}"
    echo ""

    cd tests
    export ASAN_OPTIONS=detect_leaks=1:log_path=asan_unit
    perl run_tests.pl
    RESULT=$?
    cd ..

    if [ $RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ All unit tests passed${NC}"
    else
        echo -e "${RED}✗ Some unit tests failed${NC}"
        return 1
    fi
    echo ""
}

run_dirshare_leak_test() {
    echo -e "${YELLOW}Running DirShare Leak Test...${NC}"
    echo ""

    # Create test directory
    TEST_DIR="/tmp/leak_test_$$"
    mkdir -p "$TEST_DIR"

    echo "Starting DirShare in background..."
    ./dirshare -DCPSConfigFile rtps.ini "$TEST_DIR" > /tmp/dirshare_leak.log 2>&1 &
    DIRSHARE_PID=$!

    echo "DirShare PID: $DIRSHARE_PID"
    echo "Waiting 10 seconds for initialization..."
    sleep 10

    # Check if process is still running
    if ! kill -0 $DIRSHARE_PID 2>/dev/null; then
        echo -e "${RED}✗ DirShare process died unexpectedly${NC}"
        cat /tmp/dirshare_leak.log
        rm -rf "$TEST_DIR"
        return 1
    fi

    # On macOS, use leaks command
    if command -v leaks &> /dev/null; then
        echo -e "${YELLOW}Running macOS leaks analysis...${NC}"
        echo ""
        leaks $DIRSHARE_PID > /tmp/leaks_report.txt 2>&1 || true

        # Show summary
        if grep -q "0 leaks for 0 total leaked bytes" /tmp/leaks_report.txt; then
            echo -e "${GREEN}✓ No memory leaks detected${NC}"
            LEAK_STATUS=0
        else
            echo -e "${YELLOW}Leak analysis results:${NC}"
            grep "leaks for" /tmp/leaks_report.txt || echo "  (Unable to determine leak status)"
            echo ""
            echo "Full report saved to: /tmp/leaks_report.txt"
            LEAK_STATUS=1
        fi
    else
        echo -e "${YELLOW}Note: 'leaks' command not available${NC}"
        LEAK_STATUS=0
    fi

    # Cleanup
    echo ""
    echo "Stopping DirShare..."
    kill $DIRSHARE_PID 2>/dev/null || true
    wait $DIRSHARE_PID 2>/dev/null || true

    rm -rf "$TEST_DIR"

    return $LEAK_STATUS
}

# Run tests based on mode
case "$MODE" in
    unit)
        run_unit_tests
        ;;
    dirshare)
        run_dirshare_leak_test
        ;;
    all)
        run_unit_tests
        echo ""
        run_dirshare_leak_test
        ;;
    *)
        echo -e "${RED}ERROR: Invalid mode '$MODE'${NC}"
        echo "Usage: $0 [unit|dirshare|all]"
        exit 1
        ;;
esac

FINAL_RESULT=$?

echo ""
echo -e "${BLUE}========================================${NC}"
if [ $FINAL_RESULT -eq 0 ]; then
    echo -e "${GREEN}Memory Leak Testing Complete!${NC}"
else
    echo -e "${YELLOW}Memory Leak Testing Complete (with warnings)${NC}"
fi
echo -e "${BLUE}========================================${NC}"
echo ""

exit $FINAL_RESULT
