# OTS Userscript - Source Code

TypeScript userscript that bridges OpenFront.io gameplay with the OTS hardware controller.

## Directory Structure

```
src/
├── game/              Game integration & event detection
│   ├── trackers/      Entity trackers (nukes, boats, land attacks)
│   ├── constants.ts   Configuration constants
│   ├── game-api.ts    Safe game state wrapper
│   ├── openfront-bridge.ts  Main game bridge
│   └── troop-monitor.ts     Troop count monitoring
│
├── hud/               User interface components
│   └── sidebar/       Sidebar HUD implementation
│       ├── tabs/      Tab components (logs, hardware, sound)
│       ├── types/     Type re-exports from root
│       ├── utils/     Log formatting and filtering
│       └── *.ts       Panel and manager components
│
├── storage/           Persistent storage management
│   ├── keys.ts        Storage key constants
│   └── config.ts      Configuration helpers
│
├── types/             TypeScript type definitions
│   ├── position-types.ts  HUD positioning types
│   ├── log-types.ts       Logging types
│   └── greasemonkey.d.ts  GM API declarations
│
├── utils/             Utility functions
│   └── dom.ts         DOM helpers
│
├── websocket/         WebSocket client
│   └── client.ts      Connection to ots-server/firmware
│
└── main.user.ts       Entry point
```

## Module Overview

### Game Integration (`game/`)

Monitors OpenFront.io game state and emits protocol events:
- **Trackers**: Event-driven entity monitoring (see `trackers/README.md`)
- **GameAPI**: Safe wrapper preventing crashes from game updates
- **GameBridge**: Orchestrates all trackers and lifecycle
- **TroopMonitor**: Tracks troop counts and attack ratio changes

### HUD (`hud/`)

Draggable, resizable sidebar interface:
- **3 Tabs**: Logs, Hardware diagnostics, Sound controls
- **Position management**: Snap-to-edges with arrow indicators
- **Filter panel**: Advanced log filtering by direction and event type
- **Settings panel**: WebSocket URL configuration

### WebSocket (`websocket/`)

Connects to:
- **ots-server** (development) - Nuxt dashboard with emulator
- **ots-fw-main** (production) - ESP32 firmware on physical device

Features:
- Auto-reconnect with exponential backoff
- Handshake-based client identification
- Protocol-compliant message serialization

## Key Files

| File | Lines | Purpose |
|------|-------|---------|
| `game/openfront-bridge.ts` | 563 | Main game integration |
| `hud/sidebar/position-manager.ts` | 517 | HUD positioning logic |
| `game/game-api.ts` | 397 | Safe game state access |
| `hud/sidebar-hud.ts` | 392 | Main HUD controller |
| `websocket/client.ts` | 255 | WebSocket connection |
| `game/trackers/nuke-tracker.ts` | 253 | Nuclear weapon tracking |
| `hud/sidebar/tabs/logs-tab.ts` | 217 | Logs display & filtering |

## Development

**Build:**
```bash
npm run build
```

**Watch mode:**
```bash
npm run watch
```

**Output:**
- `build/userscript.ots.user.js` - Tampermonkey-ready script

## Architecture Principles

1. **Event-Driven**: Game changes trigger protocol events
2. **Polling-Based**: 100ms game state polling for real-time detection
3. **Type-Safe**: Shared types from `ots-shared` package
4. **Modular**: Clear separation of concerns (game/hud/websocket)
5. **Resilient**: Safe wrappers prevent crashes from game updates

## Related Documentation

- **Protocol**: `/prompts/protocol-context.md` - WebSocket message spec
- **Project Context**: `../copilot-project-context.md` - AI assistant guide
- **Tracker Details**: `game/trackers/README.md` - Entity tracking details
