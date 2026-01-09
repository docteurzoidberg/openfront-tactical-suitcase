# Scenario: test_loopback.py

## Purpose

Two-device loopback-focused CAN debugging.

- Sends a test frame from one node.
- Compares TX/RX deltas on both nodes.
- Helps distinguish “driver/pin issue” vs “bus wiring/termination issue”.

## Script

- Source: `ots-fw-cantest/tools/test_loopback.py`

## Usage

```bash
python3 ots-fw-cantest/tools/test_loopback.py /dev/ttyACM0 /dev/ttyUSB0
```
