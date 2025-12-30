# Game Module

Integration layer between the OpenFront.io browser game and the OTS hardware controller.

## Overview

The game module bridges the gap between OpenFront.io's web game and the physical OTS hardware. It monitors game state changes, detects events (nukes, attacks, victories), and communicates with the hardware via WebSocket.

## Architecture

```
Game Page (OpenFront.io)
        ↓
   GameBridge ← main coordinator
        ├─→ GameAPI (safe game access)
        ├─→ Trackers (event detection)
        │     ├─ NukeTracker
        │     ├─ BoatTracker
        │     └─ LandAttackTracker
        ├─→ TroopMonitor (troop count updates)
        └─→ VictoryHandler (win/loss detection)
        ↓
   WebSocket → Server/Firmware
```

## Files

### Core

#### `openfront-bridge.ts` (567 lines)
Main coordinator that polls the game every 100ms and orchestrates all subsystems.

**Key Responsibilities:**
- Initialize and start game polling
- Coordinate trackers and monitors
- Handle commands from server (send-nuke, set-attack-ratio)
- Detect game phase transitions (lobby → spawning → in-game → ended)
- Process game updates and route to appropriate handlers

**Public Methods:**
- `start()` - Initialize game connection and start polling
- `handleCommand(action, params)` - Process commands from WebSocket

#### `game-api.ts` (400 lines)
Safe wrapper around OpenFront.io's game API with defensive programming.

**Key Features:**
- Null-safe access to game methods
- Caching to prevent excessive polling
- Type definitions for game objects
- Handles API changes gracefully

**Main Functions:**
- `createGameAPI()` - Factory for creating API instance
- `getGameView()` - Get current game instance
- Various safe accessors: `myPlayer()`, `units()`, `config()`, etc.

#### `constants.ts` (54 lines)
Centralized configuration constants.

**Sections:**
- DOM selectors
- Polling intervals
- Game update type IDs
- WebSocket constants
- Reconnection settings

### Event Detection

#### `trackers/` Subdirectory
Event detection modules that monitor game state for specific events. See [trackers/README.md](trackers/README.md) for details.

- `nuke-tracker.ts` (253 lines) - Nuclear weapon tracking
- `boat-tracker.ts` (168 lines) - Naval invasion detection
- `land-tracker.ts` (159 lines) - Ground attack monitoring

#### `troop-monitor.ts` (184 lines)
Monitors troop count changes and attack ratio slider.

**Features:**
- Detects current/max troop changes
- Monitors attack ratio slider (DOM input element)
- Sends state updates to server every 100ms
- Attaches event listeners to attack ratio input

### Victory/Defeat Logic

#### `victory-handler.ts` (NEW - 220 lines)
Extracted victory/defeat detection logic for better maintainability.

**Exports:**
- `extractWinner(winUpdate)` - Parse winner from Win update object
- `handleTeamVictory(ctx, winnerId)` - Process team game results
- `handleSoloVictory(ctx, winnerId)` - Process solo game results
- `processWinUpdate(ctx, winUpdate)` - Main entry point

**Why Extracted:**
- 150+ lines of complex if-else logic removed from openfront-bridge.ts
- Easier to test victory/defeat scenarios
- Cleaner separation of concerns
- Multiple winner detection strategies in one place

## Data Flow

### Game State Updates (100ms polling)

```
GameBridge.update() (every 100ms)
  ↓
GameAPI.getUpdates()
  ↓
Check for game phase changes:
  - Lobby → Spawning → In-Game → Ended
  ↓
Detect Win updates:
  - Extract winner from update object
  - Determine victory/defeat
  - Send GAME_END event
  - Play victory/defeat sound
  ↓
Check for death:
  - myPlayer.isAlive() === false
  - Send GAME_END event
  - Play death sound
  ↓
Update trackers:
  - NukeTracker.update()
  - BoatTracker.update()
  - LandAttackTracker.update()
  ↓
Update troop monitor:
  - TroopMonitor sends state updates
```

### Command Handling (from server)

```
WsClient receives command
  ↓
GameBridge.handleCommand(action, params)
  ↓
Validate params with type guards
  ↓
Route to handler:
  - send-nuke → handleSendNuke()
  - set-attack-ratio → handleSetAttackRatio()
  - ping → log and continue
  ↓
Execute game action
  ↓
Send confirmation event
```

## Event Types

### Outgoing (to server/firmware)

- `GAME_START` - Game begins
- `GAME_END` - Game ends (with victory/defeat data)
- `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED` - Nuclear weapons
- `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV` - Incoming nukes
- `ALERT_LAND`, `ALERT_NAVAL` - Incoming invasions
- `NUKE_EXPLODED`, `NUKE_INTERCEPTED` - Nuke outcomes
- `SOUND_PLAY` - Trigger hardware sounds
- `INFO` - General status updates

### Incoming (from server)

- `send-nuke` - Launch nuclear weapon
- `set-attack-ratio` - Set troop deployment percentage
- `ping` - Connection health check

## Configuration

Key constants in `constants.ts`:

```typescript
GAME_POLL_INTERVAL_MS = 100      // Game state polling
GAME_INSTANCE_CHECK_INTERVAL_MS = 500  // Game init check
TROOP_MONITOR_INTERVAL_MS = 100  // Troop update frequency
GAME_UPDATE_TYPE.WIN = 13        // Win update ID from game
```

## Usage Example

```typescript
import { GameBridge } from './game'

// Create bridge (typically in main.user.ts)
const gameBridge = new GameBridge(wsClient, hud)

// Start monitoring
await gameBridge.start()

// Handle commands from server
wsClient.onCommand((action, params) => {
  gameBridge.handleCommand(action, params)
})
```

## Testing & Debugging

**Enable debug logging:**
```typescript
// In constants.ts, set lower polling interval to see more logs
GAME_POLL_INTERVAL_MS = 1000  // Slower polling for debugging
```

**Check game connection:**
1. Open browser console
2. Look for `[GameBridge] Game instance detected`
3. Watch for `Update types this tick:` messages

**Test victory detection:**
1. Play a quick game
2. Console will show win update inspection
3. Victory/defeat event should fire

## Common Issues

**Game not connecting:**
- Check `control-panel` element exists on page
- Verify game instance is attached to element
- Look for `Game instance detected` in console

**Events not firing:**
- Check polling interval isn't too long
- Verify tracker registration in constructor
- Look for error messages in console

**Victory detection fails:**
- Enable win update inspection logs
- Check `extractWinner()` strategies
- Verify team/player ID comparison logic

## Future Improvements

- [ ] Add game replay support
- [ ] Implement event history/buffering
- [ ] Add configurable polling intervals
- [ ] Create mock game API for testing
- [ ] Add performance metrics collection
