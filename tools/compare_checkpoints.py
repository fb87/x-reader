#!/usr/bin/env python3
"""Compare two directories of PGM simulator checkpoints."""
import os
import sys

from PIL import Image, ImageChops


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: compare_checkpoints.py EXPECTED ACTUAL")
    expected, actual = sys.argv[1:]
    names = sorted(name for name in os.listdir(expected) if name.endswith(".pgm"))
    if not names:
        raise SystemExit("no PGM checkpoints found")
    failures = 0
    for name in names:
        left = Image.open(os.path.join(expected, name)).convert("L")
        right = Image.open(os.path.join(actual, name)).convert("L")
        if left.size != right.size:
            print(f"FAIL {name}: size {left.size} != {right.size}")
            failures += 1
            continue
        changed = sum(1 for value in ImageChops.difference(left, right).getdata() if value)
        if changed:
            print(f"FAIL {name}: {changed} pixels differ")
            failures += 1
        else:
            print(f"PASS {name}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
