# Tool: dual_monitor.py

## Purpose

Run two serial monitors side-by-side (useful for two-node debugging).

This is an interactive helper, not a JSONL pipeline tool.

## Script

- Source: `ots-fw-cantest/tools/dual_monitor.py`

## Notes / gotchas

- The script historically uses hardcoded default ports.
- Prefer scenario runners for automation; use this when you’re diagnosing live behavior.
