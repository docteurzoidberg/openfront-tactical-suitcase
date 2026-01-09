# Tool: can_send.py

## Purpose

Higher-level helper to drive cantest via serial.

It wraps “connect + optional reset/boot capture + send command(s)” into a single CLI.

## Script

- Source: `ots-fw-cantest/tools/can_send.py`

## Output

- Designed for automation; may emit JSON for structured consumption.

## Recommended use

- Quick scripted transitions: idle → controller sim → send discovery.
- Combine with `can_monitor.py` for verifying resulting bus traffic.
