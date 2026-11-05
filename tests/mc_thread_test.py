#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""Every mixer mode must match the serial render, including the shared random streams."""
import re
import subprocess
import sys

binary = sys.argv[1]
for scene in ("mixed", "voice", "twovoice", "parallel", "shared", "transition", "fade", "polyphony"):
    hashes = []
    for mode in ("serial", "deferred", "threaded", "chaos"):
        result = subprocess.run([binary, mode, scene], text=True, capture_output=True)
        if result.returncode:
            sys.stderr.write(result.stdout + result.stderr)
            result.check_returncode()
        print(result.stdout.strip())
        hashes.append(re.search(r"hash=([0-9a-f]+)", result.stdout).group(1))
    if len(set(hashes)) != 1:
        raise SystemExit("mc-thread: output / random state mismatch in " + scene)
subprocess.run([binary, "timeout", "parallel"], check=True)
print("mc-thread: all mixer modes and timeout recovery PASS")
