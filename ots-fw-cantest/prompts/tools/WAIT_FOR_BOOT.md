# Tool: wait_for_boot.py

## Purpose

Wait for one (or more) boot marker strings to appear on a serial port.

Useful after flashing/resetting when scripts need to block until the firmware is ready.

## Script

- Source: `ots-fw-cantest/tools/wait_for_boot.py`

## Usage

Typical pattern:

```bash
python3 ots-fw-cantest/tools/wait_for_boot.py --port /dev/ttyACM0 --marker "READY" --timeout 15
```

Notes:
- Supports multiple `--marker` arguments (first match wins).
- Intended for automation; exit code should reflect success/failure.

## Recommended use

- After `flash_firmware.py`.
- Before sending mode-switch commands to cantest.
