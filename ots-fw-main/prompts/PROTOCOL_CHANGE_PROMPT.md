# Protocol Change Prompt (ots-fw-main)

Any protocol change must stay consistent across:

1. Repo root spec: `../../prompts/WEBSOCKET_MESSAGE_SPEC.md` (single source of truth)
2. Shared TS types: `../../ots-shared/src/game.ts`
3. Firmware C types: `include/protocol.h` and `src/protocol.c`
4. Implementations: server/userscript/firmware

## Workflow (Do This In Order)

1. Update `../../prompts/WEBSOCKET_MESSAGE_SPEC.md`
   - Add/modify message envelope and event/command payloads
   - Include JSON examples
2. Update `../../ots-shared/src/game.ts`
   - Keep enums/data types strongly typed
3. Update firmware:
   - Add enum values in `include/protocol.h`
   - Update string conversions in `src/protocol.c`
   - Update parsing/dispatch in `src/ws_protocol.c` and any module handlers
4. Update affected modules
   - Ensure nukes use **unitID-based** tracking where needed
5. Validate with the smallest relevant `test-*` env

## Anti-Patterns

- Don’t add a new event type only in one place.
- Don’t ship timer-only LED control for nuke tracking; use unitIDs and state.
- Don’t use `data?: unknown` in TypeScript for new messages when the shape is known.
