#!/usr/bin/env python3
"""
Debug script to test DirShareLibrary startup
"""

import sys
sys.path.insert(0, 'libraries')

from DirShareLibrary import DirShareLibrary

# Create library instance
lib = DirShareLibrary()

print(f"DirShare executable: {lib.dirshare_exe}")
print(f"DCPSInfoRepo executable: {lib.dcpsinfo_repo_exe}")

# Create test directories
dir_a = lib.create_test_directory("A")
dir_b = lib.create_test_directory("B")

print(f"Test directory A: {dir_a}")
print(f"Test directory B: {dir_b}")

# Try to start DirShare with InfoRepo
try:
    print("\n--- Starting DirShare with InfoRepo ---")
    lib.start_dirshare_inforepo("A", dir_a)
    print(f"DirShare A started successfully")
    print(f"InfoRepo process: {lib.inforepo_process}")
    print(f"InfoRepo IOR file: {lib.inforepo_ior_file}")

    # Wait a bit
    import time
    time.sleep(3)

    # Start second instance
    lib.start_dirshare_inforepo("B", dir_b)
    print(f"DirShare B started successfully")

    # Wait for discovery
    time.sleep(5)

    # Check processes
    print(f"\nDirShare A running: {lib.dirshare_is_running('A')}")
    print(f"DirShare B running: {lib.dirshare_is_running('B')}")

except Exception as e:
    print(f"ERROR: {e}")
    import traceback
    traceback.print_exc()

finally:
    # Cleanup
    print("\n--- Cleaning up ---")
    lib.stop_all_dirshare()
    lib.cleanup_all_test_directories()
