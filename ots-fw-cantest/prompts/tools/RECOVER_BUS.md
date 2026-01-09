# Tool: recover_bus.py (WIP)

## Purpose

Attempt to recover devices from TWAI BUS_OFF state.

## Script

- Source: `ots-fw-cantest/tools/recover_bus.py`

## Current behavior

- Connects to two hardcoded ports.
- Sends `t` to check status.
- If BUS_OFF is detected, it currently reports that a recovery CLI command is not implemented.

## Follow-up work

If the cantest firmware CLI adds an `r` command, this tool can be updated to:
- send `r` (recover)
- verify state returns to RUNNING

Until then, treat this as a *status checker*.
