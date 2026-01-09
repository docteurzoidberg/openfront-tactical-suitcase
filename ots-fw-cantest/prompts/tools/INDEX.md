# cantest tools — Index

This index documents the automation-friendly CLI tools in `ots-fw-cantest/tools/`.

## Philosophy

- Prefer **Unix-style composable tools**.
- When possible, produce **JSON Lines** (one JSON object per line) so output can be piped:
  - `can_monitor.py` → JSONL frames
  - `can_decode.py` → JSONL decoded frames

## Common prerequisites

- Python 3 + `pyserial`
- Serial ports accessible (Linux: `/dev/ttyACM*`, `/dev/ttyUSB*`)
- Firmware built/flashed via PlatformIO (`pio run -e <env> -t upload`)

## Tools

- [FLASH_FIRMWARE](FLASH_FIRMWARE.md) — Flash a PlatformIO environment to a device.
- [WAIT_FOR_BOOT](WAIT_FOR_BOOT.md) — Wait for one of several boot markers.
- [SERIAL_EXPECT](SERIAL_EXPECT.md) — Expect-style matcher (substring/regex) for serial logs.
- [SEND_CANTEST_COMMAND](SEND_CANTEST_COMMAND.md) — Send one CLI command to cantest.
- [CAN_SEND](CAN_SEND.md) — Higher-level CLI for driving cantest actions.
- [CAN_MONITOR](CAN_MONITOR.md) — Convert cantest monitor output into JSONL.
- [CAN_DECODE](CAN_DECODE.md) — Decode JSONL frames using `tools/lib/protocol.py`.
- [DUAL_MONITOR](DUAL_MONITOR.md) — Two-port live monitor (interactive helper).

### Test scripts (scenario runners)

These are orchestration scripts (not generic tools), but they’re included here because they’re part of the documented workflow:

- [TEST_TWO_DEVICE](TEST_TWO_DEVICE.md) — Phase 2.5 baseline: cantest ↔ cantest.
- [TEST_PRODUCTION_AUDIO](TEST_PRODUCTION_AUDIO.md) — Phase 3.2: cantest → production audiomodule.
- [TEST_PRODUCTION_MAIN](TEST_PRODUCTION_MAIN.md) — Phase 3.3: production main → cantest audio sim.
- [TEST_DISCOVERY_FIXED](TEST_DISCOVERY_FIXED.md) — Debugging harness for discovery on two ports.
- [SIMPLE_DISCOVERY_TEST](SIMPLE_DISCOVERY_TEST.md) — Minimal “do we see 0x411?” check.
- [TEST_LOOPBACK](TEST_LOOPBACK.md) — Two-device loopback-focused CAN debug.
- [TEST_LOOPBACK_SINGLE](TEST_LOOPBACK_SINGLE.md) — Single-device loopback sanity check.

### WIP / placeholder

- [RECOVER_BUS](RECOVER_BUS.md) — Currently a status checker; mentions adding a CLI recovery command.
