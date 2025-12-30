# Game Trackers

Event-driven trackers for monitoring OpenFront.io game entities in real-time.

## Overview

Each tracker monitors a specific type of game entity (nukes, boats, land attacks) and emits events when state changes occur. All trackers follow a consistent pattern:

1. **Poll** - Check game state every 100ms via GameAPI
2. **Detect** - Compare current state with tracked entities
3. **Emit** - Fire events for state changes (launches, arrivals, completions)

## Trackers

### NukeTracker
**File:** `nuke-tracker.ts` (253 lines)

Monitors nuclear weapons (atom bombs, hydrogen bombs, MIRVs).

**Events Emitted:**
- `NUKE_LAUNCHED` / `HYDRO_LAUNCHED` / `MIRV_LAUNCHED` - When nuke is fired
- `ALERT_ATOM` / `ALERT_HYDRO` / `ALERT_MIRV` - When incoming nuke detected
- `NUKE_EXPLODED` - When nuke reaches target
- `NUKE_INTERCEPTED` - When nuke is destroyed before impact

**Features:**
- Tracks up to 32 simultaneous nukes
- Distinguishes incoming vs outgoing nukes
- Deduplicates launch events by unitID

### BoatTracker
**File:** `boat-tracker.ts` (168 lines)

Monitors naval invasions.

**Events Emitted:**
- `ALERT_NAVAL` - When incoming boat attack detected

**Features:**
- Tracks boats by unitID
- Detects new boats every poll cycle
- Identifies retreating boats

### LandAttackTracker
**File:** `land-tracker.ts` (159 lines)

Monitors ground invasions.

**Events Emitted:**
- `ALERT_LAND` - When incoming ground attack detected

**Features:**
- Uses player.incomingAttacks() API
- Tracks by attackID
- Handles attack cancellations and completions

## Usage

```typescript
import { NukeTracker, BoatTracker, LandAttackTracker } from './trackers'

const nukeTracker = new NukeTracker()
nukeTracker.onEvent((event) => {
  console.log('Nuke event:', event.type, event.data)
})

// Poll game state every 100ms
const gameAPI = createGameAPI()
setInterval(() => {
  nukeTracker.poll(gameAPI)
}, 100)
```

## Architecture

All trackers implement the same event pattern:

```typescript
class Tracker {
  private eventCallbacks: Array<(event: GameEvent) => void> = []
  
  onEvent(callback: (event: GameEvent) => void) {
    this.eventCallbacks.push(callback)
  }
  
  poll(gameAPI: GameAPI) {
    // 1. Get current game state
    // 2. Compare with tracked entities
    // 3. Emit events for changes
    // 4. Update tracked entities
  }
}
```

## Performance

- **Memory**: Each tracker maintains a `Map<unitID, TrackedEntity>`
- **CPU**: O(n) scan of active units every 100ms
- **Deduplication**: Prevents duplicate events for same entity

## Related

- **GameAPI** (`../game-api.ts`) - Safe wrapper for game state access
- **GameBridge** (`../openfront-bridge.ts`) - Orchestrates all trackers
- **Protocol** (`../../../../ots-shared/src/game.ts`) - Event type definitions
