# Protocol Alignment Check

**Date**: December 29, 2025  
**Status**: ✅ ALIGNED

## Overview

This document records the protocol alignment verification between firmware and TypeScript implementations.

## Component Versions

- **ots-shared** (TypeScript types): Latest
- **ots-server** (Nuxt dashboard): Latest
- **ots-userscript** (Tampermonkey): Latest
- **ots-fw-main** (ESP32-S3 firmware): Updated

## Event Types Comparison

### TypeScript (ots-shared/src/game.ts)

```typescript
export type GameEventType =
  | 'INFO'
  | 'ERROR'
  | 'GAME_SPAWNING'
  | 'GAME_START'
  | 'GAME_END'
  | 'SOUND_PLAY'
  | 'HARDWARE_DIAGNOSTIC'
  | 'NUKE_LAUNCHED'
  | 'NUKE_EXPLODED'
  | 'NUKE_INTERCEPTED'
  | 'ALERT_NUKE'
  | 'ALERT_HYDRO'
  | 'ALERT_MIRV'
  | 'ALERT_LAND'
  | 'ALERT_NAVAL'
  | 'TROOP_UPDATE'
  | 'HARDWARE_TEST'
```

### Firmware (ots-fw-main/include/protocol.h)

```c
typedef enum {
    GAME_EVENT_INFO = 0,
    GAME_EVENT_ERROR,
    GAME_EVENT_GAME_SPAWNING,
    GAME_EVENT_GAME_START,
    GAME_EVENT_GAME_END,
    GAME_EVENT_SOUND_PLAY,
    GAME_EVENT_HARDWARE_DIAGNOSTIC,
    GAME_EVENT_NUKE_LAUNCHED,
    GAME_EVENT_NUKE_EXPLODED,
    GAME_EVENT_NUKE_INTERCEPTED,
    GAME_EVENT_ALERT_NUKE,
    GAME_EVENT_ALERT_HYDRO,
    GAME_EVENT_ALERT_MIRV,
    GAME_EVENT_ALERT_LAND,
    GAME_EVENT_ALERT_NAVAL,
    GAME_EVENT_TROOP_UPDATE,
    GAME_EVENT_HARDWARE_TEST,
    // Internal-only events (not in protocol)
    INTERNAL_EVENT_NETWORK_CONNECTED,
    INTERNAL_EVENT_NETWORK_DISCONNECTED,
    INTERNAL_EVENT_WS_CONNECTED,
    INTERNAL_EVENT_WS_DISCONNECTED,
    INTERNAL_EVENT_WS_ERROR,
    INTERNAL_EVENT_BUTTON_PRESSED,
    GAME_EVENT_INVALID
} game_event_type_t;
```

## Changes Made

### Updated Files

1. **`ots-fw-main/include/protocol.h`**
   - Added `GAME_EVENT_ERROR` (after INFO)
   - Added `GAME_EVENT_TROOP_UPDATE` (before HARDWARE_TEST)
   - Reordered events to match TypeScript order

2. **`ots-fw-main/src/protocol.c`**
   - Added `ERROR` case to `event_type_to_string()`
   - Added `TROOP_UPDATE` case to `event_type_to_string()`
   - Added `ERROR` parser to `string_to_event_type()`
   - Added `TROOP_UPDATE` parser to `string_to_event_type()`
   - Reordered cases to match enum order

## Verification

### Build Status

```
Processing esp32-s3-dev
RAM:   [=         ]  12.4% (used 40496 bytes from 327680 bytes)
Flash: [=====     ]  48.2% (used 1011728 bytes from 2097152 bytes)
=========================================== [SUCCESS] Took 3.76 seconds ===========================================
```

✅ Firmware builds successfully with updated protocol types.

### Event Coverage

| Event Type | TypeScript | Firmware | Status |
|------------|-----------|----------|--------|
| INFO | ✅ | ✅ | ✅ |
| ERROR | ✅ | ✅ | ✅ |
| GAME_SPAWNING | ✅ | ✅ | ✅ |
| GAME_START | ✅ | ✅ | ✅ |
| GAME_END | ✅ | ✅ | ✅ |
| SOUND_PLAY | ✅ | ✅ | ✅ |
| HARDWARE_DIAGNOSTIC | ✅ | ✅ | ✅ |
| NUKE_LAUNCHED | ✅ | ✅ | ✅ |
| NUKE_EXPLODED | ✅ | ✅ | ✅ |
| NUKE_INTERCEPTED | ✅ | ✅ | ✅ |
| ALERT_NUKE | ✅ | ✅ | ✅ |
| ALERT_HYDRO | ✅ | ✅ | ✅ |
| ALERT_MIRV | ✅ | ✅ | ✅ |
| ALERT_LAND | ✅ | ✅ | ✅ |
| ALERT_NAVAL | ✅ | ✅ | ✅ |
| TROOP_UPDATE | ✅ | ✅ | ✅ |
| HARDWARE_TEST | ✅ | ✅ | ✅ |

**Total**: 17 event types, all aligned ✅

## Protocol Constants

### WebSocket Configuration
- Default URL: `ws://localhost:3000/ws`
- Default Port: 3000
- Client Types: `ui`, `userscript`, `firmware`

### Heartbeat Configuration
- Interval: 5000ms (5 seconds)
- Reconnect Delay: 2000ms (2 seconds)
- Max Reconnect Delay: 15000ms (15 seconds)

## Sound System

Both implementations support:
- `game_start` — Game start sound
- `game_player_death` — Player death sound
- `game_victory` — Victory sound
- `game_defeat` — Defeat sound

Sound events use the `SOUND_PLAY` event type with `soundId` in the data payload.

## Hardware Diagnostics

Both implementations support the `HARDWARE_DIAGNOSTIC` event type for:
- Module status reporting
- Pin configuration display
- I2C address information
- Error state reporting

## Troops Module

The `TROOP_UPDATE` event type is used for:
- Current troop count updates
- Maximum troop count updates
- Attack ratio percentage (0-100)

Data format:
```json
{
  "currentTroops": 120000,
  "maxTroops": 1100000,
  "attackRatioPercent": 50
}
```

## Internal Events (Firmware Only)

The firmware uses additional internal events not exposed in the protocol:
- `INTERNAL_EVENT_NETWORK_CONNECTED`
- `INTERNAL_EVENT_NETWORK_DISCONNECTED`
- `INTERNAL_EVENT_WS_CONNECTED`
- `INTERNAL_EVENT_WS_DISCONNECTED`
- `INTERNAL_EVENT_WS_ERROR`
- `INTERNAL_EVENT_BUTTON_PRESSED`

These events are used for module coordination and do not appear in WebSocket messages.

## Next Steps

- ✅ Protocol alignment verified
- ✅ Firmware builds successfully
- ✅ All event types present in both implementations
- ⏭️ Ready for testing with complete protocol coverage

## Maintenance Notes

When adding new event types:
1. Update `/prompts/protocol-context.md` (source of truth)
2. Update `ots-shared/src/game.ts` TypeScript types
3. Update `ots-fw-main/include/protocol.h` enum
4. Update `ots-fw-main/src/protocol.c` string conversions
5. Add handlers in relevant modules (server/userscript/firmware)
6. Run this alignment check again

## References

- Protocol Spec: `/prompts/protocol-context.md`
- TypeScript Types: `/ots-shared/src/game.ts`
- Firmware Types: `/ots-fw-main/include/protocol.h`
- Firmware Implementation: `/ots-fw-main/src/protocol.c`
