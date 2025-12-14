# Project Context – OTS Server, Userscript, Firmware, and Hardware

You are GitHub Copilot, assisting on a multi-project repository containing:

- `ots-server`: **Nuxt 4 + TypeScript** dashboard + Nitro WebSocket server (acts as both OTS client and hardware emulator).
- `ots-userscript`: **TypeScript Tampermonkey userscript** that bridges the OpenFront.io game page to the OTS backend.
- `ots-fw-main`: **PlatformIO-based ESP32-S3 firmware** for the OTS hardware controller (WebSocket server).
- `ots-shared`: Shared TypeScript types for protocol and game state.
- `ots-hardware`: Hardware specifications and module designs for the physical OTS device.

The high-level goals are:

- Build a **real-time dashboard UI** (Vue 3 + Nuxt 4 + TypeScript) for the OpenFront.io game.
- Stream **game state and events** from a userscript to the dashboard and hardware via WebSockets.
- Use a **single, shared WebSocket protocol** across all components, documented in `protocol-context.md` (the source of truth) and implemented in `ots-shared/src/game.ts`.
- Make `ots-server` operate in two modes:
  - **Client mode**: Connects to real hardware (firmware WebSocket server) to send game state
  - **Emulator mode**: Simulates hardware firmware for dev/debug without physical device

---

## High-level architecture

### Protocol Definition (Source of Truth)

**All protocol and message definitions are in `/protocol-context.md`**. This file defines:
- WebSocket message envelopes (`state`, `event`, `cmd`, `ack`)
- Core types (`GameState`, `GameEvent`, `Command`)
- Incoming/outgoing message formats
- Semantics for all components

TypeScript implementations live in `ots-shared/src/game.ts` and C++ implementations in `ots-fw-main/include/protocol.h`, but `/protocol-context.md` is the canonical specification.

### Hardware Specifications

**All hardware module specifications are in `/ots-hardware/`**. This directory defines:
- Main controller architecture (ESP32-S3)
- Module bus specification (I2C, CAN, power)
- Individual module designs and behaviors
- Game state domain mapping per module

See `/ots-hardware/copilot-project-context.md` for detailed hardware context.

### 1. Nuxt 4 app (UI + server)

- Framework: **Nuxt 4**, **Vue 3**, **TypeScript**.
- Nitro is used as the **HTTP server and WebSocket server**.
- The Nuxt app exposes:
  - WebSocket endpoint `/ws-script` for the **userscript**.
  - WebSocket endpoint `/ws-ui` for the **dashboard UI**.
  - (Future) WebSocket **client** to connect to hardware firmware server
- Nitro's **experimental websocket support** is enabled
```txt
repo root/
  protocol-context.md          # SOURCE OF TRUTH for all WebSocket message definitions
  copilot-project-context.md   # This file - overall project context
  
  ots-hardware/                # Hardware specifications and module designs
    copilot-project-context.md
    hardware-spec.md           # Main controller and bus spec
    module-template.md
    modules/                   # Individual module specs
      nuke-module.md

  ots-shared/                  # Shared TS-only types (implements protocol-context.md)
    src/
      game.ts                  # TypeScript implementation of protocol types
      index.ts

  ots-server/                  # Nuxt 4 app (dashboard + Nitro server)
    copilot-project-context.md # Detailed context for the server project
    nuxt.config.ts
    app/
      app.vue
      pages/
        index.vue              # Main dashboard page
      components/
        hardware/              # Vue components mirroring hardware modules
    composables/
      useGameSocket.ts         # WebSocket client for dashboard UI
    server/
      routes/
        ws-script.ts           # WebSocket handler for userscript
        ws-ui.ts               # WebSocket handler for dashboard UI
        api/state.get.ts       # HTTP endpoint to fetch current state

  ots-userscript/              # Standalone TS userscript project
    copilot-project-context.md
    package.json
    tsconfig.json              # Imports from ../ots-shared/src/game
    src/
      main.user.ts             # Tampermonkey entry
    build/
      userscript.ots.user.js   # Built userscript bundle

  ots-fw-main/                 # PlatformIO firmware project for ESP32-S3
    copilot-project-context.md
    platformio.ini
    include/
      protocol.h               # C++ implementation of protocol types
      config.h
    src/
      main.cpp
      protocol.cpp
```

There is **no root-level package.json**. Each subproject is managed independently:

- To run the Nuxt app:
  - `cd ots-server`
  - `npm install`
  - `npm run dev` (or `build`, `generate`, `preview`).
- To work on the userscript:
  - See `ots-userscript/copilot-project-context.md` for detailed behavior and build/install steps.
- To work on the firmware:
  - See `ots-fw-main/copilot-project-context.md` for firmware-specific details.
- To work on hardware specifications:
  - See `ots-hardware/copilot-project-context.md` for module design workflow.

**Important**: `/protocol-context.md` is the single source of truth for all message and event definitions. When evolving the protocol:
1. Update `/protocol-context.md` first
2. Then update implementations in `ots-shared/src/game.ts` (TypeScript) and `ots-fw-main/include/protocol.h` (C++)
3. Update server routes, userscript, and firmware code to match

Copilot should keep the protocol and shared types compatible across all components and avoid breaking changes when evolving code.

---

## Technologies and conventions

- **Language**: TypeScript everywhere.
- **UI**: Vue 3 with `<script setup lang="ts">`.
- **Styling**: use nuxt-ui (it alreadyincludes tailwindcss)
- **State management**: For now, use local component state or simple composables. No Vuex/Pinia unless explicitly requested.
- **WebSocket client**: Use `@vueuse/core`’s `useWebSocket` on the client side.
- **WebSocket server**: Use Nitro’s `defineWebSocketHandler` with channels (`peer.subscribe`, `peer.publish`).

Prefer **clear, strongly typed data structures** and avoid `any` whenever possible.

**Note**: Do NOT duplicate type definitions. Always reference `/protocol-context.md` as the source of truth for message formats, then import implementations from `ots-shared/src/game.ts`.

---

## Desired files and structure (within the Nuxt project)

**Important**: Type definitions shown below are for illustration. The actual definitions are in `/protocol-context.md` and implemented in `ots-shared/src/game.ts`. Always import from `ots-shared` rather than redefining types.

Copilot should help maintain or create the following structure.

```txt
root/
  nuxt.config.ts

  composables/
    useGameSocket.ts  # WebSocket client for the dashboard UI
  app/
    app.vue
    pages/
      index.vue       # Main dashboard page
    components/
      hardware/       # Vue components matching hardware modules
  server/
    routes/
      ws-script.ts    # WebSocket handler for userscript
      ws-ui.ts        # WebSocket handler for dashboard UI
      api/state.get.ts  # HTTP endpoint to fetch current state
```

### Type Imports

All server routes and composables should import types from `ots-shared`:

```ts
import type { GameState, GameEvent, IncomingMessage, OutgoingMessage } from '../../ots-shared/src/game'
```

**Do NOT redefine these types** - they are defined in `/protocol-context.md` and implemented in `ots-shared/src/game.ts`.

### Current Protocol Types

Key types as of latest protocol update:
- **GameEventType**: `INFO`, `WIN`, `LOOSE`, `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`, `NUKE_ALERT`, `HYDRO_ALERT`, `MIRV_ALERT`, `LAND_ALERT`, `NAVAL_ALERT`
- **GameState**: Contains `timestamp`, `mapName`, `mode`, `playerCount`, and `hwState` (hardware module states)
- **HWState**: Nested structure with `m_general`, `m_alert`, and `m_nuke` module states
- **GameEvent**: Has `type`, `timestamp`, optional `message`, optional `data`

---

## Server-side WebSocket handlers (Nitro)

Use `defineWebSocketHandler` from Nitro.

### `server/routes/ws-script.ts`

Responsibilities:

- Accept WebSocket connections from the **userscript**.
- Parse incoming JSON messages as `IncomingMessage`.
- Broadcast:
  - game events → to the `"ui"` channel
  - game state → to the `"ui"` channel + store the newest state in memory
- Optionally handle commands from UI (via channel) back to the userscript.

Implementation hints:

- Use a module-level variable to store the latest `GameState`.
- Use `peer.publish('ui', rawMessageText)` so that UI peers subscribed to `"ui"` receive the broadcast.
- Log basic connection info (`open`, `close`, `error`).

### `server/routes/ws-ui.ts`

Responsibilities:

- Accept WebSocket connections from the **dashboard UI**.
- Subscribe to channel `"ui"` to receive messages coming from `ws-script`.
- On new connection:
  - Optionally send the latest stored `GameState` as an initial message.
- Accept messages from UI:
  - Typically `OutgoingMessage` such as `{ type: 'cmd', ... }`.
  - Broadcast commands over a `"script"`
