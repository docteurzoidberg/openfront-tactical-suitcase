# Module Development Prompt (ots-fw-main)

Use this when adding or significantly changing a firmware hardware module.

## Rules

- Each module must implement the `hardware_module_t` interface.
- Register new modules in `src/main.c` via the module manager.
- If you add new `.c` files, ensure they are included in `src/CMakeLists.txt`.
- Do not duplicate protocol types; use `include/protocol.h`.

## Preferred Approach

1. Confirm the hardware behavior is defined in the hardware specs (repo root `ots-hardware/`).
2. Confirm any new messages/events are defined in `../../prompts/protocol-context.md`.
3. Implement the module with minimal coupling:
   - init/update/handle_event/get_status/shutdown
4. Add/adjust a module prompt in this folder documenting:
   - Pin mapping and behavior
   - Event/command integration
   - Timing/state rules

## Existing Module Prompts

- Nuke: `NUKE_MODULE_PROMPT.md`
- Alert: `ALERT_MODULE_PROMPT.md`
- Main power: `MAIN_POWER_MODULE_PROMPT.md`
- Troops: `TROOPS_MODULE_PROMPT.md`

## Related Firmware Docs

- `docs/NAMING_CONVENTIONS.md`
- `docs/LCD_DISPLAY_MODES.md`
- `docs/TESTING.md`
