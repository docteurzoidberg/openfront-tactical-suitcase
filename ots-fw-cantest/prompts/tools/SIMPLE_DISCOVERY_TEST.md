# Scenario: simple_discovery_test.py

## Purpose

Minimal check: watch a cantest monitor port and verify that a `MODULE_QUERY (0x411)` appears.

## Script

- Source: `ots-fw-cantest/tools/simple_discovery_test.py`

## Usage

```bash
python3 ots-fw-cantest/tools/simple_discovery_test.py
```

## Notes

- The script currently uses a hardcoded port (`/dev/ttyUSB0`).
- Use when you only need a “did anything send 0x411?” sanity check.
