# Debugging & Recovery Prompt (ots-fw-main)

## First Checks

- Confirm you’re monitoring the correct port:
  - Usually `/dev/ttyACM0` (USB Serial/JTAG)
- Confirm you built the intended environment:
  - `pio run -e esp32-s3-dev` vs `pio run -e test-*`

## If It "Doesn’t Boot" / No Logs

- Try `pio device monitor --port /dev/ttyACM0 --baud 115200`
- If still nothing, follow `docs/ERROR_RECOVERY.md`.

## Common Firmware Pitfalls

- Hard-failing when optional I2C hardware is missing (e.g., MCP23017 NACK) can block WiFi/WS tests.
- Event queue overloads: avoid flooding INFO-like events into the dispatcher.
- Wrong test selected -> link errors or missing `app_main`.

## WebSocket/WSS Debug

- Use `test-websocket` env for minimal reproduction.
- For WSS specifics, follow `docs/WSS_TEST_GUIDE.md`.

## Status LED Debug

- Reference `docs/RGB_LED_STATUS.md` for expected colors/meaning.
- Prefer changing LED state on actual state transitions/events (not on timers alone).
