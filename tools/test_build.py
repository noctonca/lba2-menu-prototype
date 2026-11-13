#!/usr/bin/env python3
"""Build the optimised copy the tests run, and print its path.

    tools/test_build.py

The tests run build-test/lba2menu, a RelWithDebInfo build: the self test is about
five times faster than in the Debug build/, and the symbols stay for a profile.
It is configured the first time and built every time, so a test never runs a
binary older than the source (a build with nothing to do takes under a second).
"""
import os, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
BUILD = os.path.join(ROOT, "build-test")
BIN = os.path.join(BUILD, "lba2menu")

def ensure():
    """Configure (first time) and build build-test; return the binary's path."""
    if not os.path.exists(os.path.join(BUILD, "CMakeCache.txt")):
        subprocess.run(["cmake", "-S", ROOT, "-B", BUILD, "-G", "Ninja",
                        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"],
                       check=True, stdout=sys.stderr)
    subprocess.run(["cmake", "--build", BUILD], check=True, stdout=sys.stderr)
    return BIN

if __name__ == "__main__":
    print(ensure())
