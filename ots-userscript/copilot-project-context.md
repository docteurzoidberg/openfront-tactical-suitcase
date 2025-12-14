# OTS Userscript Project Context

This file documents the `ots-userscript` subproject. It is a TypeScript-based Tampermonkey userscript that bridges a game web page to the local OTS dashboard via WebSockets.

## Purpose

- Inject a userscript into the game page (e.g. `https://example-game.com/*`).
- Connect to the local OTS server over WebSocket and stream game state and events.
- Provide an in-page HUD for:
  - WebSocket connection status.
  - A rolling log of sent/received/info messages.
  - Dragging and resizing the HUD window, with position/size persisted across page loads.
  - An independent settings window for configuring the WebSocket URL at runtime, with the URL persisted across page loads.

## Repo Layout (userscript-only)

- `ots-userscript/`
  - `package.json` — independent npm project for building the userscript.
  - `tsconfig.json` — standalone TS config targeting browser/DOM.
  - `build.mjs` — esbuild-based bundler that emits the final `.user.js` file.
  - `src/main.user.ts` — **single main entrypoint** for the Tampermonkey userscript.
  - `build/userscript.ots.user.js` — built userscript output, ready to install in Tampermonkey.

The userscript imports shared protocol/game types from the sibling `ots-shared` project via relative paths (e.g. `../../ots-shared/src/game`). That folder is **not** an npm package; it's just a TS-only shared types location.

## Build & Install

From repo root:

```sh
cd ots-userscript
npm install
npm run build
```

This produces `build/userscript.ots.user.js` with a Tampermonkey header. Install/update it in Tampermonkey by:

- Opening the generated file in the browser (or copy-paste into a new userscript), or
- Drag-and-dropping the `.user.js` file into the browser/Tampermonkey UI.

The `@match` rule in the header is currently set to `https://openfront.io/*`.

## WebSocket Behavior

- Default WebSocket URL: `ws://localhost:3000/ws-script`.
- The effective URL is loaded from the Tampermonkey storage key `ots-ws-url` (via `GM_getValue`); if missing or empty, it falls back to the default.
- On connect:
  - HUD status badge shows `CONNECTING` then `OPEN` when connected.
  - Sends an `INFO` event `userscript-connected` with the current page URL.
  - Sends an initial demo `GameState` payload.
- On close/error:
  - HUD status shows `DISCONNECTED` or `ERROR`.
  - Exponential backoff auto-reconnect (starting at 2s, up to ~15s).

Messages follow the shared types defined in `ots-shared/src/game.ts`:

- Outgoing:
  - `{ type: "event", payload: GameEvent }` - GameEventType includes:
    - Alert events: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
    - Launch events: `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`
    - Outcome events: `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
    - Game lifecycle: `GAME_START`, `GAME_END`, `WIN`, `LOOSE`
    - Info events: `INFO`, `HARDWARE_TEST`
- Incoming:
  - `{ type: "cmd", payload: { action: string, params?: Record<string, unknown> } }`

The userscript currently implements commands from the dashboard:

- `ping` → replies with `INFO` event `pong-from-userscript`.
- `send-nuke` → attempts to send nuke in game, replies with `NUKE_LAUNCHED`/`HYDRO_LAUNCHED`/`MIRV_LAUNCHED` event if successful.

**Event-Driven Architecture**: The userscript uses polling trackers (NukeTracker, BoatTracker, LandAttackTracker) that detect game events every 100ms and emit protocol-compliant events. Alert events trigger LED indicators on hardware modules or in the dashboard emulator. Outcome events (exploded/intercepted) are logged for statistics and debugging.

## HUD UI

The userscript injects a compact HUD element fixed to the top-right of the page:

- **Status header**:
  - Two small pills:
    - `WS` pill with a colored dot for WebSocket status (`DISCONNECTED`, `CONNECTING`, `OPEN`, `ERROR`).
    - `GAME` pill with a colored dot reserved for future game-state status (currently always red).
  - Gear button (`⚙`) to open settings.
  - Close button (`×`) to hide the HUD for the current page lifetime.
- **Log area**:
  - Shows a stream of log lines with tags:
    - `SEND` — JSON messages sent to the server.
    - `RECV` — messages received from the server.
    - `INFO` — internal informational messages (connect, close, errors, URL changes, etc.).
  - Log auto-scrolls to the latest entry.
- **Drag**:
  - Drag the header bar to move the HUD around the viewport.
  - The HUD's position (left/top) is persisted via Tampermonkey storage under `ots-hud-pos` so it is restored on the next page load.
- **Resize**:
  - Bottom-right handle (diagonal lines) allows resizing; min width/height are enforced.
  - The HUD's size (width/height) is persisted under `ots-hud-size` and restored on the next load.

## Settings Window (WebSocket URL)

The gear button opens a separate, floating settings window:

- Shows a `WebSocket URL` text input.
- Buttons:
  - **Reset**: restore the input to the built-in default `ws://localhost:3000/ws-script` (does not immediately save).
  - **Save**: validate non-empty value, then:
    - Update the in-memory `currentWsUrl`.
    - Persist to `localStorage` under `ots-ws-url`.
    - Log an `INFO` entry: `WS URL updated to ...`.
    - If the socket is currently open, close it with code `4100` and reason `"URL changed"`.
    - Hide the settings panel and call `connect()` to reconnect using the new URL.
- The window can be closed via its own `×` button.
- The settings window is an independent fixed-position overlay:
  - Initially positioned near the top-right of the viewport.
  - Can be dragged anywhere on screen using its header bar (it does not move the main HUD).
  - Dragging works on the header only and adjusts the window's `top`/`left` coordinates, keeping its width constant.

Both the main HUD window and the settings window reuse small internal classes for their window behavior:

- A `BaseWindow` class encapsulates the main HUD container and provides header-based drag handling.
- A `FloatingPanel` class encapsulates the settings panel element and provides its own header-based drag handling without affecting the HUD.

## Demo Event Generator

For now, the userscript does not read real game state from the page. Instead, it uses a demo hook:

- On startup, `demoHook()` sets up a `setInterval` every 5 seconds.
- Each tick sends a `GameEvent` with a random type among `KILL`, `DEATH`, `OBJECTIVE`, and `INFO`, and a small payload with `random: Math.random()`.

This is only to exercise the protocol and dashboard UI. When integrating with a real game:

- Replace `demoHook()` with logic that reads from the game DOM / JS globals.
- Build a proper `GameState` snapshot and `GameEvent`s mapped to the shared types in `ots-shared/src/game.ts`.

## Implementation Notes

- The userscript is compiled as an IIFE and runs in page context with Tampermonkey grants for `GM_getValue` and `GM_setValue`.
- All DOM manipulation is done with plain DOM APIs to keep the bundle small and dependency-free.
- The HUD creation is lazy: any status/log update first calls `ensureHud()` so the HUD is created as soon as it is needed.
- HUD and settings window drag behavior is implemented via the `BaseWindow` and `FloatingPanel` helper classes in `src/hud/window.ts`.
- Persistent state (WS URL, HUD position, HUD size) is stored via `GM_setValue` and restored via `GM_getValue` on startup.

## When Updating This Project

When you add or change behavior in `src/main.user.ts`, remember to:

- Update this file to reflect any new commands, message shapes, or HUD/settings behavior.
- Rebuild the userscript via `npm run build` and reinstall it in Tampermonkey.
- Keep the description of the WebSocket protocol in sync with `ots-shared/src/game.ts`.
