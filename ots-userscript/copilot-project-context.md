# OTS Userscript Project Context

This file documents the `ots-userscript` subproject. It is a TypeScript-based Tampermonkey userscript that bridges a game web page to the local OTS dashboard via WebSockets.

## Game Source Code

- **Official Repository**: https://github.com/openfrontio/OpenFrontIO/
- **Local Clone**: Available at `../OpenFrontIO` (relative to workspace root)
- **Game Type**: Real-time multiplayer territory control game (OpenFront.io)

## Purpose

- Inject a userscript into the game page (OpenFront.io at `https://openfront.io/*`).
- Connect to the local OTS server over WebSocket and stream game state and events.
- Monitor and report real-time troop data (current/max/ratio) to hardware modules.
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

## Game Integration Notes

### Attack Ratio Access Methods

The OpenFront.io game stores the attack ratio (troop send percentage) in multiple places:

1. **UIState Object** (BEST - Live game state)
   - Accessible via `document.querySelector('control-panel').uiState.attackRatio`
   - Returns ratio as decimal (0.0 - 1.0)
   - Updated in real-time when player changes slider
   - Defined in: `src/client/graphics/UIState.ts`

2. **DOM Input Element** (BACKUP - Current UI value)
   - Element ID: `#attack-ratio`
   - Input type: range (1-100)
   - Returns percentage (1-100), must divide by 100
   - Located in: `src/client/graphics/layers/ControlPanel.ts`

3. **localStorage** (FALLBACK - Persisted setting)
   - Key: `settings.attackRatio`
   - Stores ratio as decimal string (e.g., "0.2")
   - Default value: "0.2" (20%)
   - Persisted across sessions

### Recommended Access Pattern

```typescript
// Priority order:
1. controlPanel.uiState.attackRatio  // Live game state
2. input#attack-ratio.value / 100    // Current UI value
3. localStorage.settings.attackRatio // Saved setting
4. 0.2                                // Default fallback
```

### Game End Detection Scenarios

The userscript must detect game end in multiple scenarios and send `GAME_END` events with correct victory/defeat status.

**Solo Game:**
- **Player dies**: DEFEAT - Modal shows "You died"
- **Player wins**: VICTORY - Modal shows "You won" (by controlling 95% of map)

**Team Game:**
- **Player dies**: DEFEAT - Modal shows "You died" (regardless of team outcome)
- **Player's team wins**: VICTORY - Modal shows "Your team won!"
- **Another team wins**: DEFEAT - Player's team lost

**Detection Implementation** (in `openfront-bridge.ts`):

1. **Win Updates (Primary)**: Check `game.updatesSinceLastTick()[11]` for `GameUpdateType.Win` updates
   - Contains `winner: [type, id]` where type is `'team'` or `'player'`
   - For team games: Compare `winner[1]` with `myPlayer.team()`
   - For solo games: Compare `winner[1]` with `myPlayer.clientID()`

2. **Death Detection**: Check `myPlayer.isAlive()` for immediate death detection
   - Triggers before Win updates in some cases
   - Always results in DEFEAT for the player

3. **Fallback**: Use `game.gameOver()` and check if player is eliminated
   - Less reliable for team games
   - Used when Win updates aren't available

**Important**: Win updates use the game's internal translation system, so don't look for literal strings like "You won" in the code. The modal text is determined by translation labels based on the win update data.

### Ghost Structure Targeting Mode

OpenFrontIO uses a **ghost structure system** for all unit placement (buildings, nukes, etc.). This is the internal mechanism that powers keybinds (8/9/0 for nukes) and the build menu.

**How It Works**:

1. **Activation**: Set `controlPanel.uiState.ghostStructure` to the unit type name (string)
2. **UI Feedback**: Game renders a transparent "ghost" of the structure on the map
3. **User Interaction**: User moves mouse to preview placement location, then clicks to confirm
4. **Processing**: `StructureIconsLayer.createStructure()` handles the click event
5. **Cleanup**: Game automatically sets `ghostStructure` back to `null` after placement

**Implementation Example** (from `handleSendNuke` in openfront-bridge.ts):

```typescript
// Access control panel to get UIState
const controlPanel = document.querySelector('control-panel') as any

// Get unit type name (STRING, not number ID)
const unitTypeName = 'Atom Bomb' // or 'Hydrogen Bomb', 'MIRV'

// Activate ghost structure targeting mode (identical to pressing keybind 8/9/0)
controlPanel.uiState.ghostStructure = unitTypeName

// Emit event to update UI components
class GhostStructureChangedEvent {
  constructor(public readonly ghostStructure: string | null) {}
}
const eventBus = (window as any).game.eventBus
eventBus.emit(new GhostStructureChangedEvent(unitTypeName))

// Monitor for completion (ghostStructure returns to null after user clicks)
const checkInterval = setInterval(() => {
  if (controlPanel.uiState.ghostStructure === null) {
    clearInterval(checkInterval)
    console.log('Target selected, nuke launched')
  }
}, 100)
```

**Key Points**:

- **Unit Type Format**: Must be STRING name (e.g., `"Atom Bomb"`), not numeric ID
- **Don't Call API Directly**: Avoid calling `buildMenu.sendBuildOrUpgrade()` or emitting `BuildUnitIntentEvent` manually - this bypasses the targeting UI workflow
- **Let Game Handle Clicks**: Once `ghostStructure` is set, the game's `StructureIconsLayer` automatically processes map clicks
- **Works for All Buildables**: This pattern works for any buildable unit/structure, not just nukes
- **Keybinds Reference**: See `OpenFrontIO/src/client/components/UnitDisplay.ts` for how keybinds 8/9/0 implement this

**Why This Approach**:

Direct API calls (like `sendBuildOrUpgrade(x, y)`) skip the targeting workflow and often target the wrong tile (e.g., your capital or nearest existing silo). The ghost structure system is the game's intended mechanism for user-selected tile placement.

### Game API Access Points

The game exposes its API through DOM elements with custom elements:
- `events-display` → `.game` property → GameView instance
- `control-panel` → `.uiState` property → UIState instance
- Game methods: `myPlayer()`, `config()`, `troops()`, `maxTroops()`, etc.

Source: https://github.com/openfrontio/OpenFrontIO/tree/main/src/client/graphics/

## WebSocket Behavior

- Default WebSocket URL: `ws://localhost:3000/ws`.
- The effective URL is loaded from the Tampermonkey storage key `ots-ws-url` (via `GM_getValue`); if missing or empty, it falls back to the default.
- On connect:
  - HUD status badge shows `CONNECTING` then `OPEN` when connected.
  - Sends handshake message: `{"type": "handshake", "clientType": "userscript"}`
  - Sends periodic `INFO` events (heartbeats) to indicate connection status.
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
