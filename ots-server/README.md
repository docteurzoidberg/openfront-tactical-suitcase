# OTS Server

Dashboard and WebSocket server for the Openfront Tactical Suitcase project.

## Overview

This Nuxt 4 application provides:

- **Single WebSocket endpoint** (`/ws`) for both userscript and UI communication
- **Hardware module emulation** - Vue components that simulate physical hardware modules
- **Event broadcast** - All messages are broadcast to all connected peers
- **Development environment** for testing hardware modules without physical device

## Architecture

- `/ws` - Single WebSocket endpoint for all connections (UI and userscript)
- `app/components/hardware/` - Hardware module emulators (mirrors `/ots-hardware/` specs)
- `app/composables/useGameSocket.ts` - WebSocket client and hardware state management

**Note**: This server does NOT track or store game state. It only relays events and commands between userscript and UI, and emulates hardware module behavior.

## Setup

Make sure to install dependencies:

```bash
# npm
npm install

# pnpm
pnpm install

# yarn
yarn install

# bun
bun install
```

## Development Server

Start the development server on `http://localhost:3000`:

```bash
# npm
npm run dev

# pnpm
pnpm dev

# yarn
yarn dev

# bun
bun run dev
```

## Hardware Modules

Currently implemented modules:

### Main Power Module
- POWER LED (always on when powered)
- LINK LED (shows userscript connection status)
- Component: `app/components/hardware/MainPowerModule.vue`
- Spec: `/ots-hardware/modules/main-power-module.md`

### Alert Module
- WARNING LED (active when any threat detected)
- 5 threat LEDs: ATOM, HYDRO, MIRV, LAND, NAVAL
- Receives alert events and blinks LEDs for 10s (nukes) or 15s (invasions)
- Component: `app/components/hardware/AlertModule.vue`
- Spec: `/ots-hardware/modules/alert-module.md`

### Nuke Control Panel
- Three buttons: Atom, Hydro, MIRV
- Sends `send-nuke` commands to userscript
- Receives `NUKE_LAUNCHED`/`HYDRO_LAUNCHED`/`MIRV_LAUNCHED` events and blinks LEDs for 4 seconds
- Component: `app/components/hardware/NukeModule.vue`
- Spec: `/ots-hardware/modules/nuke-module.md`

## Event Types

The dashboard displays all events from the game:

**Alert Events** (trigger hardware LEDs):
- `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV` - Incoming nuke threats (10s duration)
- `ALERT_LAND`, `ALERT_NAVAL` - Incoming invasions (15s duration)

**Launch Events** (from dashboard or hardware buttons):
- `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED` - Nuke fired

**Outcome Events** (informational):
- `NUKE_EXPLODED` - Tracked nuke reached target
- `NUKE_INTERCEPTED` - Tracked nuke destroyed before impact

**Game Lifecycle**:
- `GAME_START`, `GAME_END`, `WIN`, `LOOSE`

**Info Events**:
- `INFO` - General logging and debug information
- `HARDWARE_TEST` - Hardware diagnostics

## Protocol

All WebSocket messages follow the protocol defined in `/protocol-context.md`.

See `ots-shared/src/game.ts` for TypeScript type definitions.

## Related Projects

- `/ots-userscript` - Tampermonkey script that bridges the game
- `/ots-fw-main` - ESP32-S3 firmware for physical hardware
- `/ots-hardware` - Hardware module specifications
- `/ots-shared` - Shared TypeScript types

## Production

Build the application for production:

```bash
# npm
npm run build

# pnpm
pnpm build

# yarn
yarn build

# bun
bun run build
```

Locally preview production build:

```bash
# npm
npm run preview

# pnpm
pnpm preview

# yarn
yarn preview

# bun
bun run preview
```

Check out the [deployment documentation](https://nuxt.com/docs/getting-started/deployment) for more information.
