# Tool: serial_expect.py

## Purpose

An “expect”-style serial matcher.

Waits for a substring match or regex match from a serial port, then exits.

## Script

- Source: `ots-fw-cantest/tools/serial_expect.py`

## Usage

Examples:

```bash
# Wait for a simple substring
python3 ots-fw-cantest/tools/serial_expect.py --port /dev/ttyACM0 --contains "AUDIO MODULE SIMULATOR" --timeout 5

# Wait for a regex
python3 ots-fw-cantest/tools/serial_expect.py --port /dev/ttyACM0 --regex "State: (RUNNING|BUS_OFF)" --timeout 5
```

## Output

- Prints the matched line (or diagnostic info on timeout).

## Recommended use

- Gate steps in shell scripts (flash → wait for marker → run test).
- Detect BUS_OFF transitions quickly.
