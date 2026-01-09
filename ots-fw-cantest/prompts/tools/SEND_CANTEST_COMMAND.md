# Tool: send_cantest_command.py

## Purpose

Send a single cantest CLI command (one line) to a device over serial.

Useful for scripting mode switches like `m`, `a`, `c`, and common test triggers.

## Script

- Source: `ots-fw-cantest/tools/send_cantest_command.py`

## Usage

```bash
python3 ots-fw-cantest/tools/send_cantest_command.py --port /dev/ttyACM0 --cmd a
python3 ots-fw-cantest/tools/send_cantest_command.py --port /dev/ttyACM0 --cmd t
```

## Notes

- The tool is intentionally lightweight.
- Some cantest commands are single-character and may not emit a structured response; this script is tolerant of that.
