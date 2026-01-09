# Tool: flash_firmware.py

## Purpose

Flash firmware to a device using PlatformIO from a specified project directory.

This is the standard way to automate “build+upload” for:
- `ots-fw-cantest`
- `ots-fw-main`
- `ots-fw-audiomodule`

## Script

- Source: `ots-fw-cantest/tools/flash_firmware.py`

## Usage

```bash
python3 ots-fw-cantest/tools/flash_firmware.py \
  --project <path-to-platformio-project> \
  --env <platformio-env> \
  --port <serial-port>
```

Common examples:

```bash
python3 ots-fw-cantest/tools/flash_firmware.py --project ots-fw-cantest --env esp32-s3-devkit --port /dev/ttyACM0
python3 ots-fw-cantest/tools/flash_firmware.py --project ots-fw-audiomodule --env esp32-a1s-espidf --port /dev/ttyUSB0
```

## Output

- Human-readable PlatformIO output (and whatever parsing the script performs).

## Notes

- Prefer using the smallest build/env needed to validate the change.
- PlatformIO environment names must match the target project’s `platformio.ini`.
