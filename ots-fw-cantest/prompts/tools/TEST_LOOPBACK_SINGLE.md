# Scenario: test_loopback_single.py

## Purpose

Single-device loopback sanity check.

Validates that the device can TX and also RX its own frames in loopback/NO_ACK configuration.

## Script

- Source: `ots-fw-cantest/tools/test_loopback_single.py`

## Usage

```bash
python3 ots-fw-cantest/tools/test_loopback_single.py /dev/ttyACM0 "ESP32-S3"
python3 ots-fw-cantest/tools/test_loopback_single.py /dev/ttyUSB0 "ESP32-A1S"
```
