#!/usr/bin/env perl

use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/../../../..";
use Env qw(ACE_ROOT DDS_ROOT);

my $status = 0;
my $passed = 0;
my $failed = 0;

# Color output for terminal
my $RED = "\033[0;31m";
my $GREEN = "\033[0;32m";
my $YELLOW = "\033[0;33m";
my $NC = "\033[0m"; # No Color

sub run_test {
    my ($test_name, $test_exe) = @_;

    print "${YELLOW}=== Running $test_name ===${NC}\n";

    my $result = system("./$test_exe");
    my $exit_code = $? >> 8;

    if ($exit_code == 0) {
        print "${GREEN}✓ $test_name PASSED${NC}\n\n";
        $passed++;
        return 0;
    } else {
        print "${RED}✗ $test_name FAILED (exit code: $exit_code)${NC}\n\n";
        $failed++;
        return 1;
    }
}

print "\n";
print "╔══════════════════════════════════════════════╗\n";
print "║   DirShare Component Unit Tests              ║\n";
print "╔══════════════════════════════════════════════╗\n";
print "\n";

# Check if tests are built
unless (-f "./ChecksumBoostTest" || -f "./ChecksumBoostTest.exe") {
    print "${RED}ERROR: Tests not built. Please run 'make' or 'mwc.pl -type gnuace tests.mpc && make' first.${NC}\n";
    exit 1;
}

print "${YELLOW}--- Phase 2: Foundation Tests ---${NC}\n\n";

# Run Phase 2 Boost.Test suites
$status |= run_test("ChecksumBoostTest", "ChecksumBoostTest");
$status |= run_test("FileUtilsBoostTest", "FileUtilsBoostTest");
$status |= run_test("FileMonitorBoostTest", "FileMonitorBoostTest");

print "${YELLOW}--- Phase 3: User Story 1 (Initial Sync) Tests ---${NC}\n\n";

# Run Phase 3 Boost.Test suites (US1 - Initial Directory Synchronization)
$status |= run_test("DirectorySnapshotBoostTest", "DirectorySnapshotBoostTest");
$status |= run_test("FileContentBoostTest", "FileContentBoostTest");
$status |= run_test("FileChunkBoostTest", "FileChunkBoostTest");

# Summary
print "╔══════════════════════════════════════════════╗\n";
print "║              Test Summary                    ║\n";
print "╔══════════════════════════════════════════════╗\n";
print "  Total Tests: " . ($passed + $failed) . "\n";
print "  ${GREEN}Passed: $passed${NC}\n";
print "  ${RED}Failed: $failed${NC}\n";
print "\n";

if ($status == 0) {
    print "${GREEN}✓ All tests passed!${NC}\n";
} else {
    print "${RED}✗ Some tests failed.${NC}\n";
}

exit $status;
