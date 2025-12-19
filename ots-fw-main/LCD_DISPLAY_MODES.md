# LCD Display Modes - Troops Module

## Overview

The troops module now implements intelligent display mode switching to provide contextual information about the system state. Instead of always showing troop counts, the LCD displays meaningful messages during boot, connection, and lobby phases.

## Display Modes

### 1. WAITING_CLIENT Mode
**Trigger:** Boot / WebSocket disconnected  
**Event:** `INTERNAL_EVENT_WS_DISCONNECTED`

```
┌────────────────┐
│ Waiting for    │
│ Connection...  │
└────────────────┘
```

**When shown:**
- System just booted up
- WebSocket connection to server lost
- Waiting for userscript to connect

**Purpose:** Clearly indicates the system is operational but not connected to the game server.

---

### 2. LOBBY Mode
**Trigger:** WebSocket connected / Game ended  
**Events:** `INTERNAL_EVENT_WS_CONNECTED`, `GAME_EVENT_GAME_END`, `GAME_EVENT_WIN`, `GAME_EVENT_LOOSE`

```
┌────────────────┐
│ Connected!     │
│ Waiting Game...│
└────────────────┘
```

**When shown:**
- WebSocket successfully connected to server
- Game has ended (win/loss/disconnect)
- Waiting for next game to start

**Purpose:** Confirms connection is active and system is ready, waiting for game to begin.

---

### 3. IN_GAME Mode
**Trigger:** Game started  
**Event:** `GAME_EVENT_GAME_START`

```
┌────────────────┐
│    120K / 1.1M │  (right-aligned troop counts)
│ 50% (60K)      │  (left-aligned slider + calculated)
└────────────────┘
```

**When shown:**
- Active game in progress
- Troop data available from server

**Purpose:** Real-time troop management display with current/max counts and deployment slider.

---

## State Transitions

```
        BOOT
          │
          ▼
  ┌─────────────────┐
  │ WAITING_CLIENT  │◄────────┐
  └─────────────────┘         │
          │                   │
    WS CONNECTED         WS DISCONNECTED
          │                   │
          ▼                   │
     ┌────────┐               │
     │ LOBBY  │───────────────┘
     └────────┘
          │      ▲
    GAME START   │
          │      │ GAME END/WIN/LOSS
          ▼      │
     ┌─────────┐ │
     │ IN_GAME │─┘
     └─────────┘
```

## Event Handlers

The troops module listens for these events and switches display modes accordingly:

| Event Type | From Mode | To Mode | Description |
|------------|-----------|---------|-------------|
| `INTERNAL_EVENT_WS_CONNECTED` | WAITING_CLIENT | LOBBY | WebSocket connection established |
| `INTERNAL_EVENT_WS_DISCONNECTED` | Any | WAITING_CLIENT | WebSocket connection lost |
| `GAME_EVENT_GAME_START` | LOBBY | IN_GAME | Game started, show troop display |
| `GAME_EVENT_GAME_END` | IN_GAME | LOBBY | Game ended normally |
| `GAME_EVENT_WIN` | IN_GAME | LOBBY | Player won |
| `GAME_EVENT_LOOSE` | IN_GAME | LOBBY | Player lost |

## Implementation Details

### Data Structures

```c
typedef enum {
    DISPLAY_MODE_WAITING_CLIENT,  // Boot/disconnected
    DISPLAY_MODE_LOBBY,           // Connected but no game
    DISPLAY_MODE_IN_GAME          // Active game
} display_mode_t;

static display_mode_t current_display_mode = DISPLAY_MODE_WAITING_CLIENT;
```

### Display Functions

- `display_waiting_client()` - Shows "Waiting for Connection..."
- `display_lobby()` - Shows "Connected! Waiting Game..."
- `display_in_game()` - Shows troop counts and slider (existing logic)

### Mode-Aware Update

```c
static void update_display(void) {
    if (!module_state.display_dirty) return;
    
    switch (current_display_mode) {
        case DISPLAY_MODE_WAITING_CLIENT:
            display_waiting_client();
            break;
        case DISPLAY_MODE_LOBBY:
            display_lobby();
            break;
        case DISPLAY_MODE_IN_GAME:
            display_in_game();
            break;
    }
    
    module_state.display_dirty = false;
}
```

## Benefits

### User Experience
- **Clear feedback:** Users always know the system state
- **No confusion:** "0 / 0" troop display no longer shown when not in game
- **Connection status:** Immediate visual indication of connection problems

### Debugging
- **Boot sequence:** Can verify system boots and attempts connection
- **Connection issues:** Easily identify WebSocket connectivity problems
- **Game flow:** Track game lifecycle (lobby → in-game → lobby)

### Professional Polish
- **Purposeful UI:** Every screen provides relevant information
- **State awareness:** Display adapts to context
- **Reduced cognitive load:** Users don't need to interpret "0/0" as "not connected"

## Testing

### Manual Test Scenarios

1. **Boot Sequence:**
   - Power on device
   - Verify "Waiting for Connection..." appears
   - Start userscript/server
   - Verify transition to "Connected! Waiting Game..."

2. **Game Lifecycle:**
   - Join game in browser
   - Verify transition to troop display (IN_GAME mode)
   - End game (win/loss)
   - Verify return to "Connected! Waiting Game..."

3. **Connection Loss:**
   - During game, stop server or userscript
   - Verify transition to "Waiting for Connection..."
   - Restart server/userscript
   - Verify return to "Connected! Waiting Game..."

4. **Multiple Games:**
   - Play game → end → play again
   - Verify smooth transitions: LOBBY → IN_GAME → LOBBY → IN_GAME

### Expected Serial Logs

```
[Troops] WebSocket connected - switching to LOBBY mode
[Troops] Game started - switching to IN_GAME mode
[Troops] Troop update: 120000 / 1100000
[Troops] Game ended - switching to LOBBY mode
[Troops] WebSocket disconnected - switching to WAITING_CLIENT mode
```

## Related Systems

### Works With
- **Main Power Module:** LINK LED shows connection status
- **Event Dispatcher:** Receives WebSocket and game events
- **Game State:** Uses game phase tracking
- **WebSocket Client:** Triggered by connection state changes

### Integration Points
- Event handler in `troops_handle_event()`
- Display refresh in `update_display()`
- No changes to ADC slider logic
- No changes to troop data parsing

## Configuration

No configuration needed - works automatically based on event flow.

### Constants

```c
#define TROOPS_CHANGE_THRESHOLD 1  // Send command on ≥1% slider change
```

Display mode switching does not affect slider behavior or command sending.

## Future Enhancements

- [ ] Add "Reconnecting..." animation (scrolling text)
- [ ] Show connection strength indicator
- [ ] Display game timer in lobby mode
- [ ] Show player name/team in IN_GAME mode
- [ ] Add "Paused" mode if game pauses

## Troubleshooting

### Display stuck in WAITING_CLIENT mode
- Check WebSocket connection (Main Power LINK LED should be ON)
- Verify `INTERNAL_EVENT_WS_CONNECTED` is being dispatched
- Check serial logs for "WebSocket connected" messages

### Display stuck in LOBBY mode
- Verify game has actually started in browser
- Check `GAME_EVENT_GAME_START` event is being sent from userscript
- Review event dispatcher logs

### Display stuck in IN_GAME mode
- Check `GAME_EVENT_GAME_END` event is being dispatched
- Verify userscript detects game end correctly
- May need to restart firmware if event was missed

### Display never updates
- Check `module_state.display_dirty` flag is set on events
- Verify `update_display()` is called in `troops_update()`
- Check LCD driver initialization

---

**File:** `src/troops_module.c`  
**Feature Added:** 2025-12-19  
**Related:** `protocol.h`, `event_dispatcher.h`, `game_state.h`
