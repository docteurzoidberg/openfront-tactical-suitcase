# LCD Display Modes - System Status & Troops Modules

## Overview

The OTS firmware implements intelligent LCD display mode switching across two modules:
- **System Status Module**: Controls LCD during boot, connection, and lobby phases
- **Troops Module**: Takes control during active gameplay to show troop counts

This division provides contextual information based on system state, with the System Status module acting as the primary display controller and yielding control to the Troops module during gameplay.

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
**Events:** `INTERNAL_EVENT_WS_CONNECTED`, `GAME_EVENT_GAME_END`  
**Controlled By:** System Status Module

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
**Controlled By:** Troops Module
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
Both modules listen for events to coordinate LCD control:

| Event Type | From Mode | To Mode | Controller | Description |
|------------|-----------|---------|------------|-------------|
| `INTERNAL_EVENT_WS_CONNECTED` | WAITING_CLIENT | LOBBY | System Status | WebSocket connection established |
| `INTERNAL_EVENT_WS_DISCONNECTED` | Any | WAITING_CLIENT | System Status | WebSocket connection lost |
| `GAME_EVENT_GAME_START` | LOBBY | IN_GAME | Troops Module | Game started, show troop display |
| `GAME_EVENT_GAME_END` | IN_GAME | LOBBY | System Status | Game ended (System Status reclaims control) |

**Note:** `WIN` and `LOOSE` events have been removed from the protocol. The system now uses `GAME_EVENT_GAME_END` with a `data.victory` field (true/false/null) to indicate game outcome.
| `GAME_EVENT_LOOSE` | IN_GAME | LOBBY | Player lost |

## Implementation Details

### Module Coordination

The System Status module owns LCD control by default and uses a `ws_connected` flag to track connection state:

```c
// system_status_module.c
typedef struct {
    bool initialized;
    bool display_active;   // Are we controlling the display?
    bool display_dirty;
    bool player_won;       // true = victory, false = defeat
    bool show_game_end;    // Show game end screen
    bool ws_connected;     // WebSocket connection status
} system_status_state_t;
```

**Critical Logic:** The System Status module checks `ws_connected` BEFORE checking game phase to ensure "Waiting for Connection..." is shown when disconnected, regardless of the game phase state.

### System Status Module Update Logic

```c
// system_status_module.c - system_status_update()
static esp_err_t system_status_update(void) {
    if (!module_state.initialized) return ESP_OK;
    
    if (module_state.display_dirty && module_state.display_active) {
        game_phase_t phase = game_state_get_phase();
        
        // Show game end screen if flag is set
        if (module_state.show_game_end) {
            display_game_end(module_state.player_won);
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // *** CRITICAL: Check connection status FIRST ***
        if (!module_state.ws_connected) {
            display_waiting_connection();
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // Connected - show screen based on game phase
        switch (phase) {
            case GAME_PHASE_LOBBY:
            case GAME_PHASE_SPAWNING:
                display_lobby();
                break;
            case GAME_PHASE_IN_GAME:
                // Yield control to troops module
                module_state.display_active = false;
                break;
            case GAME_PHASE_WON:
            case GAME_PHASE_LOST:
            case GAME_PHASE_ENDED:
                module_state.show_game_end = true;
                module_state.player_won = (phase == GAME_PHASE_WON);
                display_game_end(module_state.player_won);
                break;
            default:
                display_waiting_connection();
                break;
        }
        
        module_state.display_dirty = false;
    }
    
    return ESP_OK;
}
```

**Key Points:**
1. Connection state (`ws_connected`) is checked BEFORE game phase
2. This ensures "Waiting for Connection..." shows when disconnected, even if `game_phase` is still `LOBBY`
3. System Status yields control to Troops Module during `GAME_PHASE_IN_GAME`
4. System Status reclaims control on game end or disconnect         display_in_game();
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
- **Purposeful UI:** Every screen provides relevant infor (separate from LCD)
- **Event Dispatcher:** Routes events to both System Status and Troops modules
- **Game State:** Tracks game phase (`game_state.c`)
- **WebSocket Client:** Posts connection events (`INTERNAL_EVENT_WS_CONNECTED/DISCONNECTED`)

### Integration Points

**System Status Module:**
- `system_status_handle_event()` - Sets `ws_connected` flag on WS events
- `system_status_update()` - Checks connection state before game phase
- Controls LCD during: boot, disconnected, lobby, game end

**Troops Module:**
- `troops_handle_event()` - Receives troop data updates
- `troops_update()` - Updates display during IN_GAME phase
- Controls LCD during: active gameplay only

**Game State:**
- `game_state_update()` - Updates phase based on game events
- `game_state_get_phase()` - Used by System Status to determine screen
- Phases: `LOBBY`, `SPAWNING`, `IN_GAME`, `WON`, `LOST`, `ENDED`
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

- **Common cause:** `ws_connected` flag not being set in System Status module

### Display stuck in LOBBY mode
- Verify game has actually started in browser
- Check `GAME_EVENT_GAME_START` event is being sent from userscript
- Review event dispatcher logs
- Check `game_state_get_phase()` returns `GAME_PHASE_IN_GAME`

### Display stuck in IN_GAME mode
- Check `GAME_EVENT_GAME_END` event is being dispatched
- Verify userscript detects game end correctly (uses `game.gameOver()` API)
- Check System Status module reclaims control (`display_active = true`)
- May need to restart firmware if event was missed

### Display never updates
- Check `module_state.display_dirty` flag is set on events
- Verify `system_status_update()` or `troops_update()` is being called
- Check LCD driver initialization (`lcd_init()` in main)
- Verify I2C bus is working (LCD at 0x27, ADC at 0x48)

### "Waiting for Connection" shows even when connected
- **Root cause:** `ws_connected` flag not being set or event not received
- Check `INTERNAL_EVENT_WS_CONNECTED` is posted by WebSocket client
- Verify System Status module's event handler sets `ws_connected = true`
- Check order of operations: connection state must be checked BEFORE game phase

---

**Primary Files:** 
- `src/system_status_module.c` (Connection & lobby screens)
- `src/troops_module.c` (In-game troop display)

**Feature Added:** 2025-12-19  
**Last Updated:** 2025-12-21 (Added connection state tracking)  
**Related:** `protocol.h`, `event_dispatcher.h`, `game_state.h`, `lcd_drivers

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
