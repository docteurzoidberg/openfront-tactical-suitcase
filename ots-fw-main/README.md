# OTS Firmware (ots-fw-main)

ESP-IDF firmware for the OpenFront Tactical Suitcase (OTS) hardware.

## What this firmware does

- Connects to WiFi and exchanges events/commands over WebSocket with the OTS system.
- Drives hardware modules (LEDs, buttons, LCD, ADC) through an event-driven module architecture.
- Supports multiple PlatformIO build environments, including focused `test-*` images.

## Quick start

From this folder:

```bash
# Install deps (PlatformIO)
# (Assumes you already have PlatformIO set up in your environment)

# Build production firmware
pio run -e esp32-s3-dev

# Flash production firmware
pio run -e esp32-s3-dev -t upload

# Monitor serial
pio device monitor
```

## Running hardware tests

The `test-*` environments flash small, purpose-built firmware images.

```bash
# Example: I2C scan test
pio run -e test-i2c -t upload
pio device monitor
```

See docs/TESTING.md for the full test matrix and expected output.

## Where to look

- docs/ — human docs (OTA, testing, display modes, etc.)
- prompts/ — AI prompt files for firmware work (task-oriented)
- include/ — public headers
- src/ — firmware source

## Key docs

- docs/TESTING.md
- docs/OTA_GUIDE.md
- docs/WSS_TEST_GUIDE.md
- docs/RGB_LED_STATUS.md
- docs/NAMING_CONVENTIONS.md
- CHANGELOG.md

## Protocol changes

Protocol is defined at the repo root:

- ../prompts/protocol-context.md

If you change the protocol, update the shared types and keep firmware/server/userscript in sync.

## Notes

- Build warning `esp_idf_size: ... --ng` can be ignored unless you’re debugging size tooling.
