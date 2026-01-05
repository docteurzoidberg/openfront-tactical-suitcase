# OTS LCD Display Screens Specification

This document describes all LCD display screens implemented in the OTS firmware. Use this specification to replicate the exact display behavior in the `ots-simulator` dashboard emulator.

## Hardware Specifications

- **Display**: 16×2 character LCD (HD44780 compatible)
- **Interface**: I2C via PCF8574 backpack
- **I2C Address**: 0x27 (configurable to 0x3F)
- **Character Set**: ASCII
- **Dimensions**: 16 columns × 2 rows
- **Backlight**: Yellow-green LED (always on when powered)

## Display Ownership Model

The LCD is shared between two modules with a **yielding mechanism**:

1. **System Status Module**: Owns display by default
   - Boot screen
   - Connection status screens
   - Lobby screen
   - Game end screen
   - **Yields control** when game starts

2. **Troops Module**: Takes control during gameplay
   - Only active during `GAME_PHASE_IN_GAME`
   - Shows troop counts and deployment percentage
   - **Returns control** when game ends

## Screen Definitions

### 1. Splash Screen (Boot)

**Module**: System Status  
**Phase**: Startup (before network ready)  
**Duration**: Persists until captive portal starts OR WebSocket server starts

```
┌────────────────┐
│  OTS Firmware  │ (centered)
│  Booting...    │ (centered)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): "  OTS Firmware  "  // 2 spaces padding on each side
Line 2 (0,1): "  Booting...    "  // 2 spaces at start, 4 at end
```

**Display Trigger**: 
- Shown immediately on module initialization
- Persists until network enters a stable state:
  - Portal mode activated → Switch to Captive Portal screen
  - WebSocket server started (STA mode) → Switch to Waiting for Connection screen

**Important**: The splash screen acts as a "holding screen" and remains visible until the firmware knows what network mode it's operating in. This prevents premature display of "Waiting for Connection" before the WebSocket server is actually ready.

---

### 2. Captive Portal Setup Screen

**Module**: System Status  
**Phase**: Captive portal mode (no WiFi credentials stored)  
**Duration**: Until WiFi credentials are configured

```
┌────────────────┐
│   Setup WiFi   │ (centered)
│  Read Manual   │ (centered)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): "   Setup WiFi   "  // 3 spaces at start, 3 at end
Line 2 (0,1): "  Read Manual   "  // 2 spaces at start, 3 at end
```

**Display Trigger**: 
- Captive portal mode activated (no WiFi credentials)
- LED Status: Blue (WIFI_CONNECTING state)
- Shows when `network_manager_is_portal_mode()` returns true

**Notes**:
- NO connection animation on this screen (static text)
- Blue LED indicates setup/configuration mode
- User must connect to AP (SSID: "OTS-SETUP-XXXX") and configure WiFi via web interface
- After WiFi configured, device reboots and enters normal mode

---

### 3. Waiting for Connection

**Module**: System Status  
**Phase**: Network connected, WebSocket disconnected  
**Duration**: Until WebSocket connection established

```
┌────────────────┐
│ Waiting for    │ (left-aligned with leading space)
│ Connection...  │ (left-aligned with leading space)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): " Waiting for    "  // 1 space at start, 4 at end
// Line 2 is a small scan/loading animation using a SINGLE moving dot.
// Prefix stays stable: " Connection" (with padding spaces; no fixed dots)
// Dot moves across the last 3 columns every ~250ms: ".  " -> " . " -> "  ." -> " . " (repeat)
// Example frame 0:
Line 2 (0,1): " Connection   .  "
```

**Display Trigger**: `INTERNAL_EVENT_WS_DISCONNECTED` or no WebSocket connection

---

### 4. Lobby Screen (Connected, No Game)

**Module**: System Status  
**Phase**: `GAME_PHASE_LOBBY`  
**Duration**: Until game starts

```
┌────────────────┐
│ Connected!     │ (left-aligned with leading space)
│ Waiting Game.  │ (left-aligned with leading space; animated)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): " Connected!     "  // 1 space at start, 5 at end
// Line 2 is a small scan/loading animation over the last 3 columns.
// Prefix stays stable: " Waiting Game" (13 chars)
// Suffix cycles every ~250ms: ".  " -> " . " -> "  ." -> " . " (repeat)
// Example frame 0:
Line 2 (0,1): " Waiting Game.  "
```

**Display Trigger**: `INTERNAL_EVENT_WS_CONNECTED` or `GAME_PHASE_LOBBY`

---

### 5. Spawning Screen

**Module**: System Status  
**Phase**: `GAME_PHASE_SPAWNING`  
**Duration**: Until game starts

```
┌────────────────┐
│   Spawning...  │
│ Get Ready!     │
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): "   Spawning...  "  // 3 spaces at start, 2 at end
Line 2 (0,1): " Get Ready!     "  // 1 space at start, 5 at end
```

**Display Trigger**: `GAME_EVENT_GAME_SPAWNING` or `GAME_PHASE_SPAWNING`

---

### 6. Troop Display (In-Game)

**Module**: Troops  
**Phase**: `GAME_PHASE_IN_GAME`  
**Duration**: Entire game duration  
**Update Frequency**: When troop data changes or slider moves ≥1%

```
┌────────────────┐
│     120K / 1.1M│ (Line 1: right-aligned)
│50% (60K)       │ (Line 2: left-aligned)
└────────────────┘
```

**Line 1 Format**: `{current} / {max}` with intelligent unit scaling
- Right-aligned with space padding on left
- Format: `"     120K / 1.1M"` (total 16 chars)

**Line 2 Format**: `{percent}% ({calculated})`
- Left-aligned with space padding on right
- Format: `"50% (60K)       "` (total 16 chars)
- Calculated = current × (percent/100)

**Implementation**:
```c
// Line 1: Right-align troop counts
char line1[17];
snprintf(line1, 17, "%s / %s", current_str, max_str);
int padding = 16 - strlen(line1);
// Shift right and pad left with spaces

// Line 2: Left-align percentage
char line2[17];
snprintf(line2, 17, "%d%% (%s)", percent, calculated_str);
// Pad right with spaces to 16 chars
```

#### Unit Scaling Rules

Numbers are formatted with **1 decimal place** and unit suffix:

| Value Range | Format | Example |
|-------------|--------|---------|
| 0 - 999 | Raw number | `123` |
| 1,000 - 999,999 | X.XK | `1.2K`, `123.4K` |
| 1,000,000 - 999,999,999 | X.XM | `1.2M`, `123.4M` |
| 1,000,000,000+ | X.XB | `1.2B`, `123.4B` |

**Scaling Logic**:
```c
if (troops >= 1000000000) {
    billions = troops / 1000000000;
    decimal = (troops % 1000000000) / 100000000;  // First decimal place
    sprintf(buffer, "%lu.%luB", billions, decimal);
} else if (troops >= 1000000) {
    millions = troops / 1000000;
    decimal = (troops % 1000000) / 100000;
    sprintf(buffer, "%lu.%luM", millions, decimal);
} else if (troops >= 1000) {
    thousands = troops / 1000;
    decimal = (troops % 1000) / 100;
    sprintf(buffer, "%lu.%luK", thousands, decimal);
} else {
    sprintf(buffer, "%lu", troops);
}
```

**Example Values**:
- `120000` → `"120.0K"`
- `1100000` → `"1.1M"`
- `50000` → `"50.0K"`
- `1234567890` → `"1.2B"`

**Display Trigger**: 
- Initial: `GAME_EVENT_GAME_START`
- Updates: Troop data messages via WebSocket
- Slider changes ≥1%

---

### 7. Victory Screen

**Module**: System Status  
**Phase**: `GAME_PHASE_WON`  
**Duration**: Persists until userscript disconnect/reconnect resets state

```
┌────────────────┐
│   VICTORY!     │ (centered with 3 leading spaces)
│ Good Game!     │ (centered with 1 leading space)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): "   VICTORY!     "  // 3 spaces at start, 5 at end
Line 2 (0,1): " Good Game!     "  // 1 space at start, 5 at end
```

**Display Trigger**: `GAME_EVENT_GAME_END` with `data.victory: true`

---

### 8. Defeat Screen

**Module**: System Status  
**Phase**: `GAME_PHASE_LOST`  
**Duration**: Persists until userscript disconnect/reconnect resets state

```
┌────────────────┐
│    DEFEAT      │ (centered with 4 leading spaces)
│ Good Game!     │ (centered with 1 leading space)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): "    DEFEAT      "  // 4 spaces at start, 6 at end
Line 2 (0,1): " Good Game!     "  // 1 space at start, 5 at end
```

**Display Trigger**: `GAME_EVENT_GAME_END` with `data.victory: false`

---

### 9. Shutdown Screen

**Module**: System Status  
**Phase**: Device shutdown  
**Duration**: Until power off

```
┌────────────────┐
│  Shutting down │ (centered)
│                │ (blank line)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): "  Shutting down "  // 2 spaces at start, 1 at end
Line 2 (0,1): "                "  // All spaces (blank)
```

**Display Trigger**: Module shutdown call

---

## State Transition Diagram

```
   [BOOT]
     │
     ├─→ Splash Screen ("OTS Firmware / Booting...")
     │   (persists until network mode determined)
     │
     ↓
[NETWORK MODE CHECK]
     │
     ├───────────────────────┬─────────────────────────┐
     │                       │                         │
     ↓                       ↓                         ↓
[NO WIFI CREDENTIALS]   [WIFI OK]              [WIFI FAIL]
     │                       │                         │
     ├─→ Captive Portal      ├─→ Connect to WiFi      ├─→ Retry...
     │   "Setup WiFi /       │                         │
     │    Read Manual"       ↓                         ↓
     │   (Blue LED)      [START WSS SERVER]      [RECONNECT ATTEMPT]
     │                       │
     │                       ↓
     │                  [NO WS CONNECTION]
     │                       │
     │                       ├─→ "Waiting for Connection..."
     │                       │   (Yellow LED, animated)
     │                       │
     │                       ↓
     │                  [WS CONNECTED]
     │                       │
     └─────────────────────→ ├─→ Lobby Screen
                             │   "Connected! / Waiting Game..."
                             │   (Purple LED)
                             │
                             ↓
                        [GAME START]
                             │
                             ├─→ Troop Display (in-game)
                             │   "{current} / {max}"
                             │   "{percent}% ({calculated})"
                             │   (Green LED when game active)
                             │      ↓ (slider changes)
                             │      ├─→ Update Line 2
                             │      ↓ (troop data update)
                             │      └─→ Update Line 1 & 2
                              Notes |
|-------|------------------|-------|
| Module initialization | → Splash Screen | Persists until network mode determined |
| Captive portal activated | Splash → Captive Portal Setup | Blue LED, no animation, `network_manager_is_portal_mode() == true` |
| WebSocket server started | Splash → Waiting for Connection | Yellow LED, animated, requires `ws_server_is_started() == true` |
| `INTERNAL_EVENT_WS_CONNECTED` | Waiting → Lobby | Purple LED |
| `INTERNAL_EVENT_WS_DISCONNECTED` | Any → Waiting for Connection | Yellow LED, animated (only if WSS server running) |
| `GAME_EVENT_GAME_START` | Lobby → Troop Display | Green LED when game active |
| `GAME_EVENT_WIN` | Troop Display → Victory Screen | Persists until reconnect |
| `GAME_EVENT_LOOSE` | Troop Display → Defeat Screen | Persists until reconnect |
| `GAME_EVENT_GAME_END` | Troop Display → Lobby | Returns to waiting for next game |
| Game end timeout (5s) | Victory/Defeat → Lobby | Automatic return to lobby |

**Critical Implementation Details:**

1. **Splash Screen Persistence**: The splash screen remains visible until the firmware knows what network mode it's in. This prevents premature "Waiting for Connection" display before the WebSocket server is actually running.

2. **Portal Mode Check**: System Status module queries `network_manager_is_portal_mode()` before showing any other screen. If true, shows "Setup WiFi" regardless of other state.

3. **WSS Server Check**: Before showing "Waiting for Connection", module checks `ws_server_is_started()`. If false, keeps splash screen. This ensures the display accurately reflects whether the server is ready to accept connections.

4. **Connection Animation**: Only "Waiting for Connection" and "Waiting Game..." screens have animations. Captive Portal screen is static.
                             └─→ Lobby Screen (if GAME_END unknown)
                             │
                             │ (after 5s timeout)
                             │
                             ↓
                        [LOBBY]
                             │
                             └─→ Lobby Screen (repeat)

[DISCONNECTION EVENTS]
     │
     ├─→ WS Disconnect → "Waiting for Connection..." (Yellow LED)
     └─→ WiFi Disconnect → Reconnect attempt → Splash/Portal/Waiting
```

**Key Decision Points:**

1. **Boot → Network Mode**: Splash persists until either:
   - Portal mode activated (no credentials) → Show "Setup WiFi"
   - WSS server started (STA mode) → Show "Waiting for Connection"

2. **Portal vs Normal Mode**:
   - Portal: Blue LED + "Setup WiFi / Read Manual" (static, no animation)
   - Normal: Yellow LED + "Waiting for Connection..." (animated) when WSS ready

3. **Display Logic Priority** (in `system_status_update()`):
   ```c
   if (portal_mode) → "Setup WiFi"
   else if (!wss_server_started) → Keep splash
   else if (!ws_connected) → "Waiting for Connection"
   else → Show game phase screens
   ```

## Event-to-Screen Mapping

| Event | Screen Transition |
|-------|------------------|
| `INTERNAL_EVENT_NETWORK_CONNECTED` | Splash → Waiting for Connection |
| `INTERNAL_EVENT_WS_CONNECTED` | Waiting → Lobby |
| `INTERNAL_EVENT_WS_DISCONNECTED` | Any → Waiting for Connection |
| `GAME_EVENT_GAME_START` | Lobby → Troop Display |
| `GAME_EVENT_WIN` | Troop Display → Victory Screen |
| `GAME_EVENT_LOOSE` | Troop Display → Defeat Screen |
| `GAME_EVENT_GAME_END` | Troop Display → Lobby |
| Game end timeout (5s) | Victory/Defeat → Lobby |

## Protocol Messages for Display Updates

### Troop Data Update

**Incoming WebSocket Message**:
```json
{
  "type": "event",
  "payload": {
    "type": "INFO",
    "data": {
      "troops": {
        "current": 120000,
        "max": 1100000
      }
    }
  }
}
```

### Slider Value Command

**Outgoing WebSocket Message** (when slider changes ≥1%):
```json
{
  "type": "cmd",
  "payload": {
    "action": "set-troops-percent",
    "params": {
      "percent": 50
    }
  }
}
```

## Implementation Notes for ots-simulator Emulator

### Display Component Requirements

1. **16×2 Character Grid**:
   - Use monospace font (e.g., `'Courier New', monospace`)
   - Each character should be same width/height
   - Grid background: LCD backlight color (yellow-green: `#BFFF00` or `#9ACD32`)
   - Character color: Dark blue/black (`#003366` or `#000000`)

2. **Exact Text Rendering**:
   - Always render exactly 16 characters per line
   - Preserve spaces (don't trim)
   - Use exact strings from this spec

3. **Update Behavior**:
   - Clear entire display before new screen
   - No partial updates (full screen refresh)
   - Instant transitions (no animations in firmware)

4. **State Management**:
   - Track game phase state
   - Track WebSocket connection state
   - Track display ownership (system vs troops module)
  - Track game end persistence (clears on userscript disconnect/reconnect)

### Example Vue Component Structure

```vue
<template>
  <div class="lcd-display">
    <div class="lcd-line">{{ line1 }}</div>
    <div class="lcd-line">{{ line2 }}</div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch } from 'vue'

const line1 = ref('  OTS Firmware  ')
const line2 = ref('  Booting...    ')

// Update based on game state
const updateDisplay = (screen: string, data?: any) => {
  switch (screen) {
    case 'splash':
      line1.value = '  OTS Firmware  '
      line2.value = '  Booting...    '
      break
    case 'waiting':
      line1.value = ' Waiting for    '
      line2.value = ' Connection...  '
      break
    case 'lobby':
      line1.value = ' Connected!     '
      line2.value = ' Waiting Game...'
      break
    case 'troops':
      line1.value = formatTroopLine(data.current, data.max)
      line2.value = formatPercentLine(data.percent, data.current)
      break
    case 'victory':
      line1.value = '   VICTORY!     '
      line2.value = ' Good Game!     '
      break
    case 'defeat':
      line1.value = '    DEFEAT      '
      line2.value = ' Good Game!     '
      break
  }
}
</script>

<style scoped>
.lcd-display {
  font-family: 'Courier New', monospace;
  background: #9ACD32;
  color: #003366;
  padding: 8px;
  border-radius: 4px;
}
.lcd-line {
  white-space: pre;
  font-size: 16px;
  line-height: 24px;
}
</style>
```

### Text Formatting Helpers

```typescript
// Format troop count with unit scaling
function formatTroopCount(troops: number): string {
  if (troops >= 1000000000) {
    const b = Math.floor(troops / 1000000000)
    const d = Math.floor((troops % 1000000000) / 100000000)
    return `${b}.${d}B`
  } else if (troops >= 1000000) {
    const m = Math.floor(troops / 1000000)
    const d = Math.floor((troops % 1000000) / 100000)
    return `${m}.${d}M`
  } else if (troops >= 1000) {
    const k = Math.floor(troops / 1000)
    const d = Math.floor((troops % 1000) / 100)
    return `${k}.${d}K`
  } else {
    return troops.toString()
  }
}

// Format Line 1: right-aligned troop counts
function formatTroopLine(current: number, max: number): string {
  const currentStr = formatTroopCount(current)
  const maxStr = formatTroopCount(max)
  const content = `${currentStr} / ${maxStr}`
  const padding = 16 - content.length
  return ' '.repeat(Math.max(0, padding)) + content
}

// Format Line 2: left-aligned percentage
function formatPercentLine(percent: number, currentTroops: number): string {
  const calculated = Math.floor(currentTroops * percent / 100)
  const calcStr = formatTroopCount(calculated)
  const content = `${percent}% (${calcStr})`
  return content.padEnd(16, ' ')
}
```

## Testing Checklist

When implementing the emulator, verify:

- [ ] Splash screen shows on initial load
- [ ] Waiting screen shows when WebSocket disconnected
- [ ] Lobby screen shows when connected, no game
- [ ] Troop display shows during game with correct alignment
- [ ] Line 1 is right-aligned with proper spacing
- [ ] Line 2 is left-aligned with proper spacing
- [ ] Unit scaling works correctly (K/M/B)
- [ ] Victory screen shows after GAME_END (data.victory=true)
- [ ] Defeat screen shows after GAME_END (data.victory=false)
- [ ] End screen clears on userscript disconnect/reconnect
- [ ] All text exactly matches firmware (including spaces)
- [ ] Display updates in real-time with state changes

## Character Set Reference

The HD44780 LCD uses standard ASCII for most characters:
- Letters: A-Z, a-z
- Numbers: 0-9
- Symbols: `! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ { | } ~`
- Space: ` ` (0x20)

**Special Characters** (if needed in future):
- Custom characters can be defined (8 slots, 5×8 pixels each)
- Not currently used in OTS firmware

---

## Version History

**v1.0** (December 19, 2025):
- Initial specification
- 7 screen types documented
- Based on firmware commit with game end screen implementation

---

**Firmware Source Files**:
- `ots-fw-main/src/system_status_module.c`
- `ots-fw-main/src/troops_module.c`
- `ots-fw-main/include/lcd_driver.h`

**Related Documentation**:
- `/prompts/WEBSOCKET_MESSAGE_SPEC.md` - WebSocket protocol
- `ots-fw-main/GAME_END_SCREEN.md` - Game end screen feature
- `ots-simulator/copilot-project-context.md` - Server implementation guide
