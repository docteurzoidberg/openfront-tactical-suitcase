# Scenario: test_discovery_fixed.py

## Purpose

Debugging harness for discovery: capture outputs from BOTH devices to avoid missing traffic.

## Script

- Source: `ots-fw-cantest/tools/test_discovery_fixed.py`

## Usage

Ports are currently hardcoded inside the script.

```bash
python3 ots-fw-cantest/tools/test_discovery_fixed.py
```

## What it checks

- Controller TX query
- Audio RX query
- Audio TX announce
- Controller RX announce
