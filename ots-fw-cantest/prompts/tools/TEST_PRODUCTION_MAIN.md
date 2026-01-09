# Scenario: test_production_main.py (Phase 3.3)

## Purpose

Validate production `ots-fw-main` boot discovery behavior, using cantest as an audio simulator.

## Script

- Source: `ots-fw-cantest/tools/test_production_main.py`

## What it does

- Opens production main serial + cantest serial.
- Puts cantest into audio simulator mode.
- Observes whether main emits MODULE_QUERY on boot and whether announce handling works.

## Usage

```bash
python3 ots-fw-cantest/tools/test_production_main.py --main-port /dev/ttyACM0 --audio-port /dev/ttyUSB0
```
