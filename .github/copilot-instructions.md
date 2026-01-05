# GitHub Copilot Instructions for OTS Project

## Project Overview

OpenFront Tactical Suitcase (OTS) is a multi-component system bridging OpenFront.io gameplay with physical hardware:
- **ots-simulator**: Nuxt 4 + Vue 3 dashboard with Nitro WebSocket simulator
- **ots-userscript**: TypeScript Tampermonkey script polling game state every 100ms
- **ots-fw-main**: ESP32-S3 firmware (PlatformIO/ESP-IDF) with hardware modules
- **ots-fw-audiomodule**: ESP32-A1S audio playback module with CAN bus integration
- **ots-fw-cantest**: ESP32-S3 CAN bus testing/debugging tool (⚠️ WIP/Untested)
- **ots-fw-shared**: Shared ESP-IDF components (CAN driver, discovery, audio protocol)
- **ots-shared**: Shared TypeScript protocol types
- **ots-hardware**: Hardware module specifications
- **ots-website**: VitePress documentation site (auto-deployed to GitHub Pages)

## Architecture & Protocol

### Single Source of Truth: `/prompts/WEBSOCKET_MESSAGE_SPEC.md`

**ALL protocol changes MUST start here.** This 1000+ line document defines WebSocket message envelopes, event types, and data structures for all components.

**Protocol change workflow:**
1. Update `/prompts/WEBSOCKET_MESSAGE_SPEC.md` first
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
- Firmware tracks up to 32 simultaneous nukes in `nuke_state_manager.c`
- LEDs stay ON until ALL nukes of that type resolve (explode/intercept)
- Alert module tracks incoming (NUKE_DIR_INCOMING), nuke module tracks outgoing (NUKE_DIR_OUTGOING)
- Events MUST include `nukeUnitID` in data payload for proper tracking

## Component-Specific Conventions

### ots-simulator (Nuxt 4)

**Project modes:**
- **Emulator mode**: Dashboard simulates hardware (development without physical device)
- **Single WebSocket endpoint** (`/ws`): All connections (UI, userscript, future firmware) use same endpoint
- Clients identify via handshake message with `clientType` field

**Build/Dev:**
```bash
cd ots-simulator
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
pio run -e esp32-s3-dev              # Build
pio run -e esp32-s3-dev -t upload    # Flash device
pio device monitor                   # Serial output
```

**Architecture:**
- **Firmware acts as WebSocket SERVER** (not client) - userscript/dashboard connect TO firmware
- **WSS support**: TLS-enabled WebSocket server (port 3000) for HTTPS page compatibility
- Configure via `WS_USE_TLS` in `include/config.h` (WSS=1, WS=0)
- Uses embedded self-signed certificate (browsers show security warning)

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
- `nuke_state_manager.c` manages state for up to 32 nukes by unitID
- `nuke_module.c` handles outgoing nukes (button LEDs)
- `alert_module.c` handles incoming nukes (alert LEDs)
- LEDs stay ON while `nuke_tracker_get_active_count() > 0`

**Adding source files:**
Must update `src/CMakeLists.txt` SRCS list when adding `.c` files.

**Build warnings to ignore:**
- `esp_idf_size: error: unrecognized arguments: --ng` - non-blocking, ignore unless debugging size tools

### ots-fw-audiomodule (ESP32-A1S Audio Module)

**Build (PlatformIO):**
```bash
cd ots-fw-audiomodule
pio run -e esp32-a1s-espidf      # Build
pio run -e esp32-a1s-espidf -t upload  # Flash
pio device monitor               # Serial output
```

**Architecture:**
- Audio playback on ESP32-A1S AudioKit (ES8388 codec)
- CAN bus integration for sound commands from main controller
- SD card support for custom sound banks
- Embedded WAV files for testing
- Uses shared components: can_driver, can_audiomodule, can_discovery

**Sound System:**
- Audio mixer with up to 8 concurrent sounds
- Queue management for sound commands
- Volume control and fade effects
- WAV file support (embedded + SD card)
- Test tone generator

**CAN Protocol:**
- Responds to MODULE_QUERY with announcements
- Handles PLAY_SOUND, STOP_SOUND, STOP_ALL commands
- Sends ACK/NACK responses with status codes
- Auto-sends SOUND_FINISHED when playback completes

### ots-fw-cantest (CAN Bus Testing Tool)

**Build (PlatformIO):**
```bash
cd ots-fw-cantest
pio run -e esp32-s3-devkit       # Build
pio run -e esp32-s3-devkit -t upload  # Flash
pio device monitor               # Serial + interactive CLI
```

**Purpose:**
- Validate CAN bus communications between main controller and modules
- Debug protocol messages and timing
- Simulate audio module or controller for testing

**Operating Modes:**
- **Monitor Mode** (m): Passive bus sniffer with timestamps and statistics
- **Audio Module Simulator** (a): Auto-responds to discovery and sound commands
- **Controller Simulator** (c): Manual command sending (discovery, play, stop)
- **Interactive CLI**: Single-letter commands, help (h), real-time feedback

**Features:**
- Protocol decoder: Human-readable message parsing with raw hex
- Statistics tracking: RX/TX/error counts, message rates
- Uses shared components: can_driver, can_discovery
- Lightweight: 242KB flash, 13KB RAM

### ots-fw-shared (Shared ESP-IDF Components)

**Components:**
- **can_driver**: Generic CAN bus (TWAI) driver with mock fallback
- **can_discovery**: Module discovery protocol (MODULE_ANNOUNCE, MODULE_QUERY)
- **can_audiomodule**: Audio module CAN protocol implementation

**CAN Bus Protocol:**
- **Specification**: [`/prompts/CANBUS_MESSAGE_SPEC.md`](../prompts/CANBUS_MESSAGE_SPEC.md) - Single source of truth for CAN message formats
- **Developer Guide**: [`/doc/developer/canbus-protocol.md`](../doc/developer/canbus-protocol.md) - Implementation patterns and code examples
- **Component APIs**: Component-level docs in `/ots-fw-shared/components/*/COMPONENT_PROMPT.md`

🤖 **AI Guidelines: CAN Protocol Changes**

When user requests CAN protocol changes (new messages, CAN IDs, or data fields):

1. **Update BOTH files in this order:**
   - First: `prompts/CANBUS_MESSAGE_SPEC.md` (add message definition with byte layout)
   - Second: `doc/developer/canbus-protocol.md` (add C implementation examples)

2. **Keep them synchronized:**
   - Spec shows WHAT messages look like (CAN ID, DLC, byte layout, data fields)
   - Dev doc shows HOW to implement in C (code examples, patterns, debugging)
   - Both must reflect the same CAN IDs and message formats

3. **Firmware-specific updates:**
   - Update shared components if protocol-level changes
   - Update module-specific handlers for behavior changes
   - Document hardware behavior in spec (LED states, timing)

4. **Verification checklist:**
   - [ ] Message exists in spec with byte layout diagram
   - [ ] Message has C implementation example in dev doc
   - [ ] Both files mention same CAN IDs
   - [ ] Hardware behavior documented in both files

**Usage:**
Referenced in CMakeLists.txt via `EXTRA_COMPONENT_DIRS`:
```cmake
list(APPEND EXTRA_COMPONENT_DIRS "../ots-fw-shared/components")
```

### ots-website (VitePress Documentation)

**Build:**
```bash
cd ots-website
npm install
npm run dev     # Port 5173
npm run build   # Production build
```

**Architecture:**
- **VitePress** static site generator with Vue 3
- **Source**: Syncs markdown files from `../doc/` folder
- **Custom pages**: Index, downloads, releases (in `ots-website/`)
- **Deployment**: Auto-deployed to GitHub Pages via GitHub Actions

**Content Management:**
- Documentation source: `/doc/` (user/, developer/)
- VitePress indexes: `/ots-website/user/index.md`, `/ots-website/developer/index.md`
- Custom pages: `/ots-website/downloads.md`, `/ots-website/releases.md`
- Sync script excludes custom index.md files from overwrite

**Deployment:**
- Workflow: `.github/workflows/deploy-docs.yml`
- Trigger: Push to `main` branch
- URL: `https://docteurzoidberg.github.io/openfront-tactical-suitcase/`
- Base path: `/openfront-tactical-suitcase/` (configured in VitePress)

## Critical Workflows

### Adding New Event Types

1. Define in `/prompts/WEBSOCKET_MESSAGE_SPEC.md` with JSON examples
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

### CAN Bus Development

**Testing CAN communications:**
1. Use `ots-fw-cantest` to validate protocol messages
2. Flash cantest to ESP32-S3 DevKit board
3. Run in monitor mode (m) to sniff bus traffic
4. Run in simulator mode (a/c) to test module interactions

**Adding CAN module support:**
1. Define protocol in `/prompts/CANBUS_MESSAGE_SPEC.md` and `/doc/developer/canbus-protocol.md`
2. Create shared component in `/ots-fw-shared/components/can_<module>/`
3. Update main controller to integrate new module
4. Test with cantest simulator before physical module

### Protocol Evolution

**Before making breaking changes:**
- Verify impact across all 3 implementations (shared/server/firmware)
- Update prompts/WEBSOCKET_MESSAGE_SPEC.md version/changelog section
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
    README.md                      # Prompts directory index
    WEBSOCKET_MESSAGE_SPEC.md      # SOURCE OF TRUTH for WebSocket messages
    CANBUS_MESSAGE_SPEC.md         # SOURCE OF TRUTH for CAN bus protocol
    DOCUMENTATION_GUIDELINES.md    # VitePress documentation standards for AI
    RELEASE.md                     # Release process and version management guide
    GIT_WORKFLOW.md                # Git branching, commits, and PR process
    PLATFORMIO_WORKFLOW.md         # PlatformIO build system guidelines for AI
    PROMPT_REVIEW_GUIDE.md         # Documentation review workflow system
    PROMPT_REVIEW_PLAN.md          # Progress tracking for documentation updates
  .github/
    copilot-instructions.md        # This file - workspace-level guidance
    workflows/
      deploy-docs.yml              # GitHub Pages deployment for ots-website
  
  ots-fw-shared/                   # Shared firmware components
    README.md                      # Shared components overview
    components/
      can_driver/                  # Generic CAN bus driver (used by all firmwares)
        COMPONENT_PROMPT.md        # CAN driver documentation
  
  ots-hardware/                    # Hardware specs & module designs
    copilot-project-context.md     # Hardware-specific context
    hardware-spec.md               # Controller & bus specification
    modules/                       # Individual module specifications
  
  ots-shared/                      # Shared TypeScript types
    src/
      game.ts                      # TypeScript protocol implementation
      index.ts
  
  ots-simulator/                      # Nuxt 4 dashboard + WebSocket server
    copilot-project-context.md     # Server-specific context
    app/
      pages/index.vue              # Main dashboard
      components/hardware/         # Hardware module UI components
      composables/useGameSocket.ts # Dashboard WebSocket client
    server/routes/
      ws.ts                        # Unified WebSocket handler (all clients)
  
  ots-userscript/                  # Tampermonkey userscript
    copilot-project-context.md     # Userscript-specific context
    src/main.user.ts               # Entry point
    build/userscript.ots.user.js   # Built output
  
  ots-website/                     # VitePress documentation site
    .vitepress/
      config.ts                    # VitePress configuration
    index.md                       # Homepage
    downloads.md                   # Downloads page (CAD, PCB, firmware)
    releases.md                    # Release history page
    user/
      index.md                     # User guide index (VitePress)
      [synced from ../doc/user/]  # Actual docs
    developer/
      index.md                     # Developer guide index (VitePress)
      [synced from ../doc/developer/]  # Actual docs
  
  doc/                             # Documentation source
    README.md                      # Documentation overview
    STRUCTURE.md                   # Repository structure guide
    user/                          # User guides (synced to ots-website)
    developer/                     # Developer guides (synced to ots-website)
  
  ots-fw-main/                     # ESP32-S3 firmware
    copilot-project-context.md     # Firmware-specific context
    platformio.ini
    include/
      protocol.h                   # C protocol implementation
      *_module.h                   # Hardware module headers
    src/
      main.c
      *_module.c                   # Hardware module implementations
      nuke_state_manager.c         # Nuke state tracking
    CMakeLists.txt         # Component registration
```

**No root-level package.json** - each subproject manages dependencies independently.

## Key Files & Patterns

- **Protocol**: `/prompts/WEBSOCKET_MESSAGE_SPEC.md` (WebSocket message specification - SOURCE OF TRUTH)
- **Protocol Guide**: `/doc/developer/websocket-protocol.md` (Developer implementation guide and patterns)
- **CAN Protocol**: `/prompts/CANBUS_MESSAGE_SPEC.md` (CAN bus message specification - SOURCE OF TRUTH)
- **CAN Guide**: `/doc/developer/canbus-protocol.md` (CAN implementation guide with C examples)
- **Release Process**: `/prompts/RELEASE.md` (Complete guide to version management and tagging)
- **Git Workflow**: `/prompts/GIT_WORKFLOW.md` (Git branching, commits, and PR process)
- **Documentation Standards**: `/prompts/DOCUMENTATION_GUIDELINES.md` (VitePress markdown and writing guidelines)
- **PlatformIO Guide**: `/prompts/PLATFORMIO_WORKFLOW.md` (Build system guidelines for AI)
- **Prompts Index**: `/prompts/README.md` (Complete index of all prompt files)
- **Shared Components**: `/ots-fw-shared/components/` (Firmware code shared across projects)
  - CAN driver: `/ots-fw-shared/components/can_driver/`
- **Component context**: Per-component `copilot-project-context.md` for detailed implementation guides
- **Shared types**: `ots-shared/src/game.ts` (TypeScript protocol implementation)
- **Firmware protocol**: `ots-fw-main/include/protocol.h` (C implementation)
- **WebSocket handlers**: `ots-simulator/server/routes/ws.ts`
- **Userscript bridge**: `ots-userscript/src/game/openfront-bridge.ts`
- **Hardware specs**: `ots-hardware/hardware-spec.md`, `ots-hardware/modules/*.md`

## Project-Specific Anti-Patterns

❌ **Don't** redefine protocol types - always import from `ots-shared`  
❌ **Don't** use timer-based LED control - use state-based tracking with unitIDs  
❌ **Don't** modify protocol without updating all 3 implementations  
❌ **Don't** add firmware `.c` files without updating CMakeLists.txt  
❌ **Don't** use generic `data?: unknown` - specify exact data shape in prompts/WEBSOCKET_MESSAGE_SPEC.md

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

**Simulator:** `npm run dev` in `ots-simulator/`, check http://localhost:3000  
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
- **Simulator/dashboard**: [ots-simulator/copilot-project-context.md](../ots-simulator/copilot-project-context.md)
- **Userscript**: [ots-userscript/copilot-project-context.md](../ots-userscript/copilot-project-context.md)
- **Firmware**: [ots-fw-main/copilot-project-context.md](../ots-fw-main/copilot-project-context.md)

### Firmware Module Prompts
Individual hardware module development guides (used when creating/modifying firmware modules):
- **Alert Module**: [ots-fw-main/prompts/ALERT_MODULE_PROMPT.md](../ots-fw-main/prompts/ALERT_MODULE_PROMPT.md)
- **Nuke Module**: [ots-fw-main/prompts/NUKE_MODULE_PROMPT.md](../ots-fw-main/prompts/NUKE_MODULE_PROMPT.md)
- **Main Power Module**: [ots-fw-main/prompts/MAIN_POWER_MODULE_PROMPT.md](../ots-fw-main/prompts/MAIN_POWER_MODULE_PROMPT.md)
- **Troops Module**: [ots-fw-main/prompts/TROOPS_MODULE_PROMPT.md](../ots-fw-main/prompts/TROOPS_MODULE_PROMPT.md)
