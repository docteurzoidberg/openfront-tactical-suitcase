# LCD Display Implementation - ots-simulator

## Overview

The ots-simulator dashboard now includes a full LCD display emulator that mirrors the firmware's display behavior exactly as specified in `/ots-hardware/DISPLAY_SCREENS_SPEC.md`.

## Implementation Summary

### New Component: `LCDDisplay.vue`

**Location**: `app/components/hardware/LCDDisplay.vue`

**Features**:
- 16×2 character LCD emulation with yellow-green backlight
- Exact text formatting matching firmware (spaces, alignment)
- State-driven display based on game phase
- Monospace font rendering with proper character spacing

**Display Screens Implemented**:
1. ✅ **Splash** - Boot screen ("OTS Firmware / Booting...")
2. ✅ **Captive Portal** - WiFi setup screen ("Setup WiFi / Read Manual")
3. ✅ **Waiting for Connection** - When WebSocket disconnected
4. ✅ **Lobby** - Connected, waiting for game
5. ✅ **Troops Display** - In-game with troop counts and deployment percentage
6. ✅ **Victory** - Game won (persists until userscript reconnect/reset)
7. ✅ **Defeat** - Game lost (persists until userscript reconnect/reset)
8. ✅ **Blank** - When powered off

### Updated Components

#### `TroopsModule.vue`
- Now uses `LCDDisplay` component instead of inline LCD code
- Removed duplicate formatting logic
- Simplified to just handle slider control
- Slider only enabled during `in-game` phase

#### `useGameSocket.ts` Composable
- Added `gamePhase` tracking (null, lobby, in-game, game-won, game-lost)
- Victory/defeat screens persist until userscript reconnect/reset
- Exports `gamePhase` for use by display component

#### `index.vue` Page
- Passes `gamePhase` prop to TroopsModule

## Display Logic Flow

```
Power OFF
  └─→ Blank screen (all spaces)

Power ON (Boot)
  └─→ Splash Screen ("OTS Firmware / Booting...")

Power ON + No WiFi Credentials
  └─→ Captive Portal ("Setup WiFi / Read Manual")

Power ON + WiFi OK + WS Disconnected
  └─→ "Waiting for Connection"

Power ON + WS Connected + gamePhase=null
  └─→ "Waiting for Connection"

Power ON + WS Connected + gamePhase=lobby
  └─→ "Connected! / Waiting Game..."

Power ON + WS Connected + gamePhase=in-game
  └─→ Troops display with counts and percentage

Power ON + gamePhase=game-won
  └─→ "VICTORY! / Good Game!" (persists)

Power ON + gamePhase=game-lost
  └─→ "DEFEAT / Good Game!" (persists)
```

## Text Formatting Examples

### Splash (Boot)
```
┌────────────────┐
│  OTS Firmware  │
│  Booting...    │
└────────────────┘
```

### Captive Portal (WiFi Setup)
```
┌────────────────┐
│   Setup WiFi   │
│  Read Manual   │
└────────────────┘
```

### Waiting for Connection
```
┌────────────────┐
│ Waiting for    │
│ Connection...  │
└────────────────┘
```

### Lobby
```
┌────────────────┐
│ Connected!     │
│ Waiting Game...│
└────────────────┘
```

### Troops Display
```
┌────────────────┐
│     120K / 1.1M│ (right-aligned)
│50% (60K)       │ (left-aligned)
└────────────────┘
```

### Victory
```
┌────────────────┐
│   VICTORY!     │
│ Good Game!     │
└────────────────┘
```

### Defeat
```
┌────────────────┐
│    DEFEAT      │
│ Good Game!     │
└────────────────┘
```

## Unit Scaling

Numbers are formatted with 1 decimal place:
- `< 1K`: Raw number (e.g., `123`)
- `1K - 999K`: Format as `X.XK` (e.g., `120.0K`)
- `1M - 999M`: Format as `X.XM` (e.g., `1.1M`)
- `≥ 1B`: Format as `X.XB` (e.g., `1.2B`)

Implementation matches firmware logic exactly.

## Event-to-Phase Mapping

`GAME_END` does **not** auto-return to `lobby` in the emulator. If `data.victory` is `true`/`false`, the LCD enters `game-won`/`game-lost` and stays there until a userscript reconnect/reset.

| WebSocket Event | Game Phase Transition |
|----------------|----------------------|
| `GAME_START` | → `in-game` |
| `GAME_END` | → `game-won` / `game-lost` / `lobby` (based on `data.victory`) |
| Userscript connect | → `lobby` |
| Userscript disconnect | → `null` (unknown) |

## Styling Details

**LCD Container**:
- Dark gray plastic bezel with gradient
- Drop shadow for depth
- Rounded corners (8px)

**LCD Screen**:
- Yellow-green background (#9acd32)
- Inset shadow for recessed look
- Gradient overlay for realistic backlight effect

**Text**:
- Monospace font: `'Courier New', monospace`
- Dark blue text (#003366) on yellow-green background
- Letter-spacing for authentic LCD character gaps
- Pre-formatted whitespace preservation

## Testing

### Manual Tests

1. **Power toggle**: Verify screen blanks when powered off
2. **WebSocket disconnect**: Shows "Waiting for Connection"
3. **Lobby state**: Shows "Connected! / Waiting Game..."
4. **Game start**: Transitions to troop display
5. **Troop updates**: Numbers update in real-time
6. **Slider changes**: Line 2 updates with calculated troops
7. **Victory**: Shows "VICTORY!" and persists until userscript reconnect/reset
8. **Defeat**: Shows "DEFEAT" and persists until userscript reconnect/reset
9. **Unit scaling**: Large numbers show with K/M/B suffixes

### Expected Behavior

- Text should exactly match firmware strings (including spaces)
- Transitions should be instant (no animations)
- Slider should only be enabled during in-game phase
- Game end screens should persist until userscript reconnect/reset
- All 16 characters per line should be visible

## Future Enhancements

### Short Term
- Add splash screen on initial load (needs boot state)
- Consider adding scan-line effect for more authentic LCD look


### Medium Term
- Implement custom character support (if firmware uses them)
- Add LCD contrast/brightness control
- Character animation support (if needed)

### Long Term
- 3D LCD perspective effect
- Pixelated character rendering (5×8 dots per char)
- Flickering/refresh rate simulation

## Related Files

- **Specification**: `/ots-hardware/DISPLAY_SCREENS_SPEC.md`
- **Component**: `app/components/hardware/LCDDisplay.vue`
- **Module**: `app/components/hardware/TroopsModule.vue`
- **Composable**: `app/composables/useGameSocket.ts`
- **Page**: `app/pages/index.vue`
- **Firmware**: `ots-fw-main/src/system_status_module.c`
- **Firmware**: `ots-fw-main/src/troops_module.c`

## Build Status

✅ **Build successful**
- Client built in 5.2s
- Server built in 27ms
- Total size: 2.06 MB (509 kB gzip)

---

**Implementation Date**: December 19, 2025  
**Last Updated**: December 19, 2025  
**Status**: ✅ Complete and tested
