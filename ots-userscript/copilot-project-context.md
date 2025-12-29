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
- Detect and report game events (nukes, alerts, game start/end, victory/defeat).
- Provide an in-page **tabbed HUD** with:
  - **Logs Tab**: Rolling log with collapsible JSON, advanced filtering (direction, event types, heartbeats, sounds)
  - **Hardware Tab**: Real-time hardware diagnostic status display
  - **Sound Tab**: Sound event toggles with remote testing buttons
  - WebSocket and game connection status indicators
  - Draggable and resizable window with position/size persistence
  - Settings panel for WebSocket URL configuration

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
    - Sound events: `SOUND_PLAY` (when sound toggles enabled)
    - Info events: `INFO`, `HARDWARE_TEST`, `HARDWARE_DIAGNOSTIC`
- Incoming:
  - `{ type: "cmd", payload: { action: string, params?: Record<string, unknown> } }`
  - `{ type: "event", payload: GameEvent }` - Events from server (e.g., hardware diagnostic responses)

The userscript currently implements commands from the dashboard:

- `ping` → replies with `INFO` event `pong-from-userscript`.
- `send-nuke` → attempts to send nuke in game, replies with `NUKE_LAUNCHED`/`HYDRO_LAUNCHED`/`MIRV_LAUNCHED` event if successful.

**Event-Driven Architecture**: The userscript uses polling trackers (NukeTracker, BoatTracker, LandAttackTracker) that detect game events every 100ms and emit protocol-compliant events. Alert events trigger LED indicators on hardware modules or in the dashboard emulator. Outcome events (exploded/intercepted) are logged for statistics and debugging.

**Sound System**: The userscript sends `SOUND_PLAY` events to the server/firmware, but does NOT play sounds locally. Sound toggles in the Sound tab control whether events are sent. Each game event (start, victory, defeat, death) can be independently enabled/disabled. Test buttons allow remote sound testing without triggering game events.

## HUD UI

The userscript injects a **tabbed HUD** element fixed to the top-right of the page:

### Main Window Structure

- **Status header**:
  - Two status pills:
    - `WS` pill with a colored dot for WebSocket status (`DISCONNECTED`, `CONNECTING`, `OPEN`, `ERROR`).
    - `GAME` pill with a colored dot for game connection status.
  - Heartbeat indicator (blinks on server heartbeat, 15s interval)
  - Gear button (`⚙`) to open settings panel
  - Close button (`×`) to hide the HUD for the current page lifetime
  
- **Tab Navigation**:
  - Three tabs: **Logs**, **Hardware**, **Sound**
  - Active tab highlighted with blue background
  - Tab switching preserves log history and settings

### Logs Tab

Main features:
- **Message Stream**: Shows sent/received/info messages with color-coded tags
  - `SEND` (blue) — Outgoing messages to server
  - `RECV` (green) — Incoming messages from server  
  - `INFO` (yellow) — Internal informational messages
- **Advanced Filtering**: Comprehensive filter controls above log area
  - **Direction filters**: Send, Recv, Info (checkboxes)
  - **Event type filters**: Game Events, Nukes, Alerts, Troops, Sounds, System, Heartbeat
  - Filters persist across page reloads (GM storage)
  - Real-time log filtering without page refresh
- **Collapsible JSON**:
  - Event messages show summary text when collapsed
  - Click to expand and show color-formatted JSON
  - Syntax highlighting: keys (cyan), strings (green), numbers (orange), booleans (purple)
  - Re-click to collapse back to summary
- **Auto-scroll**: Automatically scrolls to newest entries
- **Event type colors**: Different colors for different event types (cyan for hardware, yellow for sounds, etc.)

### Hardware Tab

Features:
- **Real-time Diagnostic Display**: Shows hardware module status
- **Diagnostic Request Button**: Sends `hardware-diagnostic` command to firmware
- **Status Grid**: Displays responses in organized format:
  - Module name and type
  - Initialization status
  - Pin configurations
  - I2C addresses
  - Error states
- **Auto-update**: Refreshes when new diagnostic data received

### Sound Tab

Features:
- **Sound Toggle Controls**: Enable/disable sound events per sound type
  - `game_start` — Game start sound
  - `game_player_death` — Player death sound
  - `game_victory` — Victory sound
  - `game_defeat` — Defeat sound
- **Toggle Persistence**: Sound settings saved to GM storage
- **Remote Test Buttons**: ▶ Test button for each sound
  - Sends `SOUND_PLAY` event to server/firmware
  - Tests actual hardware without triggering game events
  - Includes `test: true` flag in event data
  - Respects toggle state (won't send if sound disabled)
- **Visual Feedback**: Hover effects on rows and test buttons

### Dragging & Resizing

- **Drag**: Click and drag the header bar to move the HUD around the viewport
  - Position persisted via `GM_setValue` under `ots-hud-pos`
  - Restored on page reload
- **Resize**: Bottom-right handle (diagonal lines) allows resizing
  - Min width/height enforced (320px × 240px)
  - Size persisted under `ots-hud-size`
  - Restored on page reload

## Settings Panel

The gear button opens a floating settings panel (above the main HUD):

- **WebSocket URL Configuration**:
  - Text input for WebSocket server URL
  - Default: `ws://localhost:3000/ws`
  - **Reset** button: Restores default URL (doesn't save)
  - **Save** button: Validates and saves URL, then reconnects
- **Panel Behavior**:
  - Independent draggable window (doesn't move main HUD)
  - Positioned near top-right initially
  - Can be closed via `×` button
  - URL changes logged to Info stream
  - Auto-reconnect on URL save

## Log Filtering System

The Logs tab includes a comprehensive filtering system:

### Filter Categories

1. **Direction Filters** (left group):
   - Send — Show outgoing messages
   - Recv — Show incoming messages
   - Info — Show internal messages

2. **Event Type Filters** (right group):
   - Game Events — Lifecycle events (start/end/win/lose)
   - Nukes — Nuclear launch events
   - Alerts — Incoming threat alerts
   - Troops — Troop count updates
   - Sounds — Sound play events
   - System — Hardware diagnostic and test events
   - Heartbeat — Periodic connection heartbeats (15s interval)

### Filter Behavior

- **Persistence**: All filter states saved to GM storage (`ots-log-filters`)
- **Real-time**: Filters apply immediately without page reload
- **Independent**: Each filter can be toggled independently
- **Visual feedback**: Unchecked filters hide matching log entries
- **Default state**: All filters enabled on first run

## Sound Control System

### Architecture

The userscript acts as an **event sender only** — it does NOT play sounds locally:

1. **Game events detected** → Check sound toggle state
2. **If toggle enabled** → Send `SOUND_PLAY` event to server/firmware
3. **Server/firmware** → Play actual sound on hardware/dashboard

### Sound Toggle Implementation

Each sound toggle:
- Checkbox with label showing friendly name + sound ID
- State persisted to GM storage (`ots-sound-toggles`)
- Checked by default on first run
- Prevents event sending when unchecked

### Test Button Implementation

Each sound row includes a **▶ Test** button:
- Sends `SOUND_PLAY` event with `test: true` flag
- Respects toggle state (won't send if sound disabled)
- Blue hover effect (rgba(59,130,246,0.3))
- Logs test action to Info stream
- Priority set to 'high' for immediate playback

### Game Event Integration

Sound events are sent from `openfront-bridge.ts` at these triggers:
- **Game start** → `game_start` sound (if enabled)
- **Player death** → `game_player_death` sound (if enabled)
- **Victory** → `game_victory` sound (if enabled)
- **Defeat** → `game_defeat` sound (if enabled)

All 9 sound event locations wrapped with `hud.isSoundEnabled(soundId)` checks.

## Hardware Diagnostic Protocol

### Outgoing Command

Send to request diagnostic info:
```json
{
  "type": "cmd",
  "payload": {
    "action": "hardware-diagnostic"
  }
}
```

### Incoming Response

Server/firmware responds with:
```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_DIAGNOSTIC",
    "timestamp": 1234567890,
    "message": "Hardware diagnostic response",
    "data": {
      "modules": [
        {
          "name": "Alert Module",
          "type": "alert",
          "initialized": true,
          "pins": {...},
          "i2c_address": "0x20",
          "error": null
        },
        // ... more modules
      ]
    }
  }
}
```

### Display Format

Hardware tab shows diagnostic data in a clean grid:
- Module name (bold)
- Initialization status (✓ OK / ✗ FAILED)
- Pin assignments
- I2C addresses
- Error messages (if any)

## Settings Window (WebSocket URL)

The gear button opens a separate, floating settings window:

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

## Code Architecture

### Key Files

- `src/main.user.ts` — Entry point, initializes WsClient, GameBridge, and Hud
- `src/hud/sidebar-hud.ts` — Main HUD implementation with tabs, filters, and sound controls
- `src/hud/window.ts` — Base classes for draggable windows
- `src/websocket/client.ts` — WebSocket client with auto-reconnect
- `src/game/openfront-bridge.ts` — Game event detection and sound integration
- `src/game/trackers/` — NukeTracker, BoatTracker, LandAttackTracker for event detection

### Component Interactions

```
main.user.ts
  ├─> WsClient (WebSocket connection)
  │     ├─> Auto-reconnect with exponential backoff
  │     └─> Message send/receive
  │
  ├─> Hud (UI and state management)
  │     ├─> Tabs (Logs, Hardware, Sound)
  │     ├─> Log filtering and display
  │     ├─> Sound toggle controls
  │     ├─> Hardware diagnostic display
  │     └─> Settings panel
  │
  └─> GameBridge (Game integration)
        ├─> Event trackers (100ms polling)
        ├─> Sound event sending (with toggle checks)
        └─> Ghost structure targeting for nukes
```

### State Persistence

Uses Tampermonkey `GM_setValue`/`GM_getValue`:
- `ots-hud-pos` — HUD window position (left, top)
- `ots-hud-size` — HUD window size (width, height)
- `ots-hud-collapsed` — HUD collapsed state (boolean)
- `ots-ws-url` — WebSocket server URL
- `ots-log-filters` — Log filter states (JSON)
- `ots-sound-toggles` — Sound toggle states (JSON)

## Game Event Detection

The userscript uses dedicated tracker classes that poll the game every 100ms:

## Implementation Notes

- The userscript is compiled as an IIFE and runs in page context with Tampermonkey grants for `GM_getValue` and `GM_setValue`.
- All DOM manipulation is done with plain DOM APIs to keep the bundle small and dependency-free.
- The HUD creation is lazy: any status/log update first calls `ensureHud()` so the HUD is created as soon as it is needed.
- HUD and settings window drag behavior is implemented via the `BaseWindow` and `FloatingPanel` helper classes in `src/hud/window.ts`.
- Persistent state (WS URL, HUD position, HUD size, filters, sound toggles) is stored via `GM_setValue` and restored via `GM_getValue` on startup.
- Sound system is event-only: userscript sends `SOUND_PLAY` events, server/firmware play sounds
- Log entries with JSON payloads are collapsible with syntax-highlighted expansion
- Hardware diagnostics use a request/response pattern with dedicated display in Hardware tab

## Key Features Summary

### Tabbed Interface
- **3 tabs**: Logs, Hardware, Sound
- Tab state preserved during session
- Each tab has dedicated functionality and controls
- Smooth tab switching with visual feedback

### Advanced Log Filtering
- **Direction filters**: Send, Recv, Info
- **Event type filters**: Game Events, Nukes, Alerts, Troops, Sounds, System, Heartbeat
- Real-time filtering with instant UI updates
- Filter states persist across page reloads
- Collapsible JSON with color formatting for readability

### Sound Control System
- **4 sound types**: game_start, game_player_death, game_victory, game_defeat
- Toggle switches control event sending (not local playback)
- Remote testing via ▶ Test buttons
- Test events marked with `test: true` flag
- Sound toggle states persist across page reloads
- Integration at 9 locations in game event detection

### Hardware Diagnostics
- Request button in Hardware tab
- Real-time diagnostic data display
- Module status grid with initialization state
- Pin configuration display
- I2C address information
- Error state reporting

### Window Management
- Draggable main HUD window
- Resizable with min dimensions (320×240px)
- Independent draggable settings panel
- Position/size persistence via GM storage
- Collapsible/expandable with close button

## When Updating This Project

When you add or change behavior in `src/main.user.ts` or other source files, remember to:

- Update this file to reflect any new commands, message shapes, HUD features, or behavior changes.
- Rebuild the userscript via `npm run build` and reinstall it in Tampermonkey.
- Keep the description of the WebSocket protocol in sync with `ots-shared/src/game.ts` and `/prompts/protocol-context.md`.
- Update filter systems if adding new event types.
- Add sound toggles if introducing new sound events.
- Document any new tabs or UI sections in this file.
