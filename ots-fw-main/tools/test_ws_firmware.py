#!/usr/bin/env python3
"""Compatibility wrapper for the older entrypoint.

Reusable primitives are in:
- tools/ots_device_tool.py

Actual test cases live in:
- tools/tests/

This wrapper delegates to:
- tools/tests/userscript_connect_disconnect.py
"""

from __future__ import annotations

import os
import runpy
import sys


def main() -> int:
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)

    target = os.path.join(repo_root, "tools", "tests", "userscript_connect_disconnect.py")
    sys.argv[0] = target
    runpy.run_path(target, run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
