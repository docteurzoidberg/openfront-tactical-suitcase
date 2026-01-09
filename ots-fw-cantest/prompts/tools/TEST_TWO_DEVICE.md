# Scenario: test_two_device.py (Phase 2.5 baseline)

## Purpose

Validate cantest↔cantest communication and protocol compliance in a controlled setup.

## Script

- Source: `ots-fw-cantest/tools/test_two_device.py`

## What it does

- Opens 2 serial ports simultaneously.
- Puts one device into **controller simulator** mode and the other into **audio simulator** mode.
- Runs discovery and audio protocol scenarios.
- Captures and decodes frames using `tools/lib/protocol.py`.

## Usage

```bash
python3 ots-fw-cantest/tools/test_two_device.py --controller-port /dev/ttyACM0 --audio-port /dev/ttyUSB0
```

## Pass criteria

- Controller sends MODULE_QUERY (0x411).
- Audio receives it and sends MODULE_ANNOUNCE (0x410).
- Controller receives announce.
