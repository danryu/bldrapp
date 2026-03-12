#!/usr/bin/env python3

import os
import subprocess
import sys


def run(script: str) -> int:
    env = os.environ.copy()
    print(f"[runner] Running {script}")
    p = subprocess.run([sys.executable, script], env=env)
    return p.returncode


def main() -> int:
    base = os.path.dirname(__file__)
    tests = [
        os.path.join(base, "test_basic_flow.py"),
    ]
    for t in tests:
        rc = run(t)
        if rc != 0:
            return rc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


