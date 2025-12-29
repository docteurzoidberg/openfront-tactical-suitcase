# GitHub Copilot Instructions for OTS Project

## Project Overview

OpenFront Tactical Suitcase (OTS) is a multi-component system bridging OpenFront.io gameplay with physical hardware:
- **ots-server**: Nuxt 4 + Vue 3 dashboard with Nitro WebSocket server
- **ots-userscript**: TypeScript Tampermonkey script polling game state every 100ms
- **ots-fw-main**: ESP32-S3 firmware (PlatformIO/ESP-IDF) with hardware modules
- **ots-shared**: Shared TypeScript protocol types
- **ots-hardware**: Hardware module specifications

## Architecture & Protocol

### Single Source of Truth: `/prompts/protocol-context.md`

**ALL protocol changes MUST start here.** This 1000+ line document defines WebSocket message envelopes, event types, and data structures for all components.

**Protocol change workflow:**
1. Update `/prompts/protocol-context.md` first
2. Update TypeScript: `ots-shared/src/game.ts` 
3. Update C firmware: `ots-fw-main/include/protocol.h`
4. Update implementations in server/userscript/firmware
5. Never duplicate type definitions - always import from `ots-shared`

### Event-Driven Architecture

The system uses WebSocket events with this flow:
```
OpenFront.io Game → Userscript (100ms poll) → Firmware WS Server → Dashboard UI
                                            ↑ (connects to)
```

**Critical: Firmware is WebSocket SERVER, not client**
- Userscript and dashboard are WebSocket **clients** connecting TO firmware
- Firmware runs HTTP/HTTPS server on port 3000 with `/ws` endpoint
- WSS (TLS) mode required for userscript on HTTPS pages (self-signed cert)

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
- **Single WebSocket endpoint** (`/ws`): All connections (UI, userscript, future firmware) use same endpoint
- Clients identify via handshake message with `clientType` field

**Build/Dev:**
```bash
cd ots-server
npm install
npm run dev     # Port 3000
npm run build   # Production build
```

**WebSocket architecture** (`server/routes/`):
- Single endpoint `/ws` for all connection types (unified architecture)
- Handshake-based client identification (`clientType: "ui" | "userscript" | "firmware"`)
- Broadcast to all connected peers via `broadcast` channel
- Use `defineWebSocketHandler` with Nitro's experimental WebSocket support
- Store latest GameState in module-level variable for initial sync

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

**Architecture:**
- **Firmware acts as WebSocket SERVER** (not client) - userscript/dashboard connect TO firmware
- **WSS support**: TLS-enabled WebSocket server (port 3000) for HTTPS page compatibility
- Configure via `WS_USE_TLS` in `include/config.h` (WSS=1, WS=0)
- Self-signed certificate in `certs/` (browsers show security warning)

**Module system:**
- All modules implement `hardware_module_t` interface: `init`, `update`, `handle_event`, `get_status`, `shutdown`
- Event dispatcher routes `internal_event_t` events to all registered modules
- I/O via MCP23017 I2C expanders: Board 0 (0x20) = inputs, Board 1 (0x21) = outputs
- Register new modules in `main.c` via `module_manager_register()`

**Event system:**
- **Game events** (`GAME_EVENT_*`): From protocol (nuke launches, alerts, etc.)
- **Internal events** (`INTERNAL_EVENT_*`): System status (network, WebSocket, buttons)
- Event dispatcher posts to FreeRTOS queue, modules handle via `handle_event()` callback

**Hardware testing:**
- **Test environments**: `test-i2c`, `test-outputs`, `test-inputs`, `test-adc`, `test-lcd`, `test-websocket`
- Each test is standalone firmware (no menus) - flash, watch serial, verify hardware
- Example: `pio run -e test-i2c -t upload && pio device monitor`
- Automation tools in `tools/tests/` (Python scripts for WebSocket testing)

**Nuke tracking:**
- `nuke_tracker.c` manages state for up to 32 nukes by unitID
- `nuke_module.c` handles outgoing nukes (button LEDs)
- `alert_module.c` handles incoming nukes (alert LEDs)
- LEDs stay ON while `nuke_tracker_get_active_count() > 0`

**Adding source files:**
Must update `src/CMakeLists.txt` SRCS list when adding `.c` files.

**Build warnings to ignore:**
- `esp_idf_size: error: unrecognized arguments: --ng` - non-blocking, ignore unless debugging size tools

## Critical Workflows

### Adding New Event Types

1. Define in `/prompts/protocol-context.md` with JSON examples
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
- Update prompts/protocol-context.md version/changelog section
- Consider backward compatibility for OTA firmware updates
- Test WebSocket message flow end-to-end

### Release Management

**Creating releases** (see `/prompts/RELEASE.md` for full guide):

1. Use `./release.sh` at repo root for unified versioning
2. Tag format: `YYYY-MM-DD.N` (date-based with auto-increment)
3. Single tag applies to all projects (userscript, firmware, server)
4. Release script handles: version updates → builds → git commit → tag
5. Update `weekly_announces.md` for Discord changelog

**Quick commands:**
```bash
./release.sh -u -p -m "Release description"  # Full automated release
./release.sh -l                               # List existing releases
./release.sh -u -m "Fix" userscript          # Userscript-only release
```

**Version locations:**
- Userscript: `package.json`, `src/main.user.ts` (VERSION constant)
- Firmware: `include/config.h` (OTS_FIRMWARE_VERSION macro)
- Server: `package.json` (displayed in dashboard header)

## Repository Structure

```
ots/
  release.sh                       # Automated release script
  weekly_announces.md              # Discord changelog history
  prompts/
    protocol-context.md            # SOURCE OF TRUTH for WebSocket messages
    RELEASE.md                     # Release process and version management guide
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

- **Protocol**: `/prompts/protocol-context.md` (1155 lines, complete specification)
- **Release Process**: `/prompts/RELEASE.md` (Complete guide to version management and tagging)
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
❌ **Don't** use generic `data?: unknown` - specify exact data shape in prompts/protocol-context.md

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
**Firmware tests:** Use `pio run -e test-<name> -t upload` for standalone hardware tests (see `docs/TESTING.md`)  
**WebSocket debugging:** 
- Browser DevTools Network tab (WS filter) for client-side inspection
- Firmware serial logs show connection events (`INTERNAL_EVENT_WS_*`)
- Python test scripts in `ots-fw-main/tools/tests/` for automated validation
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
