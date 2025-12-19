# OTS LCD Display Screens Specification

This document describes all LCD display screens implemented in the OTS firmware. Use this specification to replicate the exact display behavior in the `ots-server` dashboard emulator.

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
**Phase**: Startup (before any connections)  
**Duration**: ~2 seconds during firmware initialization

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

**Display Trigger**: Shown immediately on module initialization

---

### 2. Waiting for Connection

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
Line 2 (0,1): " Connection...  "  // 1 space at start, 2 at end
```

**Display Trigger**: `INTERNAL_EVENT_WS_DISCONNECTED` or no WebSocket connection

---

### 3. Lobby Screen (Connected, No Game)

**Module**: System Status  
**Phase**: `GAME_PHASE_LOBBY`, `GAME_PHASE_SPAWNING`  
**Duration**: Until game starts

```
┌────────────────┐
│ Connected!     │ (left-aligned with leading space)
│ Waiting Game...│ (left-aligned with leading space)
└────────────────┘
```

**Implementation**:
```c
Line 1 (0,0): " Connected!     "  // 1 space at start, 5 at end
Line 2 (0,1): " Waiting Game..."  // 1 space at start, no trailing
```

**Display Trigger**: `INTERNAL_EVENT_WS_CONNECTED` or `GAME_PHASE_LOBBY`

---

### 4. Troop Display (In-Game)

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

### 5. Victory Screen

**Module**: System Status  
**Phase**: `GAME_PHASE_WON`  
**Duration**: 5 seconds, then returns to lobby

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

**Display Trigger**: `GAME_EVENT_WIN`  
**Auto-Clear**: After 5 seconds (5000ms timeout)

---

### 6. Defeat Screen

**Module**: System Status  
**Phase**: `GAME_PHASE_LOST`  
**Duration**: 5 seconds, then returns to lobby

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

**Display Trigger**: `GAME_EVENT_LOOSE` (note: protocol uses "LOOSE" not "LOSE")  
**Auto-Clear**: After 5 seconds (5000ms timeout)

---

### 7. Shutdown Screen

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
     ├─→ Splash Screen (2s)
     │
     ↓
[NO WS CONNECTION]
     │
     ├─→ Waiting for Connection
     │
     ↓
[WS CONNECTED]
     │
     ├─→ Lobby Screen
     │
     ↓
[GAME START]
     │
     ├─→ Troop Display (in-game)
     │      ↓ (slider changes)
     │      ├─→ Update Line 2
     │      ↓ (troop data update)
     │      └─→ Update Line 1 & 2
     │
     ↓
[GAME END]
     │
     ├─→ Victory Screen (if WIN)
     │   OR
     ├─→ Defeat Screen (if LOOSE)
     │   OR
     └─→ Lobby Screen (if GAME_END unknown)
     │
     │ (after 5s timeout)
     │
     ↓
[LOBBY]
     │
     └─→ Lobby Screen (repeat)
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

## Implementation Notes for ots-server Emulator

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
   - Track game end timeout (5 seconds)

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
      setTimeout(() => updateDisplay('lobby'), 5000)
      break
    case 'defeat':
      line1.value = '    DEFEAT      '
      line2.value = ' Good Game!     '
      setTimeout(() => updateDisplay('lobby'), 5000)
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
- [ ] Victory screen shows for 5 seconds after WIN event
- [ ] Defeat screen shows for 5 seconds after LOOSE event
- [ ] Screens return to lobby after game end timeout
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
- `/protocol-context.md` - WebSocket protocol
- `ots-fw-main/GAME_END_SCREEN.md` - Game end screen feature
- `ots-server/copilot-project-context.md` - Server implementation guide
