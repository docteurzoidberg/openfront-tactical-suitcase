# LCD Display Modes - OTS Firmware

## Overview

The OTS firmware uses a **16×2 character LCD** to display system status, connection information, and game state. The LCD is shared between two modules:

1. **System Status Module** - Owns display by default (boot, connection, lobby, game end)
2. **Troops Module** - Takes control during active gameplay (`GAME_PHASE_IN_GAME`)

This document describes the display modes, state transitions, and implementation details.

## Display Modes

### 1. SPLASH (Boot Screen)

**Controller**: System Status Module  
**Phase**: Firmware startup  
**Duration**: Persists until network mode is determined

```
┌────────────────┐
│  OTS Firmware  │
│  Booting...    │
└────────────────┘
```

**Purpose**: 
- Initial boot screen shown immediately on module initialization
- Acts as a "holding screen" until firmware determines network mode
- Prevents premature display of "Waiting for Connection" before WebSocket server is ready

**Display Logic**:
```c
// In system_status_update():
// Splash persists until either:
// 1. Portal mode activated (no WiFi credentials)
// 2. WebSocket server started (normal STA mode)

if (!network_manager_is_portal_mode() && !ws_server_is_started()) {
    // Keep splash screen, don't mark display_dirty as done
    return;
}
```

**LED Status**: OFF (disconnected)

---

### 2. CAPTIVE_PORTAL (WiFi Setup)

**Controller**: System Status Module  
**Phase**: Captive portal mode (no WiFi credentials stored)  
**Duration**: Until WiFi credentials are configured via web interface

```
┌────────────────┐
│   Setup WiFi   │
│  Read Manual   │
└────────────────┘
```

**Purpose**:
- Shown when device has no WiFi credentials (first boot or after WiFi clear)
- Directs user to connect to AP and configure WiFi via captive portal
- Static display (NO connection animation)

**Display Logic**:
```c
// In system_status_update():
bool in_portal_mode = network_manager_is_portal_mode();

if (in_portal_mode) {
    display_captive_portal();  // "Setup WiFi / Read Manual"
    return;
}
```

**Captive Portal Details**:
- AP SSID: `OTS-SETUP-XXXX` (where XXXX = last 4 hex digits of MAC)
- Web Interface: http://192.168.4.1
- User configures WiFi credentials
- Device reboots after configuration

**LED Status**: Blue (WIFI_CONNECTING state)

---

### 3. WAITING_CLIENT (Waiting for Connection)

**Controller**: System Status Module  
**Phase**: WebSocket server running, no client connected  
**Duration**: Until WebSocket client connects

```
┌────────────────┐
│ Waiting for    │
│ Connection...  │ (animated scan dot)
└────────────────┘
```

**Purpose**: 
- Indicates device is ready and waiting for userscript/dashboard connection
- Shows animated "scan" effect to indicate active waiting state
- **Only shown when WebSocket server is actually running**

**Display Logic**:
```c
// In system_status_update():
bool wss_started = ws_server_is_started();

if (!wss_started) {
    // Keep splash screen until WSS server is ready
    return;
}

if (!module_state.ws_connected) {
    display_waiting_connection();  // Show animated waiting screen
    return;
}
```

**Animation**: 
- Scan dot moves across last 3 columns every ~250ms
- Frames: `".  "` → `" . "` → `"  ."` → `" . "` (repeat)
- Only animates when NOT in portal mode

**LED Status**: Yellow (WIFI_ONLY state - WiFi connected, no WebSocket)

---

### 4. LOBBY (Connected, Waiting for Game)

**Controller**: System Status Module  
**Phase**: `GAME_PHASE_LOBBY`  
**Duration**: Until game starts

```
┌────────────────┐
│ Connected!     │
│ Waiting Game...│ (animated scan dot)
└────────────────┘
```

**Purpose**:
- Confirms WebSocket connection is active
- Indicates system is ready and waiting for game to start
- Provides visual feedback via animated scan dot

**LED Status**: Purple (USERSCRIPT_CONNECTED - WebSocket active, no game)

---

### 5. IN_GAME (Troop Display)

**Controller**: Troops Module  
**Phase**: `GAME_PHASE_IN_GAME`  
**Duration**: Entire game duration

```
┌────────────────┐
│    120K / 1.1M │  (right-aligned troop counts)
│ 50% (60K)      │  (left-aligned slider + calculated)
└────────────────┘
```

**Purpose**:
- Real-time troop management during active gameplay
- Shows current/max troop counts with intelligent unit scaling
- Displays deployment slider percentage and calculated troops

**Update Triggers**:
- Troop data messages from server → Update Line 1 & 2
- Slider position changes ≥1% → Update Line 2

**LED Status**: Green (GAME_STARTED - active gameplay)

---

### 6. GAME_END (Victory/Defeat Screens)

**Controller**: System Status Module  
**Phase**: `GAME_PHASE_WON` / `GAME_PHASE_LOST`  
**Duration**: ~5 seconds, then auto-return to LOBBY

**Victory Screen**:
```
┌────────────────┐
│   VICTORY!     │
│ Good Game!     │
└────────────────┘
```

**Defeat Screen**:
```
┌────────────────┐
│    DEFEAT      │
│ Good Game!     │
└────────────────┘
```

**Purpose**: Provides clear end-game feedback before returning to lobby

---

## State Transition Diagram

```
                    [DEVICE BOOT]
                          │
                          ↓
                   [MODULE INIT]
                          │
                          ├─→ Splash Screen
                          │   "OTS Firmware / Booting..."
                          │   (LED: OFF)
                          │
                          ↓
              [NETWORK MODE DETERMINATION]
                          │
        ├─────────────────┴──────────────────┐
        │                                     │
        ↓                                     ↓
[NO WIFI CREDENTIALS]                [WIFI CREDENTIALS]
        │                                     │
        ├─→ Start Captive Portal              ├─→ Connect to WiFi
        │                                     │
        ↓                                     ↓
   CAPTIVE_PORTAL                      [START WSS SERVER]
   "Setup WiFi /                             │
    Read Manual"                             │
   (LED: Blue)                               ↓
        │                              WAITING_CLIENT
        │                              "Waiting for /
        │                               Connection..."
        │                              (LED: Yellow, animated)
        │                                     │
        │                                     ↓
        │                            [WS CLIENT CONNECTS]
        │                                     │
        └─────────────────┬───────────────────┘
                          │
                          ↓
                       LOBBY
                  "Connected! /
                   Waiting Game..."
                  (LED: Purple)
                          │
                          ↓
                  [GAME STARTS]
                          │
                          ↓
                      IN_GAME
                  "120K / 1.1M"
                  "50% (60K)"
               (Troops Module Control)
                  (LED: Green)
                          │
                          ↓
                   [GAME ENDS]
                          │
        ├─────────────────┼──────────────────┐
        │                 │                   │
        ↓                 ↓                   ↓
    VICTORY           DEFEAT              [TIMEOUT]
  "VICTORY! /      "DEFEAT /                 │
   Good Game!"      Good Game!"              │
        │                 │                   │
        └─────────────────┴───────────────────┘
                          │
                          ↓
                  [AFTER 5s TIMEOUT]
                          │
                          ↓
                       LOBBY
                  (repeat cycle)

[CONNECTION LOSS EVENTS]
    │
    ├─→ WS Disconnect → WAITING_CLIENT (Yellow LED)
    └─→ WiFi Disconnect → Reconnect → WAITING_CLIENT
```

**Key Decision Points:**

1. **Splash → Portal or Waiting**:
   - Splash persists until network mode determined
   - Portal mode: Show "Setup WiFi" (blue LED, no animation)
   - Normal mode: Wait for WSS server, then show "Waiting for Connection" (yellow LED, animated)

2. **Display Logic Priority** (in `system_status_update()`):
   ```c
   // Check portal mode FIRST
   if (network_manager_is_portal_mode()) {
       display_captive_portal();
       return;
   }
   
   // Then check if WSS server is ready
   if (!ws_server_is_started()) {
       // Keep splash screen
       return;
   }
   
   // Then check WebSocket connection
   if (!module_state.ws_connected) {
       display_waiting_connection();
       return;
   }
   
   // Finally show game phase screens
   switch (game_phase) { ... }
   ```

3. **Animation Control**:
   - Splash: No animation (static)
   - Captive Portal: No animation (static)
   - Waiting Client: Animated scan dot (only when WSS running, not in portal mode)
   - Lobby: Animated scan dot
   - In-Game: Real-time troop updates
   - Game End: No animation (static)

---

## Event-to-Mode Transitions
Both modules listen for events to coordinate LCD control:

| Event Type | From Mode | To Mode | Controller | Description |
|------------|-----------|---------|------------|-------------|
| Module init | - | SPLASH | System Status | Initial boot screen, persists until network ready |
| Portal activated | SPLASH | CAPTIVE_PORTAL | System Status | No WiFi credentials, start AP mode |
| WSS started | SPLASH | WAITING_CLIENT | System Status | WebSocket server ready, waiting for clients |
| `INTERNAL_EVENT_WS_CONNECTED` | WAITING_CLIENT | LOBBY | System Status | WebSocket connection established |
| `INTERNAL_EVENT_WS_DISCONNECTED` | Any | WAITING_CLIENT | System Status | WebSocket connection lost (returns to waiting) |
| `GAME_EVENT_GAME_START` | LOBBY | IN_GAME | Troops Module | Game started, show troop display |
| `GAME_EVENT_GAME_END` | IN_GAME | LOBBY | System Status | Game ended (System Status reclaims control) |
| `GAME_EVENT_WIN` | IN_GAME | VICTORY | System Status | Player won |
| `GAME_EVENT_LOOSE` | IN_GAME | DEFEAT | System Status | Player lost |
| Timeout (5s) | VICTORY/DEFEAT | LOBBY | System Status | Auto-return to lobby |

**Critical Event Handling:**

1. **Portal Mode**: Checked via `network_manager_is_portal_mode()` function call (not event-based)
2. **WSS Server**: Checked via `ws_server_is_started()` function call (not event-based)
3. **WebSocket Connection**: Event-driven via `INTERNAL_EVENT_WS_CONNECTED/DISCONNECTED`
4. **Game Phase**: Event-driven via `GAME_EVENT_*` events

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
        // *** CRITICAL: Check portal mode FIRST ***
        bool in_portal_mode = network_manager_is_portal_mode();
        
        if (in_portal_mode) {
            // Captive portal mode - show setup screen
            display_captive_portal();  // "Setup WiFi / Read Manual"
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // *** CRITICAL: Check if WSS server is ready ***
        bool wss_started = ws_server_is_started();
        
        if (!wss_started) {
            // Keep splash screen until server is running
            // Don't mark display_dirty as done - splash persists
            return ESP_OK;
        }
        
        // *** CRITICAL: Check connection status BEFORE game phase ***
        if (!module_state.ws_connected) {
            // WSS server running but no client connected
            display_waiting_connection();  // "Waiting for / Connection..."
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // WebSocket connected - show screen based on game phase
        game_phase_t phase = game_state_get_phase();
        
        // Show game end screen if flag is set
        if (module_state.show_game_end) {
            display_game_end(module_state.player_won);
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        switch (phase) {
            case GAME_PHASE_LOBBY:
            case GAME_PHASE_SPAWNING:
                display_lobby();  // "Connected! / Waiting Game..."
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
    
    // Handle connection animation (only when not in portal mode and not connected)
    if (module_state.display_active && !module_state.ws_connected && !network_manager_is_portal_mode()) {
        // Animate scan dot on "Waiting for Connection" screen
        animate_connection_scan();
    }
    
    return ESP_OK;
}
```

**Key Implementation Points:**

1. **Query-Based Checks**: Portal mode and WSS server status are checked via function calls, not events
2. **Splash Persistence**: When WSS server isn't ready, function returns WITHOUT marking `display_dirty = false`, keeping splash visible
3. **Priority Order**: Portal → WSS Ready → Connection → Game Phase
4. **Animation Control**: Only animate when display_active AND not connected AND not in portal mode
5. **Display Refresh**: `system_status_refresh_display()` called after portal start and WSS start to trigger re-evaluation

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
- **Symptom**: Screen remains on one mode regardless of state changes
- **Cause**: `display_dirty` flag not being set or update function not called
- **Check**: 
  - `module_state.display_dirty` flag set on events
  - `system_status_update()` or `troops_update()` being called in main loop
  - LCD driver initialization (`lcd_init()` in main)
  - I2C bus working (LCD at 0x27, ADC at 0x48)
- **Fix**: Verify event handlers set `display_dirty = true`, check main loop execution

### Animation not working
- **Symptom**: "Waiting for Connection" or "Waiting Game" scan dot not moving
- **Cause**: Animation logic not executing
- **Check**:
  - Animation only runs when `display_active && !ws_connected && !portal_mode`
  - Timer for animation updates (~250ms intervals)
  - Portal mode does NOT animate (intentional)
- **Fix**: Verify animation conditions, check timer implementation

### Wrong screen in portal mode
- **Symptom**: Shows "Waiting for Connection" instead of "Setup WiFi"
- **Cause**: Portal mode check not being performed
- **Check**:
  - `network_manager_is_portal_mode()` called in update logic
  - Portal mode checked BEFORE other display logic
  - `display_captive_portal()` function implementation
- **Fix**: Verify display logic priority order, check portal mode function

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
