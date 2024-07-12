#!/usr/bin/env python3
import os.path
import subprocess
import sys


def main():
    fd_dir = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))
    os.chdir(os.path.join(fd_dir, "src", "search"))
    commit_hash = subprocess.getoutput('git log -1 --format="%h"')
    if os.path.exists("version.h"):
        with open("version.h") as f:
            read_commit_hash = f.read().splitlines()[1].split()[2].replace('"', '')
            if commit_hash == read_commit_hash:
                print("Commit has not changed.")
                return
    with open("version.h", "w+") as f:
        print("Writing version.h")
        f.write(f'#pragma once\n#define DOWNWARD_COMMIT "{commit_hash}"\n')


if __name__ == "__main__":
    main()
