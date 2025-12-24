# OTS Firmware Prompts (ots-fw-main)

This folder contains **AI prompt files** used when working on the firmware.

## Start Here

- **Main firmware prompt**: `FIRMWARE_MAIN_PROMPT.md`
- **Build & test prompt**: `BUILD_AND_TEST_PROMPT.md`
- **Protocol change prompt**: `PROTOCOL_CHANGE_PROMPT.md`
- **Debug & recovery prompt**: `DEBUGGING_AND_RECOVERY_PROMPT.md`
- **Module development prompt**: `MODULE_DEVELOPMENT_PROMPT.md`

## Existing Module Prompts

- `MAIN_POWER_MODULE_PROMPT.md`
- `NUKE_MODULE_PROMPT.md`
- `ALERT_MODULE_PROMPT.md`
- `TROOPS_MODULE_PROMPT.md`

## Reference Docs (not prompts)

These are firmware docs under `docs/`, referenced by prompts above:

- `docs/TESTING.md` – how to run the `test-*` PlatformIO environments
- `docs/WSS_TEST_GUIDE.md` – WSS/WebSocket test notes
- `docs/OTA_GUIDE.md` – OTA update flow
- `docs/ERROR_RECOVERY.md` – how to recover from boot loops / bad flashes
- `docs/RGB_LED_STATUS.md` – status LED behavior and mappings
- `docs/NAMING_CONVENTIONS.md` – naming rules for modules/files

For a quick human overview and basic commands, start at `../README.md`.

## How to Add a New Prompt

- Keep prompts **task-oriented** (what to do next, what to avoid), not a full spec.
- Prefer **links to existing docs** over duplicating the same instructions.
- If a change impacts protocol, start at the repo root `prompts/protocol-context.md`.
