# Game End Screen Implementation

## Overview

The game end screen provides visual feedback on the LCD display showing whether the player won or lost the game. This feature integrates game outcome detection in the userscript with LCD display logic in the firmware.

## Architecture

### Event Flow

```
OpenFront.io Game
    ↓ (game ends)
Userscript GameAPI.didPlayerWin()
    ↓ (checks myPlayer.eliminated())
OpenFront Bridge
    ↓ (sends WIN/LOOSE/GAME_END events)
OTS Server
    ↓ (relays WebSocket events)
OTS Firmware
    ↓ (event_dispatcher)
System Status Module
    ↓ (displays outcome)
LCD Display: "VICTORY!" or "DEFEAT"
```

## Components

### 1. Userscript - Game Outcome Detection

**File**: `ots-userscript/src/game/game-api.ts`

Added `didPlayerWin()` method to GameAPI:

```typescript
didPlayerWin(): boolean | null {
  try {
    const game = (window as any).game
    if (!game) return null

    const myPlayer = game.myPlayer()
    if (!myPlayer) return null

    // Check if game is over
    const gameOver = game.gameOver()
    if (!gameOver) return null

    // If player is eliminated, they lost
    if (myPlayer.eliminated()) {
      return false
    }

    // If game is over and player is not eliminated, they won
    return true
  } catch (error) {
    console.error('[GameAPI] Error checking win status:', error)
    return null
  }
}
```

**How it works**:
- Returns `true` if game is over and player is NOT eliminated (victory)
- Returns `false` if player is eliminated (defeat)
- Returns `null` if game is not over or status cannot be determined

### 2. Userscript - Event Sending

**File**: `ots-userscript/src/game/openfront-bridge.ts`

Updated game end detection:

```typescript
private handleGameEnd() {
  const didWin = this.gameAPI.didPlayerWin()
  
  if (didWin === true) {
    // Player won
    this.ws.sendEvent('WIN', 'Victory!', { result: 'victory' })
  } else if (didWin === false) {
    // Player lost
    this.ws.sendEvent('LOOSE', 'Defeat', { result: 'defeat' })
  } else {
    // Cannot determine outcome, send generic game end
    this.ws.sendEvent('GAME_END', 'Game ended', { result: 'unknown' })
  }
}
```

**Events sent**:
- `WIN`: Player won the game
- `LOOSE`: Player lost the game (note: protocol uses "LOOSE" not "LOSE")
- `GAME_END`: Fallback when outcome cannot be determined

### 3. Firmware - System Status Module

**File**: `ots-fw-main/src/system_status_module.c`

Added game end screen state and display logic:

```c
typedef struct {
    system_status_screen_t current_screen;
    uint64_t last_update;
    bool show_game_end;        // Show victory/defeat screen
    bool player_won;           // True = victory, false = defeat
    uint64_t game_end_time;    // When game ended (for timeout)
} system_status_state_t;
```

**Display Function**:

```c
static void display_game_end(bool victory) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    if (victory) {
        lcd_write_string("   VICTORY!     ");
    } else {
        lcd_write_string("    DEFEAT      ");
    }
    lcd_set_cursor(0, 1);
    lcd_write_string(" Good Game!     ");
}
```

**Event Handlers**:

```c
case GAME_EVENT_GAME_END:
  // Expected JSON in event->data: {"victory": true|false|null}
  // Victory/defeat screens persist until userscript disconnect/reconnect.
  break;
```

**Update Logic**:

```c
static void system_status_update(void) {
  // No timeout: if show_game_end is set, keep showing it.
  // The end screen clears when the userscript disconnects/reconnects.
}
```

## Display Format

### Victory Screen
```
┌────────────────┐
│   VICTORY!     │
│ Good Game!     │
└────────────────┘
```

### Defeat Screen
```
┌────────────────┐
│    DEFEAT      │
│ Good Game!     │
└────────────────┘
```

## Configuration

**Display Duration**: No timeout
- End screen persists until userscript disconnect/reconnect resets state

## Testing

### Manual Testing

1. **Victory Test**:
   - Play OpenFront.io game until you win
  - Verify userscript sends `GAME_END` with `data.victory: true`
  - Verify firmware receives event (check serial monitor)
  - Verify LCD displays "VICTORY!" and persists

2. **Defeat Test**:
   - Play OpenFront.io game until you lose
  - Verify userscript sends `GAME_END` with `data.victory: false`
  - Verify firmware receives event (check serial monitor)
  - Verify LCD displays "DEFEAT" and persists

3. **Fallback Test**:
   - Simulate game end with unknown outcome
   - Verify `GAME_END` event sent
   - Verify LCD doesn't show victory/defeat screen

### Debug Logging

**Userscript** (browser console):
```
[OpenFront Bridge] Game ended
[GameAPI] Player won: true
[WebSocket] Sending event: GAME_END
```

**Firmware** (serial monitor):
```
I (12345) system_status: Game ended - VICTORY!
```

## Protocol Reference

**Event Types** (from `prompts/protocol-context.md`):
- `GAME_EVENT_GAME_END` (firmware) / `GAME_END` (WebSocket): Game ended (outcome indicated by `data.victory`)

**Event Data Format**:
```json
{
  "type": "event",
  "payload": {
    "type": "WIN",
    "timestamp": 1234567890,
    "message": "Victory!",
    "data": {
      "result": "victory"
    }
  }
}
```

## Future Enhancements

1. **Extended Statistics**:
   - Show score/kills/deaths on game end screen
   - Multi-line scrolling display for detailed stats

2. **Animations**:
   - Victory: Flashing/blinking text
   - Defeat: Dimmed/slow fade effect

3. **Sound Effects**:
   - Victory fanfare via buzzer
   - Defeat tone

4. **Configurable Display Time**:
   - User-adjustable timeout via WebSocket command
   - Persist setting in NVS storage

5. **Game Summary**:
   - Total troops deployed
   - Territories captured
   - Time survived

## Related Files

- `ots-userscript/src/game/game-api.ts`: Game outcome detection
- `ots-userscript/src/game/openfront-bridge.ts`: Event sending logic
- `ots-fw-main/src/system_status_module.c`: Display logic
- `ots-fw-main/include/system_status_module.h`: Module interface
- `protocol-context.md`: Event protocol definitions

## Build Information

**Userscript Build**: ✅ Success
**Firmware Build**: ✅ Success (7.05 seconds)
**Flash Usage**: 49.0% (1,028,584 bytes)
**RAM Usage**: 12.6% (41,340 bytes)

---

**Implementation Date**: December 19, 2025
**Last Updated**: December 19, 2025
