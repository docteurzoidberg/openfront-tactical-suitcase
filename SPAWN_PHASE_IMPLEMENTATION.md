# Spawn Phase Detection Implementation

## Overview

Added support for detecting the **spawning phase** in OpenFront.io - the period between when a player spawns on the map and when the spawn countdown ends and gameplay actually begins.

## Motivation

Previously, the system only detected two states:
1. **LOBBY**: Before player spawns
2. **IN_GAME**: After player spawns

This meant the LCD would immediately show troop counts even during the spawn countdown, which was confusing. Now we have:

1. **LOBBY**: Before player spawns
2. **SPAWNING**: Player spawned, countdown active (game.started() = false)
3. **IN_GAME**: Countdown ended, gameplay active (game.started() = true)

## Changes Made

### 1. Userscript (`ots-userscript`)

**File: `src/game/game-api.ts`**
- Added `isGameStarted()` method to GameAPI interface
- Implementation checks `game.started()` method (if available)
- Fallback to `game.ticks() > 0` if started() method doesn't exist

```typescript
isGameStarted(): boolean | null {
  if (typeof game.started === 'function') {
    return game.started()
  }
  const ticks = game.ticks()
  return ticks !== null && ticks > 0
}
```

**File: `src/game/openfront-bridge.ts`**
- Added `inSpawning` flag to track spawning phase
- Modified game phase detection logic:
  - Detects SPAWNING when `myPlayerID` exists but `isGameStarted() === false`
  - Emits `GAME_SPAWNING` event when entering spawn phase
  - Emits `GAME_START` event when countdown ends (transition to actual gameplay)

**Build:**
```bash
cd ots-userscript && npm run build
```
✅ **Status: Complete** - Userscript built successfully

### 2. Shared Types (`ots-shared`)

**File: `src/game.ts`**
- Added `'GAME_SPAWNING'` to `GameEventType` union type

```typescript
export type GameEventType = 
  | 'INFO' 
  | 'GAME_SPAWNING'  // NEW
  | 'GAME_START'
  | 'GAME_END'
  // ... rest
```

**Note:** The `GamePhase` type already included `'spawning'` but it wasn't being used.

### 3. Server (`ots-server`)

**File: `app/composables/useGameSocket.ts`**
- Added event handler for `GAME_SPAWNING`
- Sets `gamePhase.value = 'spawning'` when spawn phase begins

```typescript
case 'GAME_SPAWNING':
  gamePhase.value = 'spawning'
  break
```

**File: `app/components/hardware/LCDDisplay.vue`**
- Added spawning screen display:
  - Line 1: `"   Spawning...  "`
  - Line 2: `" Get Ready!     "`

```vue
<div v-else-if="phase === 'spawning'" class="lcd-content">
  <div class="lcd-line">   Spawning...  </div>
  <div class="lcd-line"> Get Ready!     </div>
</div>
```

### 4. Firmware (`ots-fw-main`)

**File: `include/protocol.h`**
- Added `GAME_EVENT_GAME_SPAWNING` to event type enum (after `GAME_EVENT_INFO`)

```c
typedef enum {
    GAME_EVENT_INFO = 0,
    GAME_EVENT_GAME_SPAWNING,  // NEW
    GAME_EVENT_GAME_START,
    // ... rest
```

**File: `src/protocol.c`**
- Added string conversion for `GAME_SPAWNING` event
- Updated `event_type_to_string()`: Added case for `GAME_EVENT_GAME_SPAWNING → "GAME_SPAWNING"`
- Updated `string_to_event_type()`: Added case for `"GAME_SPAWNING" → GAME_EVENT_GAME_SPAWNING`

**File: `src/game_state.c`**
- Added handler for `GAME_EVENT_GAME_SPAWNING`
- Transitions to `GAME_PHASE_SPAWNING` when event received

```c
case GAME_EVENT_GAME_SPAWNING:
    new_phase = GAME_PHASE_SPAWNING;
    break;
```

**Build:**
```bash
cd ots-fw-main && pio run
```
✅ **Status: Complete** - Firmware built successfully (1028660 bytes, 49.1% flash)

### 5. Hardware Display

The LCD will now show different screens based on game phase:

**SPAWNING Phase:**
```
   Spawning...  
 Get Ready!     
```

**IN_GAME Phase:**
```
 120K / 1.1M    
50% (60K)       
```

## Testing Steps

### 1. Install Updated Userscript
```bash
# The built userscript is at:
ots-userscript/build/userscript.ots.user.js

# Install in Tampermonkey by:
# - Opening the file in browser, or
# - Drag-and-drop into Tampermonkey dashboard
```

### 2. Flash Updated Firmware (Optional)
```bash
cd ots-fw-main
pio run -t upload
pio device monitor
```

### 3. Test Spawn Phase Detection

1. **Join a game on OpenFront.io**
2. **Expected behavior:**
   - When player spawns → `GAME_SPAWNING` event sent
   - LCD shows "Spawning... / Get Ready!"
   - Server dashboard shows spawning screen
   - When countdown ends → `GAME_START` event sent
   - LCD transitions to troop display
   - Gameplay begins

3. **Check Serial Output (Firmware):**
```
[OTS_GAME_STATE] Game phase changed: LOBBY -> SPAWNING
[OTS_GAME_STATE] Game phase changed: SPAWNING -> IN_GAME
```

4. **Check Browser Console (Userscript):**
```
[GameBridge] Spawn countdown active
[WS] Sent GAME_SPAWNING event
[GameBridge] Game started - countdown ended
[WS] Sent GAME_START event
```

## Event Flow

```
User joins game
    ↓
Player spawns on map (game.myPlayer() exists)
    ↓
[SPAWNING PHASE BEGINS]
    ↓
Userscript checks: game.started() === false
    ↓
Emits: GAME_SPAWNING event
    ↓
Firmware: Transitions to GAME_PHASE_SPAWNING
LCD Display: "Spawning... / Get Ready!"
Server Dashboard: Shows spawning screen
    ↓
[SPAWN COUNTDOWN ACTIVE - 5-10 seconds]
    ↓
Countdown ends
    ↓
Userscript checks: game.started() === true
    ↓
Emits: GAME_START event
    ↓
[IN_GAME PHASE BEGINS]
    ↓
Firmware: Transitions to GAME_PHASE_IN_GAME
LCD Display: Shows troop counts "120K / 1.1M"
Server Dashboard: Shows troop module
Troop monitoring: Begins sending troop count updates
```

## Game API Methods Used

### Primary Method: `game.started()`
- **Returns:** `boolean` - true when countdown ends and game truly starts
- **Availability:** Should be available in OpenFront.io game object
- **Purpose:** Distinguishes spawn phase from actual gameplay

### Fallback Method: `game.ticks() > 0`
- **Returns:** `boolean` - true when game ticks start incrementing
- **Purpose:** Backup detection if `started()` method doesn't exist
- **Note:** May not be as accurate as `started()` method

## Protocol Specification Update Needed

The following file should be updated to document the new event:

**`/protocol-context.md`**
- Add `GAME_SPAWNING` event definition
- Document event data structure
- Update event flow diagrams

**Suggested addition:**
```markdown
### GAME_SPAWNING Event

**Purpose:** Indicates player has spawned but spawn countdown is still active.

**Direction:** Userscript → Server → Firmware

**Timing:** Sent immediately when player spawns on map (before gameplay starts)

**Data:**
```json
{
  "type": "event",
  "payload": {
    "type": "GAME_SPAWNING",
    "timestamp": 1234567890,
    "message": "Spawn countdown active",
    "data": {
      "spawning": true
    }
  }
}
```

**State Transitions:**
- **Before:** `GAME_PHASE_LOBBY`
- **After:** `GAME_PHASE_SPAWNING`
- **Next:** `GAME_PHASE_IN_GAME` (when `GAME_START` event received)
```

## Future Enhancements

1. **Countdown Timer Display**
   - Parse spawn countdown time from game
   - Display on LCD: "Spawning (5s)"
   - Update every second

2. **Spawn Position Info**
   - Extract spawn location from game
   - Display territory name on LCD

3. **Team Info During Spawn**
   - Show team composition
   - Display allied player count

## Notes

- The `GamePhase.spawning` type was already defined but never used
- The `GAME_PHASE_SPAWNING` firmware enum was already defined but never transitioned to
- This implementation activates the existing infrastructure
- No breaking changes to existing protocol
- Backwards compatible (old userscripts will still work, just skip spawning phase)

## Warnings

When building firmware, you may see these warnings:

```
Warning! Flash memory size mismatch detected. Expected 8MB, found 2MB!
```
- **Not critical** - Firmware still works, just configured for larger flash

```
warning: passing argument 1 of 'module_manager_register' discards 'const' qualifier
```
- **Not critical** - Just a const correctness warning, doesn't affect functionality

## Completion Status

✅ **Userscript** - Spawn detection implemented and built  
✅ **Shared Types** - GAME_SPAWNING added to event types  
✅ **Server** - Event handler and LCD spawning screen implemented  
✅ **Firmware** - Protocol and game state updated, built successfully  
⏳ **Testing** - Needs real-world testing in OpenFront.io game  
⏳ **Documentation** - protocol-context.md needs updating  

---

**Implementation Date:** January 2025  
**Components Modified:** 8 files across 4 projects  
**Build Status:** All successful  
**Ready for Testing:** Yes
