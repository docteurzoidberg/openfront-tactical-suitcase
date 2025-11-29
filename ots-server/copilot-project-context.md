# OTS Server – Project Context

## Overview

`ots-server` is a Nuxt-based dashboard that receives game state and events from the Tampermonkey userscript ("OTS Game Dashboard Bridge") over WebSocket. It exposes:

- `GET /ws-ui` – WebSocket endpoint for the dashboard UI
- `GET /ws-script` – WebSocket endpoint for the userscript bridge

The dashboard shows:

- Current game state (map/mode, score, players)
- Recent events (KILL/DEATH/OBJECTIVE/INFO/etc.)
- Connection status for both UI WebSocket and userscript

## Key Files

- `nuxt.config.ts` – Nuxt configuration for the server app
- `app/pages/index.vue` – Main dashboard page
  - Uses Nuxt UI components (`UCard`, `UInput`, `UButton`) and small dashboard components
  - Renders UI WS status and userscript status pills in the header
  - Displays current `GameState` and a list of recent `GameEvent`s
  - Provides a small command form to send `cmd` messages to the userscript
- `app/assets/css/main.css` – Nuxt UI Tailwind entrypoint
  - Imports the Tailwind theme & utilities as recommended by Nuxt UI docs
- `components/dashboard/HeaderStatus.vue` – header block
  - Shows "Game Dashboard" title and subtitle
  - Shows UI WS and USERSCRIPT status pills
  - Props: `status`, `userscriptStatus`, `userscriptBlink`
- `components/dashboard/GameStateCard.vue` – current game card
  - Always rendered; shows placeholders until a `GameState` exists
  - Expects `state: GameState | null`
- `components/dashboard/EventsList.vue` – recent events card
  - Always rendered; shows empty-state text when there are no events
  - Expects `events: GameEvent[]`
- `composables/useGameSocket.ts`
  - Wraps `@vueuse/core` `useWebSocket` for `/ws-ui`
  - Parses incoming messages into `GameState` and `GameEvent`s
  - Tracks last userscript heartbeat time and status
  - Tracks userscript WebSocket open/close via special `INFO` events (`userscript-ws-open` / `userscript-ws-close`)
  - Exposes a monotonically increasing `userscriptHeartbeatId` used to blink the userscript status pill
- `server/routes/ws-script.ts`
  - WebSocket handler for userscript connections (`/ws-script`)
  - Tracks the latest `GameState` and a set of active userscript peers
  - Broadcasts incoming messages to the `"ui"` channel
  - Emits `INFO` events (`userscript-ws-open` / `userscript-ws-close`) on userscript WS lifecycle
- `server/routes/ws-ui.ts`
  - WebSocket handler for dashboard UI connections (`/ws-ui`)
  - Sends the latest `GameState` immediately on connect, if available
  - If there are active userscript peers when a UI client connects, sends an initial `userscript-ws-open` `INFO` event so the dashboard knows the userscript is already online
  - Forwards `cmd` messages from UI to the `"script"` channel
- `../../ots-shared/src/game`
  - Shared TypeScript types: `GameState`, `GameEvent`, `IncomingMessage`, `OutgoingMessage`, `GameEventType`

## WebSocket & Status Semantics

### UI WebSocket (`/ws-ui`)

- The dashboard connects to `/ws-ui` using `useWebSocket` with autoReconnect.
- Connection status is exposed as `status` and used to color the "UI WS" pill:
  - `OPEN` → green
  - `CONNECTING` → amber
  - `CLOSED` → red

### Userscript Status

- The userscript sends:
  - An `INFO` event `userscript-connected` when it connects
  - Periodic/demo `INFO` events via its internal `demoHook()`
- The `/ws-script` route now also emits `INFO` events on userscript WS lifecycle:
  - `userscript-ws-open` when a userscript WebSocket peer connects
  - `userscript-ws-close` when that peer disconnects
- The server side composable updates:
  - `lastUserscriptHeartbeat` on any incoming `INFO` event
  - `userscriptWsOpen` based on `userscript-ws-open` / `userscript-ws-close`
  - `userscriptStatus` computed from both WS state and heartbeat age:
    - WS closed → `OFFLINE`
    - WS open and last heartbeat `< 10s` → `ONLINE`
    - WS open and last heartbeat `< 60s` → `STALE`
    - no heartbeat yet → `UNKNOWN`
  - `userscriptHeartbeatId` increments each time an `INFO` heartbeat is processed

The index page watches `userscriptHeartbeatId` and toggles a `userscriptBlink` flag so the USERSCRIPT pill briefly animates (single ping) on each heartbeat.

## Messages & Types

Messages follow the shared `IncomingMessage` / `OutgoingMessage` types:

- `type: 'state'` – carries a full `GameState`
- `type: 'event'` – carries a `GameEvent` with:
  - `type: GameEventType` (`KILL`/`DEATH`/`OBJECTIVE`/`INFO`/`ERROR`/...)
  - `timestamp`, `message`, optional `data`
- `type: 'cmd'` (from UI to userscript) – action commands, e.g. `ping`, `focus-player:123`

The UI sends commands through the `Send Command` form by emitting `cmd` messages. The userscript reacts to known actions like `ping` and `focus-player:*`.

## UX Notes

- The header shows two pills:
  - `UI WS` – status of the dashboard’s connection to `/ws-ui`
  - `USERSCRIPT` – derived userscript status with a one-shot blink on each heartbeat
- Events are displayed newest-first, capped to a reasonable number (e.g. 100).
- Game state card shows timestamp, map, mode, team scores, and per-player info.

## Implementation Guidelines

- Keep the shared types in `ots-shared` the single source of truth for message structure.
- When evolving the protocol, update both:
  - The userscript (sender/consumer)
  - The server composable and any route handlers
- Prefer small, composable pieces (composables, components) rather than packing all logic into `index.vue`.
