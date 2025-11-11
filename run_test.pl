eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# run_test.pl - Integration test script for DirShare
# Tests DirShare functionality with both InfoRepo and RTPS discovery modes
#
# Usage:
#   perl run_test.pl           # Run InfoRepo mode test (default)
#   perl run_test.pl --rtps    # Run RTPS mode test

use strict;
use Env qw(DDS_ROOT ACE_ROOT);
use lib "$DDS_ROOT/bin";
use lib "$ENV{ACE_ROOT}/bin";
use PerlDDS::Run_Test;
use File::Path qw(make_path remove_tree);
use File::Spec;
use Cwd;

my $status = 0;
my $test_opts = new PerlACE::ConfigList->check_config('DCPS') ? '' : '';
my $rtps = grep { $_ eq '--rtps' } @ARGV;

# Get absolute path to executable
my $dirshare = File::Spec->rel2abs('./dirshare');

if (! -f $dirshare) {
    print STDERR "ERROR: DirShare executable not found at: $dirshare\n";
    print STDERR "Please run 'make' to build dirshare first.\n";
    exit 1;
}

# Test configuration
my $test_dir = "run_test_dir_$$";
my $dir_a = "$test_dir/participant_A";
my $dir_b = "$test_dir/participant_B";

# Create test directories
make_path($dir_a, $dir_b);

# Create test files in participant A
print "Creating test files in $dir_a\n";
open my $fh, '>', "$dir_a/test1.txt" or die "Cannot create file: $!";
print $fh "Test file 1 content\n";
close $fh;

open $fh, '>', "$dir_a/test2.txt" or die "Cannot create file: $!";
print $fh "Test file 2 content\n";
close $fh;

# Create test file in participant B
print "Creating test file in $dir_b\n";
open $fh, '>', "$dir_b/testB.txt" or die "Cannot create file: $!";
print $fh "Test file B content\n";
close $fh;

print "\n";
print "=" x 70 . "\n";
print "DirShare Integration Test\n";
print "Discovery Mode: " . ($rtps ? "RTPS" : "InfoRepo") . "\n";
print "=" x 70 . "\n\n";

# Initialize processes
my $inforepo = undef;
my $dirshare_a = undef;
my $dirshare_b = undef;

if ($rtps) {
    #
    # RTPS Mode Test
    #
    print "Starting DirShare participants in RTPS mode...\n\n";

    my $rtps_config = File::Spec->rel2abs('./rtps.ini');

    if (! -f $rtps_config) {
        print STDERR "ERROR: RTPS configuration file not found: $rtps_config\n";
        $status = 1;
        goto cleanup;
    }

    # Participant A
    $dirshare_a = PerlDDS::create_process(
        $dirshare,
        "-DCPSConfigFile $rtps_config $dir_a"
    );

    # Participant B
    $dirshare_b = PerlDDS::create_process(
        $dirshare,
        "-DCPSConfigFile $rtps_config $dir_b"
    );

    print "Starting Participant A...\n";
    $dirshare_a->Spawn();

    print "Starting Participant B...\n";
    $dirshare_b->Spawn();

} else {
    #
    # InfoRepo Mode Test
    #
    print "Starting DCPSInfoRepo...\n";

    my $ior_file = "repo.ior";
    unlink $ior_file;

    my $DCPSREPO = PerlDDS::create_process(
        "$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
        "-o $ior_file"
    );

    $inforepo = $DCPSREPO;
    $DCPSREPO->Spawn();

    if (PerlACE::waitforfile_timed($ior_file, 30) == -1) {
        print STDERR "ERROR: cannot find file <$ior_file>\n";
        $DCPSREPO->Kill();
        $status = 1;
        goto cleanup;
    }

    my $ior_abs = File::Spec->rel2abs($ior_file);
    print "DCPSInfoRepo started, IOR: $ior_abs\n\n";

    # Participant A
    $dirshare_a = PerlDDS::create_process(
        $dirshare,
        "-DCPSInfoRepo file://$ior_abs $dir_a"
    );

    # Participant B
    $dirshare_b = PerlDDS::create_process(
        $dirshare,
        "-DCPSInfoRepo file://$ior_abs $dir_b"
    );

    print "Starting Participant A...\n";
    $dirshare_a->Spawn();

    print "Starting Participant B...\n";
    $dirshare_b->Spawn();
}

# Wait for synchronization and file propagation
# Allow time for:
#  - DDS discovery (up to 30s)
#  - Initial directory synchronization
#  - FileMonitor polling (2s interval)
#  - File transfer
print "\nWaiting 40 seconds for DDS discovery and file synchronization...\n";
sleep(40);

# Verify file synchronization
print "\nVerifying file synchronization...\n";
print "-" x 70 . "\n";

my $test_passed = 1;

# Check that files from A appeared in B
my @files_from_a = ("$dir_b/test1.txt", "$dir_b/test2.txt");
for my $file (@files_from_a) {
    if (-f $file) {
        print "✓ File synchronized: $file\n";
    } else {
        print "✗ FAILED: File not found: $file\n";
        $test_passed = 0;
    }
}

# Check that file from B appeared in A
my @files_from_b = ("$dir_a/testB.txt");
for my $file (@files_from_b) {
    if (-f $file) {
        print "✓ File synchronized: $file\n";
    } else {
        print "✗ FAILED: File not found: $file\n";
        $test_passed = 0;
    }
}

print "-" x 70 . "\n\n";

if ($test_passed) {
    print "✓ TEST PASSED: All files synchronized successfully\n";
    $status = 0;
} else {
    print "✗ TEST FAILED: Some files were not synchronized\n";
    $status = 1;
}

# Cleanup processes
print "\nStopping DirShare participants...\n";

if (defined $dirshare_a) {
    $dirshare_a->Kill();
}

if (defined $dirshare_b) {
    $dirshare_b->Kill();
}

if (defined $inforepo) {
    print "Stopping DCPSInfoRepo...\n";
    $inforepo->Kill();
}

# Wait for processes to terminate
sleep(2);

cleanup:

# Cleanup test directories and files
print "Cleaning up test directories...\n";
remove_tree($test_dir);
unlink 'repo.ior' if -f 'repo.ior';

print "\n";
print "=" x 70 . "\n";
print "Test Result: " . ($status == 0 ? "PASS" : "FAIL") . "\n";
print "=" x 70 . "\n";

exit $status;
