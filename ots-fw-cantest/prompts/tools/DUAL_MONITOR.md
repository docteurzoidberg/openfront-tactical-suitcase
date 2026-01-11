# Tool: dual_monitor.py

## Purpose

Run two serial monitors side-by-side (useful for two-node debugging).

This is an interactive helper, not a JSONL pipeline tool.

## Script

- Source: `ots-fw-cantest/tools/dual_monitor.py`

## Notes / gotchas

- The script has defaults (`/dev/ttyUSB0` for audio, `/dev/ttyACM0` for controller), but you can override via CLI flags.
- Prefer scenario runners for automation; use this when you’re diagnosing live behavior.

## Usage

```bash
python3 ots-fw-cantest/tools/dual_monitor.py --controller /dev/ttyACM0 --audio /dev/ttyUSB0
```
