# Real-Time Troop Information Guide

## Overview
This guide shows how to access real-time troop information from the OpenFront.io game using the enhanced GameAPI.

## Available Methods

### 1. **getCurrentTroops()**
Returns the current number of troops the player has.

```typescript
const currentTroops = gameAPI.getCurrentTroops()
console.log(`Current troops: ${currentTroops}`)
```

**Returns:** `number | null`
- `number`: The current troop count
- `null`: If the game isn't loaded or player data isn't available

---

### 2. **getMaxTroops()**
Returns the maximum number of troops the player can have (troop cap).

```typescript
const maxTroops = gameAPI.getMaxTroops()
console.log(`Max troops: ${maxTroops}`)
```

**Returns:** `number | null`
- `number`: The maximum troop capacity
- `null`: If the game isn't loaded or config isn't available

**Note:** Max troops is calculated based on:
- Number of tiles owned
- City levels
- Player type (Human, Bot, Nation)
- Game difficulty

---

### 3. **getAttackRatio()**
Returns the current attack ratio slider percentage (unit send slider).

```typescript
const attackRatio = gameAPI.getAttackRatio()
console.log(`Attack ratio: ${(attackRatio * 100).toFixed(0)}%`)
```

**Returns:** `number`
- Value between `0.01` (1%) and `1.0` (100%)
- Default: `0.2` (20%) if not set

**Storage:** Saved in `localStorage.getItem('settings.attackRatio')`

---

### 4. **getTroopsToSend()**
Returns the calculated number of troops that would be sent in an attack based on the current attack ratio.

```typescript
const troopsToSend = gameAPI.getTroopsToSend()
console.log(`Troops to send: ${troopsToSend}`)
```

**Returns:** `number | null`
- `number`: Calculated as `floor(currentTroops * attackRatio)`
- `null`: If current troops aren't available

---

## Complete Usage Example

```typescript
import { createGameAPI } from './game/game-api'

// Create the game API instance
const gameAPI = createGameAPI()

// Check if game is valid before accessing data
if (gameAPI.isValid()) {
  // Get all troop information
  const currentTroops = gameAPI.getCurrentTroops()
  const maxTroops = gameAPI.getMaxTroops()
  const attackRatio = gameAPI.getAttackRatio()
  const troopsToSend = gameAPI.getTroopsToSend()
  
  // Log the information
  console.log('=== TROOP INFORMATION ===')
  console.log(`Current: ${currentTroops} / ${maxTroops}`)
  console.log(`Attack Ratio: ${(attackRatio * 100).toFixed(0)}%`)
  console.log(`Troops to Send: ${troopsToSend}`)
  
  // Calculate percentage
  if (currentTroops !== null && maxTroops !== null) {
    const percentage = (currentTroops / maxTroops * 100).toFixed(1)
    console.log(`Troop Capacity: ${percentage}%`)
  }
}
```

---

## Polling for Real-Time Updates

To continuously monitor troop changes, you can poll at regular intervals:

```typescript
// Poll every 100ms (same as game tick rate)
setInterval(() => {
  if (!gameAPI.isValid()) return
  
  const currentTroops = gameAPI.getCurrentTroops()
  const maxTroops = gameAPI.getMaxTroops()
  const troopsToSend = gameAPI.getTroopsToSend()
  
  // Update your UI or send to hardware
  updateDisplay({
    current: currentTroops,
    max: maxTroops,
    toSend: troopsToSend
  })
}, 100)
```

---

## Integration with Your Hardware Module

Here's how to send troop data to your hardware:

```typescript
import { createGameAPI } from './game/game-api'

class TroopMonitor {
  private gameAPI = createGameAPI()
  private wsClient: WsClient
  
  constructor(wsClient: WsClient) {
    this.wsClient = wsClient
    this.startMonitoring()
  }
  
  private startMonitoring() {
    setInterval(() => {
      this.sendTroopUpdate()
    }, 100) // Update every 100ms
  }
  
  private sendTroopUpdate() {
    if (!this.gameAPI.isValid()) return
    
    const data = {
      currentTroops: this.gameAPI.getCurrentTroops(),
      maxTroops: this.gameAPI.getMaxTroops(),
      attackRatio: this.gameAPI.getAttackRatio(),
      troopsToSend: this.gameAPI.getTroopsToSend()
    }
    
    // Send to WebSocket server
    this.wsClient.sendEvent('TROOP_UPDATE', 'Troop status', data)
  }
}
```

---

## How the Game Calculates These Values

### Current Troops
- Starts at initial spawn value
- Increases over time based on `troopIncreaseRate()`
- Capped at `maxTroops`
- Decreases when sending attacks
- Formula: `10 + (currentTroops^0.73) / 4 * (1 - currentTroops/maxTroops)`

### Max Troops
```typescript
// Base calculation
maxTroops = 2 * (numTilesOwned^0.6 * 1000 + 50000) + 
            cityLevels * cityTroopIncrease

// Modified by player type:
// - Bot: maxTroops / 3
// - Nation (Easy): maxTroops * 0.75
// - Nation (Medium): maxTroops * 1.0
// - Nation (Hard): maxTroops * 1.25
// - Nation (Impossible): maxTroops * 1.5
```

### Attack Ratio
- User-configurable slider in the control panel
- Stored in localStorage: `settings.attackRatio`
- Range: 1% to 100% (0.01 to 1.0)
- Default: 20% (0.2)
- Can be changed via keyboard shortcuts or slider

---

## Troubleshooting

### Returns `null`
If methods return `null`, check:
1. Is the game loaded? `gameAPI.isValid()`
2. Is the player spawned? `gameAPI.getMyPlayer() !== null`
3. Are you in a game? (not just in lobby)

### Attack Ratio Not Updating
The attack ratio is read from localStorage. If it's not updating:
1. Check the control panel slider is working
2. Verify localStorage access isn't blocked
3. Listen for changes: `localStorage.setItem('settings.attackRatio', '0.5')`

### Stale Data
The GameAPI caches the game instance for 1 second. If you need fresher data:
- Wait 1 second between critical reads
- Or modify `CACHE_DURATION` in game-api.ts

---

## Source Code References

Key files from OpenFront.io repository:
- **Control Panel**: `src/client/graphics/layers/ControlPanel.ts`
- **Player Implementation**: `src/core/game/PlayerImpl.ts`
- **Config (maxTroops)**: `src/core/configuration/DefaultConfig.ts` (line 848-876)
- **Troop Rate**: `src/core/configuration/DefaultConfig.ts` (line 878-909)
- **Attack Ratio Storage**: `src/client/UserSettingModal.ts` (line 172-197)
