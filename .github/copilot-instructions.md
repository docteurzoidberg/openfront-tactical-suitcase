# GitHub Copilot Instructions for OTS Project

## Project Overview

OpenFront Tactical Suitcase (OTS) is a multi-component system bridging OpenFront.io gameplay with physical hardware:
- **ots-server**: Nuxt 4 + Vue 3 dashboard with Nitro WebSocket server
- **ots-userscript**: TypeScript Tampermonkey script polling game state every 100ms
- **ots-fw-main**: ESP32-S3 firmware (PlatformIO/ESP-IDF) with hardware modules
- **ots-shared**: Shared TypeScript protocol types
- **ots-hardware**: Hardware module specifications

## Architecture & Protocol

### Single Source of Truth: `/protocol-context.md`

**ALL protocol changes MUST start here.** This 1000+ line document defines WebSocket message envelopes, event types, and data structures for all components.

**Protocol change workflow:**
1. Update `/protocol-context.md` first
2. Update TypeScript: `ots-shared/src/game.ts` 
3. Update C firmware: `ots-fw-main/include/protocol.h`
4. Update implementations in server/userscript/firmware
5. Never duplicate type definitions - always import from `ots-shared`

### Event-Driven Architecture

The system uses WebSocket events with this flow:
```
OpenFront.io Game → Userscript (100ms poll) → ots-server → Dashboard UI
                                            ↓
                                         Firmware (future)
```

**Key event types:** `NUKE_LAUNCHED`, `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`, `NUKE_EXPLODED`, `NUKE_INTERCEPTED`

### Nuke Tracking System

Critical implementation detail: Nukes are tracked by `unitID` (not timeouts):
- Firmware tracks up to 32 simultaneous nukes in `nuke_tracker.c`
- LEDs stay ON until ALL nukes of that type resolve (explode/intercept)
- Alert module tracks incoming (NUKE_DIR_INCOMING), nuke module tracks outgoing (NUKE_DIR_OUTGOING)
- Events MUST include `nukeUnitID` in data payload for proper tracking

## Component-Specific Conventions

### ots-server (Nuxt 4)

**Project modes:**
- **Emulator mode**: Dashboard simulates hardware (development without physical device)
- **Client mode** (future): Connects to real firmware WebSocket server

**Build/Dev:**
```bash
cd ots-server
npm install
npm run dev     # Port 3000
npm run build   # Production build
```

**WebSocket handlers** (`server/routes/`):
- `ws-script.ts`: Accepts userscript connections, broadcasts to "ui" channel
- `ws-ui.ts`: Accepts dashboard connections, subscribes to "ui" channel
- Use `defineWebSocketHandler` with Nitro's experimental WebSocket support
- Store latest GameState in module-level variable for initial sync
- Future: Add `ws-hardware.ts` for firmware client connections

**Vue patterns:**
- Use `<script setup lang="ts">` exclusively
- Import types from `ots-shared`: `import type { GameState, GameEvent } from '../../ots-shared/src/game'`
- Hardware components live in `app/components/hardware/` matching physical modules
- Use nuxt-ui components (includes Tailwind CSS)
- State management: Local component state or composables (no Vuex/Pinia unless requested)

### ots-userscript

**Build:**
```bash
cd ots-userscript
npm install
npm run build  # Output: build/userscript.ots.user.js
npm run watch  # Auto-rebuild on changes
```

**Game polling architecture:**
- Main loop runs every 100ms in `setInterval`
- Trackers: `NukeTracker`, `BoatTracker`, `LandTracker` detect game state changes
- Each tracker maintains `Map<unitID, TrackedEntity>` for deduplication
- Events emitted only on state transitions (launch detected, arrival/explosion detected)
- Always include `nukeUnitID` in launch events for firmware tracking

### ots-fw-main (ESP32-S3 Firmware)

**Build (PlatformIO - preferred):**
```bash
cd ots-fw-main
pio run              # Build
pio run -t upload    # Flash device
pio device monitor   # Serial output
```

**Module architecture:**
- All modules implement `hardware_module_t` interface: `init`, `update`, `handle_event`, `get_status`, `shutdown`
- Event dispatcher routes events to all registered modules
- I/O via MCP23017 I2C expanders: Board 0 (0x20) = inputs, Board 1 (0x21) = outputs
- Register new modules in `main.c` via `module_manager_register()`

**Nuke tracking:**
- `nuke_tracker.c` manages state for up to 32 nukes by unitID
- `nuke_module.c` handles outgoing nukes (button LEDs)
- `alert_module.c` handles incoming nukes (alert LEDs)
- LEDs stay ON while `nuke_tracker_get_active_count() > 0`

**Adding source files:**
Must update `src/CMakeLists.txt` SRCS list when adding `.c` files.

## Critical Workflows

### Adding New Event Types

1. Define in `/protocol-context.md` with JSON examples
2. Add to TypeScript enum in `ots-shared/src/game.ts`
3. Add to C enum in `ots-fw-main/include/protocol.h`
4. Add string conversion in `ots-fw-main/src/protocol.c`
5. Add handlers in relevant hardware modules
6. Update userscript trackers if game-state-driven

### Hardware Module Development

1. Create `module_name.h` in `ots-fw-main/include/` with pin definitions
2. Create `module_name.c` in `ots-fw-main/src/` implementing `hardware_module_t`
3. Add to `src/CMakeLists.txt` SRCS list
4. Register in `main.c`: `module_manager_register(&module_name)`
5. Create prompt file in `prompts/MODULE_NAME_PROMPT.md`
6. Update `copilot-project-context.md` in firmware directory

### Protocol Evolution

**Before making breaking changes:**
- Verify impact across all 3 implementations (shared/server/firmware)
- Update protocol-context.md version/changelog section
- Consider backward compatibility for OTA firmware updates
- Test WebSocket message flow end-to-end

## Repository Structure

```
ots/
  protocol-context.md              # SOURCE OF TRUTH for WebSocket messages
  .github/
    copilot-instructions.md        # This file - workspace-level guidance
  
  ots-hardware/                    # Hardware specs & module designs
    copilot-project-context.md     # Hardware-specific context
    hardware-spec.md               # Controller & bus specification
    modules/                       # Individual module specifications
  
  ots-shared/                      # Shared TypeScript types
    src/
      game.ts                      # TypeScript protocol implementation
      index.ts
  
  ots-server/                      # Nuxt 4 dashboard + WebSocket server
    copilot-project-context.md     # Server-specific context
    app/
      pages/index.vue              # Main dashboard
      components/hardware/         # Hardware module UI components
      composables/useGameSocket.ts # Dashboard WebSocket client
    server/routes/
      ws-script.ts                 # Userscript WebSocket handler
      ws-ui.ts                     # Dashboard WebSocket handler
  
  ots-userscript/                  # Tampermonkey userscript
    copilot-project-context.md     # Userscript-specific context
    src/main.user.ts               # Entry point
    build/userscript.ots.user.js   # Built output
  
  ots-fw-main/                     # ESP32-S3 firmware
    copilot-project-context.md     # Firmware-specific context
    platformio.ini
    include/
      protocol.h                   # C protocol implementation
      *_module.h                   # Hardware module headers
    src/
      main.c
      *_module.c                   # Hardware module implementations
      nuke_tracker.c               # Nuke state tracking
```

**No root-level package.json** - each subproject manages dependencies independently.

## Key Files & Patterns

- **Protocol**: `/protocol-context.md` (1155 lines, complete specification)
- **Component context**: Per-component `copilot-project-context.md` for detailed implementation guides
- **Shared types**: `ots-shared/src/game.ts` (TypeScript protocol implementation)
- **Firmware protocol**: `ots-fw-main/include/protocol.h` (C implementation)
- **WebSocket handlers**: `ots-server/server/routes/ws-*.ts`
- **Userscript bridge**: `ots-userscript/src/game/openfront-bridge.ts`
- **Hardware specs**: `ots-hardware/hardware-spec.md`, `ots-hardware/modules/*.md`

## Project-Specific Anti-Patterns

❌ **Don't** redefine protocol types - always import from `ots-shared`  
❌ **Don't** use timer-based LED control - use state-based tracking with unitIDs  
❌ **Don't** modify protocol without updating all 3 implementations  
❌ **Don't** add firmware `.c` files without updating CMakeLists.txt  
❌ **Don't** use generic `data?: unknown` - specify exact data shape in protocol-context.md

## Technologies & Conventions

- **Language**: TypeScript everywhere (server/userscript), C for firmware
- **UI Framework**: Vue 3 with `<script setup lang="ts">` syntax
- **Styling**: nuxt-ui (includes Tailwind CSS)
- **State Management**: Local component state or composables (no Vuex/Pinia unless requested)
- **WebSocket Client**: `@vueuse/core`'s `useWebSocket` for dashboard
- **WebSocket Server**: Nitro's `defineWebSocketHandler` with pub/sub channels
- **Typing**: Prefer strongly typed structures, avoid `any` whenever possible
- **Type Imports**: Always import from `ots-shared`, never redefine protocol types

## Testing & Debugging

**Server:** `npm run dev` in `ots-server/`, check http://localhost:3000  
**Userscript:** Install from `ots-userscript/build/`, check browser console (opens on Tampermonkey install)  
**Firmware:** `pio device monitor` for serial logs, check LED states physically  
**WebSocket:** Use browser DevTools Network tab (WS filter) to inspect frames  
**Protocol sync:** Grep for event type across all repos to verify consistency

## Component-Specific Context

For detailed implementation guidance when working within specific components, refer to:

### Project Context Files
- **Hardware design**: [ots-hardware/copilot-project-context.md](../ots-hardware/copilot-project-context.md)
- **Server/dashboard**: [ots-server/copilot-project-context.md](../ots-server/copilot-project-context.md)
- **Userscript**: [ots-userscript/copilot-project-context.md](../ots-userscript/copilot-project-context.md)
- **Firmware**: [ots-fw-main/copilot-project-context.md](../ots-fw-main/copilot-project-context.md)

### Firmware Module Prompts
Individual hardware module development guides (used when creating/modifying firmware modules):
- **Alert Module**: [ots-fw-main/prompts/ALERT_MODULE_PROMPT.md](../ots-fw-main/prompts/ALERT_MODULE_PROMPT.md)
- **Nuke Module**: [ots-fw-main/prompts/NUKE_MODULE_PROMPT.md](../ots-fw-main/prompts/NUKE_MODULE_PROMPT.md)
- **Main Power Module**: [ots-fw-main/prompts/MAIN_POWER_MODULE_PROMPT.md](../ots-fw-main/prompts/MAIN_POWER_MODULE_PROMPT.md)
- **Troops Module**: [ots-fw-main/prompts/TROOPS_MODULE_PROMPT.md](../ots-fw-main/prompts/TROOPS_MODULE_PROMPT.md)
