# Tool: can_monitor.py

## Purpose

Convert cantest “monitor mode” serial output into machine-readable **JSON Lines**.

This is the core capture primitive for CAN traffic.

## Script

- Source: `ots-fw-cantest/tools/can_monitor.py`

## Usage

```bash
python3 ots-fw-cantest/tools/can_monitor.py --port /dev/ttyACM0 --duration 10 > frames.jsonl
```

## Output format (JSONL)

One object per line (approx):
- timestamp
- direction (rx/tx)
- can_id
- dlc
- data (bytes)
- raw_line (when present)

## Composition

```bash
python3 ots-fw-cantest/tools/can_monitor.py --port /dev/ttyACM0 --duration 10 | \
  python3 ots-fw-cantest/tools/can_decode.py > decoded.jsonl
```
