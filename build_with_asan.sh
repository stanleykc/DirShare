#!/bin/bash
# build_with_asan.sh - Build DirShare with AddressSanitizer for memory leak testing
#
# Usage:
#   ./build_with_asan.sh           # Build with ASan
#   ./build_with_asan.sh clean     # Clean ASan build
#   ./build_with_asan.sh rebuild   # Clean and rebuild with ASan

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if OpenDDS environment is sourced
if [ -z "$DDS_ROOT" ]; then
    echo -e "${RED}ERROR: OpenDDS environment not sourced${NC}"
    echo "Please run: source ~/dev/OpenDDS/setenv.sh"
    exit 1
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}DirShare AddressSanitizer Build${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Export AddressSanitizer flags
export CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
export LDFLAGS="-fsanitize=address"

echo -e "${YELLOW}Build Configuration:${NC}"
echo "  CXXFLAGS: $CXXFLAGS"
echo "  LDFLAGS:  $LDFLAGS"
echo ""

# Handle command line arguments
if [ "$1" = "clean" ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    make clean
    cd tests && make clean && cd ..
    echo -e "${GREEN}Clean complete${NC}"
    exit 0
fi

if [ "$1" = "rebuild" ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    make clean
    cd tests && make clean && cd ..
    echo ""
fi

# Build main project
echo -e "${YELLOW}Building DirShare with AddressSanitizer...${NC}"
make -j4

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ DirShare built successfully${NC}"
    ls -lh dirshare libDirShare.dylib 2>/dev/null || ls -lh dirshare libDirShare.so 2>/dev/null
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

echo ""

# Build tests
echo -e "${YELLOW}Building tests with AddressSanitizer...${NC}"
cd tests
make -j4

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Tests built successfully${NC}"
    echo ""
    echo "Test executables:"
    ls -lh *BoostTest 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
else
    echo -e "${RED}✗ Test build failed${NC}"
    exit 1
fi

cd ..

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}Important Notes:${NC}"
echo "  • LeakSanitizer is NOT supported on macOS"
echo "  • AddressSanitizer will detect memory errors (buffer overflows, use-after-free, etc.)"
echo "  • Run tests with: cd tests && perl run_tests.pl"
echo "  • For leak testing on macOS, use: leaks <pid>"
echo ""
echo -e "${YELLOW}Memory Error Detection:${NC}"
echo "  ASan will automatically detect and report:"
echo "    - Heap buffer overflows"
echo "    - Stack buffer overflows"
echo "    - Use-after-free"
echo "    - Double-free"
echo "    - Memory corruption"
echo ""
