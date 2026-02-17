# OTS Userscript

[![Version](https://img.shields.io/badge/version-2026--01--10.2--dev-blue)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![TypeScript](https://img.shields.io/badge/TypeScript-5.9-blue)](https://www.typescriptlang.org/)

A Tampermonkey/Greasemonkey userscript that bridges [OpenFront.io](https://openfront.io) gameplay with the OpenFront Tactical Suitcase (OTS) hardware controller via WebSocket.

![Userscript HUD](images/userscript-hud.png)

## Features

### 🎮 Game State Monitoring
- **Real-time troop tracking** - Current troops, max troops, attack ratio
- **Game phase detection** - Lobby, spawning, in-game, victory/defeat
- **Event tracking** - Nukes launched/exploded, attacks, alerts
- **100ms polling** with intelligent change detection (only sends updates when values change)

### 🚀 Nuke & Attack Integration
- **Nuke tracking** - Atom, Hydro, MIRV launches and impacts (by unitID)
- **Boat & land attack tracking** - Real-time invasion monitoring
- **Ghost structure targeting** - Uses game's native targeting workflow
- **Command handlers** - Dashboard can send nukes/attacks remotely

### ⌨️ Keypad Integration
- **15-key physical keypad** support via CAN bus
- **Customizable bindings** - Map keys to game actions (build, zoom, attack)
- **Live visualization** - See key presses in real-time
- **Action-based config** - Easy dropdown selection from current game keybinds
- **localStorage persistence** - Bindings saved across sessions

### 📊 Interactive HUD
- **Tabbed interface** with draggable/resizable window:
  - **Logs Tab** - Rolling event log with JSON inspection and advanced filtering
  - **Hardware Tab** - Real-time diagnostic status from firmware
  - **Sound Tab** - Sound event toggles with remote testing
  - **Keypad Tab** - Visual keypad layout with binding editor
- **Connection status** - WebSocket and game state indicators
- **Persistent settings** - Position, size, WebSocket URL saved

### 🔊 Sound System
- **Game events** - Start, victory, defeat, player death
- **Sound commands** - Play sounds via firmware audio module
- **Priority system** - Low, normal, high with interrupt support
- **Testing interface** - Trigger sounds directly from HUD

## Installation

### Prerequisites
- [Tampermonkey](https://www.tampermonkey.net/) browser extension
- Node.js 18+ (for building from source)
- OTS firmware or simulator running on `localhost:3000` (or custom WebSocket URL)

### Quick Install (Pre-built)

1. Install Tampermonkey in your browser
2. Download `build/userscript.ots.user.js`
3. Drag the file into your browser or click "Install" when prompted
4. Navigate to [https://openfront.io](https://openfront.io)
5. The OTS HUD should appear in the bottom-right corner

### Build from Source

```bash
# From repo root
cd ots-userscript

# Install dependencies
npm install

# Build userscript
npm run build

# Output: build/userscript.ots.user.js
```

Then install the built file in Tampermonkey.

## Configuration

### WebSocket URL

Default: `wss://localhost:3000/ws` (secure WebSocket with self-signed cert)

To change:
1. Click the ⚙️ (settings) icon in the HUD header
2. Enter new WebSocket URL
3. Click "Reconnect" or reload the page

**Common configurations:**
- **Firmware (HTTPS)**: `wss://localhost:3000/ws` (default)
- **Firmware (HTTP)**: `ws://localhost:3000/ws`
- **Simulator**: `ws://localhost:3000/ws` (if simulator uses HTTP)

### Keypad Bindings

Configure keypad-to-game-action mappings in the **Keypad Tab**:

1. Open the OTS HUD
2. Click the **Keypad** tab
3. For each key (K1-K15):
   - Select a game action from the dropdown
   - Toggle the checkbox to enable/disable the binding
4. Changes save automatically to localStorage

**Default bindings:**
- **K1-K7** (Row 1): Building actions (City, Factory, Port, Defense, Missile, SAM, Warship)
- **K8-K14** (Row 2): Game controls (Zoom, Attack ratio, Direction, Attacks)
- **K15** (Row 3): Toggle view (Spacebar)

**Note:** Nuke launches (8/9/0) use dedicated hardware buttons, not keypad.

### Sound Events

Control which game events trigger hardware sounds in the **Sound Tab**:

- Toggle individual sound events on/off
- Use "Test" buttons to verify audio module
- Changes apply immediately (no page reload needed)

## Usage

### Basic Operation

1. **Start the game** - Navigate to [https://openfront.io](https://openfront.io)
2. **Connect WebSocket** - HUD shows connection status (green = connected)
3. **Join a game** - Userscript automatically detects game start
4. **Monitor logs** - View real-time events in the Logs tab
5. **Check hardware** - View diagnostic info in Hardware tab

### Game Event Flow

```
Game State Change → Userscript Detection → WebSocket Send → Firmware/Dashboard
```

**Examples:**
- **Nuke launched** → `NUKE_LAUNCHED` event with unitID
- **Troops changed** → `TROOP_UPDATE` event with current/max
- **Game ends** → `GAME_END` event with victory/defeat status

### Keypad Operation

```
Physical Key Press → Firmware → CAN → Main Controller → WebSocket → Userscript
                                                                         ↓
                                                               localStorage lookup
                                                                         ↓
                                                                Trigger Game Action
```

**Press K1** (configured as "Build City"):
1. Physical keypad sends CAN message
2. Main controller forwards via WebSocket
3. Userscript receives `KEYPAD_KEY_PRESSED` event
4. Looks up binding: K1 → action "build:city"
5. Finds game hotkey for action (e.g., "1")
6. Dispatches synthetic keyboard event
7. Game builds city

### Remote Commands

The firmware/dashboard can send commands to the userscript:

- **`send-nuke`** - Launch nuke with ghost structure targeting
- **`send-land-attack`** - Trigger land attack
- **`send-boat-attack`** - Trigger boat attack
- **`set-attack-ratio`** - Change troop deployment percentage

## Architecture

### Project Structure

```
ots-userscript/
├── build/                      # Built output (gitignored)
│   └── userscript.ots.user.js
├── src/
│   ├── main.user.ts           # Tampermonkey entry point
│   ├── game/                  # Game integration
│   │   ├── openfront-bridge.ts
│   │   ├── keypad-manager.ts
│   │   └── trackers/          # Nuke/boat/land detection
│   ├── hud/                   # UI components
│   │   ├── sidebar-hud.ts     # Main HUD window
│   │   └── sidebar/tabs/      # Log, Hardware, Sound, Keypad tabs
│   ├── websocket/             # WebSocket client
│   │   └── client.ts
│   ├── storage/               # localStorage management
│   │   └── keypad.ts          # Keypad config persistence
│   ├── types/                 # TypeScript interfaces
│   │   └── keypad-types.ts
│   └── utils/                 # Helper functions
├── build.mjs                  # esbuild bundler
├── package.json
├── tsconfig.json
└── CHANGELOG.md
```

### Module Responsibilities

- **`openfront-bridge.ts`** - Main game integration, state polling, event emission
- **`keypad-manager.ts`** - Handles keypad WebSocket events, triggers game actions
- **`websocket/client.ts`** - WebSocket connection with auto-reconnect
- **`sidebar-hud.ts`** - Draggable/resizable HUD window manager
- **`trackers/`** - Specialized detectors for nukes, boats, lands (change detection)

### Event Types

See [`/prompts/WEBSOCKET_MESSAGE_SPEC.md`](../prompts/WEBSOCKET_MESSAGE_SPEC.md) for complete protocol specification.

**Key events:**
- `GAME_START`, `GAME_END` (with victory status)
- `NUKE_LAUNCHED`, `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
- `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
- `TROOP_UPDATE` (current, max, attack ratio)
- `KEYPAD_KEY_PRESSED`, `KEYPAD_KEY_RELEASED`
- `SOUND_PLAY` (with soundId, priority)

## Development

### Build System

Uses [esbuild](https://esbuild.github.io/) for fast TypeScript compilation:

```bash
npm run build   # Build userscript
npm run clean   # Remove build artifacts
```

Build configuration in `build.mjs`:
- Bundles all TypeScript into single `.user.js` file
- Includes Tampermonkey header with metadata
- Imports shared types from `../ots-shared/src/game.ts`

### Hot Reload

Tampermonkey can auto-reload userscripts:

1. Enable "Check for userscript updates" in Tampermonkey settings
2. In userscript editor, set update interval (e.g., 1 hour)
3. Modify `build/userscript.ots.user.js` directly during development
4. Save file → Tampermonkey auto-reloads on page refresh

For faster iteration: edit `build/userscript.ots.user.js` directly, then rebuild from source when stable.

### Debugging

Open browser DevTools console to see userscript logs:

```javascript
// Userscript logs are prefixed
[OTS] Connected to wss://localhost:3000/ws
[OTS] Game started
[OTS] Nuke launched: atom (unitID: 12345)
[KEYPAD] Key K1 pressed → action: build:city → hotkey: 1
```

Inspect WebSocket traffic:
1. Open DevTools → Network tab
2. Filter by "WS" (WebSocket)
3. Click WebSocket connection → Messages
4. View real-time message flow

### Testing

To test without physical hardware:

1. Run the OTS simulator: `cd ots-simulator && npm run dev`
2. Navigate to `http://localhost:3000` (dashboard)
3. Open [https://openfront.io](https://openfront.io) in another tab
4. Userscript connects to simulator
5. Test events appear in dashboard event log

Send test keypad events from simulator or use Python test scripts in `ots-fw-main/tools/tests/`.

## Related Projects

- **[ots-simulator](../ots-simulator)** - Nuxt 4 dashboard + WebSocket server (Nitro)
- **[ots-fw-main](../ots-fw-main)** - ESP32-S3 firmware (main controller)
- **[ots-fw-keypad](../ots-fw-keypad)** - M5Stack Stamp S3 keypad module firmware
- **[ots-shared](../ots-shared)** - Shared TypeScript protocol types
- **[ots-hardware](../ots-hardware)** - Hardware specifications and CAD files

## Documentation

- **[CHANGELOG.md](CHANGELOG.md)** - Version history and release notes
- **[copilot-project-context.md](copilot-project-context.md)** - Detailed project context for AI assistants
- **[src/README.md](src/README.md)** - Source code organization
- **[/prompts/WEBSOCKET_MESSAGE_SPEC.md](../prompts/WEBSOCKET_MESSAGE_SPEC.md)** - Protocol specification
- **[/doc/developer/](../doc/developer/)** - Developer guides and API documentation

## Troubleshooting

### WebSocket connection fails

**Symptoms:** HUD shows "Disconnected" status

**Solutions:**
1. Verify firmware/simulator is running: `curl http://localhost:3000`
2. Check WebSocket URL in HUD settings (wss vs ws)
3. For HTTPS pages: Use `wss://` (TLS required)
4. Accept self-signed certificate: Visit `https://localhost:3000` first
5. Check browser console for connection errors

### Keypad not working

**Symptoms:** Physical key presses don't trigger game actions

**Solutions:**
1. Verify WebSocket connection (green indicator in HUD)
2. Open Keypad tab and check bindings are enabled
3. Test key press - should highlight in real-time visualization
4. Check browser console for `[KEYPAD]` logs
5. Verify action has a game hotkey configured (e.g., "1" for build city)
6. Ensure game window has focus when pressing keys

### Nuke tracking incorrect

**Symptoms:** Nukes don't turn off LEDs after exploding

**Solutions:**
1. Check `NUKE_LAUNCHED` events include `nukeUnitID` field
2. Verify `NUKE_EXPLODED`/`INTERCEPTED` events have matching `unitID`
3. Look for firmware logs showing nuke state manager activity
4. Ensure firmware is tracking nukes by unitID (not timers)

### Game state not updating

**Symptoms:** Dashboard shows stale troop counts or attack ratio

**Solutions:**
1. Verify game is running (not in lobby)
2. Check browser console for polling activity
3. Ensure OpenFront.io game version matches expected API
4. Try refreshing the game page
5. Check WebSocket is sending events (Network tab → WS → Messages)

## Contributing

See the main [OTS repository](../) for contribution guidelines.

**Areas needing work:**
- Stage 5: Documentation (user guides, developer guides)
- End-to-end testing with physical keypad hardware
- Additional game event tracking (new OpenFront.io features)
- Performance optimization for background tab polling

## License

Part of the OpenFront Tactical Suitcase project. See main repository for license details.

## Credits

Built for [OpenFront.io](https://openfront.io) - Open-source multiplayer territory control game.

Game source: https://github.com/openfrontio/OpenFrontIO
