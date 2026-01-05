# OTS Server – Project Context

## Overview

`ots-simulator` is a Nuxt-based dashboard and WebSocket backend that:

1. **Receives events from the userscript** via WebSocket (`/ws`)
2. **Displays hardware module UI** in the dashboard (emulator mode)
3. **Forwards commands** between UI and userscript bidirectionally

**Important**: This server does NOT track or store detailed game state (players, scores, etc.). The current `GameState` type focuses on hardware module states (`hwState`) which contains status for general, alert, and nuke modules. Legacy types like `PlayerInfo` and `TeamScore` exist in shared types for potential future use, but the server currently only relays hardware-focused state and events.

It exposes:

- `GET /ws` – Single WebSocket endpoint for both dashboard UI and userscript
- Client identification via handshake message with `clientType` field

The dashboard shows:

- Hardware module emulators:
  - **NukeModule**: Nuke Control Panel (3 buttons with LEDs)
  - **AlertModule**: Alert indicators (6 LEDs for incoming threats)
  - **MainPowerModule**: Connection status (LINK LED)
  - **TroopsModule**: Troop count display (LCD + slider)
- Recent events from userscript/game
- Connection status for UI WebSocket and userscript

This server also acts as a **hardware emulator** during development: when real hardware isn't available, Vue components simulate physical modules (buttons, LEDs, displays) that would exist on the ESP32-S3 device.

## Key Files

- `nuxt.config.ts` – Nuxt configuration for the server app
- `app/pages/index.vue` – Main dashboard page
  - Uses Nuxt UI components (`UCard`, `UInput`, `UButton`)
  - Renders UI WS status and userscript status pills in the header
  - Displays hardware module emulators (NukeModule)
  - Shows recent events list
- `app/assets/css/main.css` – Nuxt UI Tailwind entrypoint
- `app/components/dashboard/HeaderStatus.vue` – header block
  - Shows "Game Dashboard" title and subtitle
  - Shows UI WS and USERSCRIPT status pills
  - Props: `status`, `userscriptStatus`, `userscriptBlink`
- `app/components/dashboard/EventsList.vue` – recent events card
  - Shows empty-state text when there are no events
  - Expects `events: GameEvent[]`
- `app/components/hardware/NukeModule.vue` – Nuke Control Panel emulator
  - Three buttons (Atom, Hydro, MIRV) with LED indicators
  - Independent blink states for each button
  - Emits `sendNuke` event when buttons are clicked
  - Props: `connected`, `blinkingButtons`
- `app/composables/useGameSocket.ts`
  - Wraps `@vueuse/core` `useWebSocket` for `/ws`
  - Parses incoming event messages
  - Tracks last userscript heartbeat time and status
  - Manages nuke button blink timers (4 seconds per button)
  - Exposes `sendNukeCommand()` helper function
  - Exposes `activeNukes` reactive state
- `server/routes/ws.ts`
  - Single WebSocket handler for all connections (`/ws`)
  - Uses handshake message to identify client type (`ui` or `userscript`)
  - Broadcasts all messages via `broadcast` channel
  - UI clients subscribe to `ui` channel, userscripts to `userscript` channel
  - Events from userscript are broadcast to all peers
  - Commands from UI are broadcast to all peers
  - Sends connection/disconnection INFO events for userscript status
- `../../ots-shared/src/game.ts`
  - Shared TypeScript types (kept in sync with `/prompts/protocol-context.md`)
  - Includes `NukeType`, `SendNukeCommand`, `NukeSentEventData` for hardware module

## WebSocket & Status Semantics

### Single WebSocket Endpoint (`/ws`)

- Both UI and userscript connect to the same `/ws` endpoint
- All messages are broadcast to all connected peers via the `broadcast` channel
- The dashboard connects using `useWebSocket` with autoReconnect
- Connection status is exposed as `status` and used to color the "UI WS" pill:
  - `OPEN` → green
  - `CONNECTING` → amber
  - `CLOSED` → red

### Userscript Status

- The userscript sends periodic `INFO` events (heartbeats)
- The composable updates `lastUserscriptHeartbeat` on any incoming `INFO` event
- `userscriptStatus` is computed from heartbeat age:
  - Last heartbeat `< 10s` → `ONLINE`
  - Last heartbeat `< 60s` → `STALE`
  - No heartbeat yet or older → `OFFLINE`
  - `UNKNOWN` → no heartbeat received yet
- `userscriptHeartbeatId` increments each time an `INFO` heartbeat is processed
- The index page watches `userscriptHeartbeatId` to animate the USERSCRIPT pill on each heartbeat

## Messages & Types

Messages follow the shared `IncomingMessage` / `OutgoingMessage` types defined in `/prompts/protocol-context.md`:

- `type: 'event'` – carries a `GameEvent` with:
  - `type: GameEventType` - All event types defined in protocol:
    - Game lifecycle: `GAME_START`, `GAME_END`, `WIN`, `LOOSE`
    - Alert events: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
    - Launch events: `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`
    - Outcome events: `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
    - Info events: `INFO`, `HARDWARE_TEST`
  - `timestamp`, optional `message`, optional `data`
- `type: 'cmd'` (from UI to userscript) – action commands
  - Example: `{ action: 'send-nuke', params: { nukeType: 'atom' } }`

### Hardware Module Protocol

The Nuke Control Panel uses these specific message patterns:

**Outbound (UI → Userscript):**
```typescript
{
  type: 'cmd',
  payload: {
    action: 'send-nuke',
    params: { nukeType: 'atom' | 'hydro' | 'mirv' }
  }
}
```

**Inbound (Userscript → UI):**
```typescript
{
  type: 'event',
  payload: {
    type: 'NUKE_LAUNCHED',  // or 'HYDRO_LAUNCHED', 'MIRV_LAUNCHED'
    timestamp: number,
    message: 'atom launched',
    data: { nukeType: 'atom' | 'hydro' | 'mirv', method: 'game.sendNuke()' }
  }
}
```

When the UI receives a nuke launched event (NUKE_LAUNCHED, HYDRO_LAUNCHED, or MIRV_LAUNCHED), it triggers a 4-second LED blink on the corresponding button. Multiple nukes can blink independently.

**Alert Events**: The Alert Module receives alert events (`ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`) which trigger corresponding alert LEDs with automatic timeouts (10s for nukes, 15s for invasions).

**Outcome Events**: The system tracks nuke outcomes and emits `NUKE_EXPLODED` (when nuke reaches target) and `NUKE_INTERCEPTED` (when destroyed before impact) events for logging and statistics. These are informational only and don't trigger hardware actions.

## UX Notes

- The header shows two pills:
  - `UI WS` – status of the dashboard's connection to `/ws-ui`
  - `USERSCRIPT` – derived userscript status with a one-shot blink on each heartbeat
- Events are displayed newest-first, capped to 100
- Hardware modules are interactive and emulate physical device behavior
- LED blink animations are managed via timers in the composable (4 seconds per nuke)

## Implementation Guidelines

- **No game state tracking**: The server does NOT store or sync `GameState`. It only relays events.
- **Hardware emulation**: Vue components in `app/components/hardware/` mirror physical modules defined in `/ots-hardware/`
- **LCD Display Emulation**: Replicate exact firmware display behavior from `/ots-hardware/DISPLAY_SCREENS_SPEC.md`
  - Use specification for exact text formatting, spacing, and alignment
  - Implement all 7 screen types (splash, waiting, lobby, troops, victory, defeat, shutdown)
  - Follow unit scaling rules (K/M/B) and text alignment (right/left)
  - Match screen transitions and event-to-screen mappings
- Keep shared types in `ots-shared` as the single source of truth
- Protocol definitions live in `/prompts/protocol-context.md` - update there first
- When adding new hardware modules:
  1. Define in `/ots-hardware/modules/<module-name>.md`
  2. Add types to `ots-shared/src/game.ts`
  3. Create Vue component in `app/components/hardware/`
  4. Update composable for any module-specific state/timers
- Prefer small, composable pieces rather than packing all logic into `index.vue`
