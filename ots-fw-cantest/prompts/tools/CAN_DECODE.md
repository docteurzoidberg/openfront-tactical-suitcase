# Tool: can_decode.py

## Purpose

Decode JSONL CAN frames (from `can_monitor.py`) into higher-level message objects.

Uses `ots-fw-cantest/tools/lib/protocol.py`.

## Script

- Source: `ots-fw-cantest/tools/can_decode.py`

## Usage

```bash
python3 ots-fw-cantest/tools/can_decode.py < frames.jsonl > decoded.jsonl
```

## Input

- JSON Lines where each line represents a raw CAN frame.

## Output

- JSON Lines with additional `decoded` fields (message type, parsed fields, etc.).

## Notes

- If the protocol decoder changes shape, update scenario tests accordingly.
