# Spawn Detection - Player Spawn Tile Selection

## Overview

This document explains how to detect when a player has chosen a spawn tile in OpenFront.io.

## API Method

The `gameAPI.hasSpawned()` method returns whether the player has selected a spawn tile:

- `true` - Player has chosen a spawn location
- `false` - Player has NOT chosen a spawn location yet (still in spawn selection phase)
- `null` - Player doesn't exist or game not loaded

## Game Flow States

```
Lobby
  ↓
Player joins game
  ↓
Spawn Selection Phase (hasSpawned = false, inSpawnPhase = true)
  ↓ [Player clicks spawn tile]
Spawn Countdown (hasSpawned = true, inSpawnPhase = true)
  ↓ [Countdown ends]
Game Active (hasSpawned = true, inSpawnPhase = false)
```

## Usage Example

### Basic Check

```typescript
import { createGameAPI } from './game/game-api'

const gameAPI = createGameAPI()

// Check if player has spawned
const hasSpawned = gameAPI.hasSpawned()

if (hasSpawned === true) {
  console.log('✓ Player has chosen spawn tile')
} else if (hasSpawned === false) {
  console.log('✗ Player has NOT chosen spawn tile yet')
} else {
  console.log('? Player not in game or not loaded')
}
```

### Monitoring Spawn Selection

```typescript
class SpawnMonitor {
  private hasDetectedSpawn = false

  poll(gameAPI: GameAPI) {
    const hasSpawned = gameAPI.hasSpawned()
    
    // Detect when player first chooses spawn tile
    if (hasSpawned === true && !this.hasDetectedSpawn) {
      this.hasDetectedSpawn = true
      console.log('[SpawnMonitor] Player has chosen spawn location!')
      
      // Emit event or trigger actions
      this.onSpawnChosen()
    }
    
    // Reset when player leaves game
    if (hasSpawned === null && this.hasDetectedSpawn) {
      this.hasDetectedSpawn = false
      console.log('[SpawnMonitor] Player left game - resetting')
    }
  }
  
  private onSpawnChosen() {
    // Your logic here
    // Example: Send event to hardware, enable features, etc.
  }
}
```

### Integration with GameBridge

You can add spawn detection to the existing `openfront-bridge.ts` polling loop:

```typescript
private startPolling() {
  this.pollInterval = setInterval(() => {
    const gameAPI = createGameAPI()
    
    if (!gameAPI.isValid()) return
    
    const myPlayer = gameAPI.getMyPlayer()
    if (!myPlayer) return
    
    // Check spawn status
    const hasSpawned = gameAPI.hasSpawned()
    const inSpawnPhase = /* get from game */
    
    // Player is in spawn selection phase but hasn't chosen yet
    if (inSpawnPhase && hasSpawned === false) {
      console.log('[GameBridge] Player in spawn selection - waiting for choice')
    }
    
    // Player has chosen spawn tile (countdown phase)
    if (inSpawnPhase && hasSpawned === true) {
      console.log('[GameBridge] Player spawned - countdown active')
    }
    
    // ... rest of polling logic
  }, 100)
}
```

## Combining with Other State Checks

### Determining Current Game Phase

```typescript
function determineGamePhase(gameAPI: GameAPI): string {
  const myPlayer = gameAPI.getMyPlayer()
  if (!myPlayer) return 'lobby'
  
  const hasSpawned = gameAPI.hasSpawned()
  const gameStarted = gameAPI.isGameStarted()
  
  if (hasSpawned === false) {
    return 'spawn-selection' // Player needs to choose spawn tile
  }
  
  if (hasSpawned === true && gameStarted === false) {
    return 'spawn-countdown' // Spawn chosen, countdown active
  }
  
  if (hasSpawned === true && gameStarted === true) {
    return 'in-game' // Game active, player playing
  }
  
  return 'unknown'
}
```

### Send Event When Spawn Chosen

```typescript
import type { GameEvent } from '../../ots-shared/src/game'

class SpawnTracker {
  private lastSpawnState: boolean | null = null
  
  detect(gameAPI: GameAPI, ws: WebSocketClient) {
    const hasSpawned = gameAPI.hasSpawned()
    
    // Detect spawn selection transition (false → true)
    if (this.lastSpawnState === false && hasSpawned === true) {
      const event: GameEvent = {
        type: 'INFO',
        timestamp: Date.now(),
        message: 'Player chose spawn location',
        data: { phase: 'spawn-chosen' }
      }
      
      ws.sendEvent(event.type, event.message, event.data)
      console.log('[SpawnTracker] Spawn location selected!')
    }
    
    this.lastSpawnState = hasSpawned
  }
}
```

## Game Source Reference

The `hasSpawned()` method is available on the player object in the OpenFront.io game source:

```typescript
// From OpenFrontIO game source (player object)
player.hasSpawned() // Returns boolean
```

This is already used in the userscript for death detection:

```typescript
// From ots-userscript/src/game/openfront-bridge.ts (line 207)
const hasSpawned = myPlayer.hasSpawned ? myPlayer.hasSpawned() : false
```

## Important Notes

1. **Spawning Phase vs Spawn Selection**
   - `hasSpawned() === false` → Player in spawn selection UI
   - `hasSpawned() === true` → Player chose tile, countdown active or game running

2. **Team Games**
   - Players can choose spawn after game starts if they join late
   - Always check `hasSpawned()` to detect if player is active

3. **Death and Respawn**
   - When player dies, `hasSpawned()` typically remains `true`
   - Use `isAlive()` to check if player is currently alive

4. **Null Safety**
   - Always check for `null` return value
   - Indicates player object doesn't exist or game not loaded

## See Also

- [game-api.ts](./src/game/game-api.ts) - GameAPI interface and implementation
- [openfront-bridge.ts](./src/game/openfront-bridge.ts) - Main game polling logic
- [protocol-context.md](../prompts/protocol-context.md) - Event definitions
