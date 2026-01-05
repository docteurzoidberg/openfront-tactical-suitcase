# Firmware Main Prompt (ots-fw-main)

You are working on the **OTS ESP32-S3 firmware** in `ots-fw-main/`.

## Goals

- Keep the firmware stable under partial hardware availability (e.g., missing MCP23017 boards).
- Maintain strict protocol compatibility with the repo root `prompts/WEBSOCKET_MESSAGE_SPEC.md`.
- Prefer state/event-driven behavior (no timer-only LED logic that ignores unit IDs).
- Keep changes minimal and consistent with existing patterns.

## Critical Constraints

- **Protocol is single-source-of-truth**: update `../../prompts/WEBSOCKET_MESSAGE_SPEC.md` first for any message/event changes.
- **Nuke tracking is by unitID** (not timeouts): events must carry `nukeUnitID` where required.
- Treat optional hardware as **non-fatal** where practical so networking can still be tested.

## Where to Look

- Entry point: `src/main.c`
- Protocol enums/strings: `include/protocol.h`, `src/protocol.c`
- WebSocket: `src/ws_server.c`, `src/ws_protocol.c`
- Event system: `src/event_dispatcher.c`
- LED/status: `src/rgb_handler.c`, `docs/RGB_LED_STATUS.md`
- OTA: `src/ota_manager.c`, `docs/OTA_GUIDE.md`
- Test builds: `platformio.ini`, `src/tests/*.c`, `docs/TESTING.md`

## Common Workflows

- Build & run: see `BUILD_AND_TEST_PROMPT.md`
- Adding/changing protocol: see `PROTOCOL_CHANGE_PROMPT.md`
- Debugging a boot/runtime regression: see `DEBUGGING_AND_RECOVERY_PROMPT.md`
- Adding a hardware module: see `MODULE_DEVELOPMENT_PROMPT.md` and the module prompts in this folder
