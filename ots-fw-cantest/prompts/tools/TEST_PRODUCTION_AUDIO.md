# Scenario: test_production_audio.py (Phase 3.2)

## Purpose

Validate production `ots-fw-audiomodule` against the CAN protocol spec using cantest as the controller.

## Script

- Source: `ots-fw-cantest/tools/test_production_audio.py`

## What it does

- Opens cantest serial (controller) + production audiomodule serial.
- Puts cantest into controller simulator mode.
- Runs discovery + audio protocol checks.
- Watches audiomodule logs for playback confirmation patterns.

## Usage

```bash
python3 ots-fw-cantest/tools/test_production_audio.py --controller-port /dev/ttyACM0 --audio-port /dev/ttyUSB0
```
