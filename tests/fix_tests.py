#!/usr/bin/env python3
import re
import sys

def fix_test_file(filename):
    with open(filename, 'r') as f:
        content = f.read()

    # Replace FileMonitor monitor(test_dir, true) with change_tracker version
    content = re.sub(
        r'(\s+)DirShare::FileMonitor monitor\(test_dir, true\);',
        r'\1DirShare::FileChangeTracker change_tracker;\n\1DirShare::FileMonitor monitor(test_dir, change_tracker, true);',
        content
    )

    # Replace FileMonitor monitor(test_dir) with change_tracker version
    content = re.sub(
        r'(\s+)DirShare::FileMonitor monitor\(test_dir\);',
        r'\1DirShare::FileChangeTracker change_tracker;\n\1DirShare::FileMonitor monitor(test_dir, change_tracker);',
        content
    )

    # Replace FileMonitor monitor(dir) variants
    content = re.sub(
        r'(\s+)DirShare::FileMonitor monitor\(([a-z_]+)\);',
        r'\1DirShare::FileChangeTracker change_tracker;\n\1DirShare::FileMonitor monitor(\2, change_tracker);',
        content
    )

    with open(filename, 'w') as f:
        f.write(content)

    print(f"Fixed {filename}")

if __name__ == "__main__":
    for filename in sys.argv[1:]:
        fix_test_file(filename)
