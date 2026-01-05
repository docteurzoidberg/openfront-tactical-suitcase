# WebSocket Message Specification

**Single Source of Truth for OTS WebSocket Protocol**

This specification defines the shared WebSocket message format for:
- `ots-userscript` (Tampermonkey userscript)
- `ots-simulator` (Nuxt 4 dashboard + Nitro server)
- `ots-fw-main` (ESP32-S3 device firmware)

**Protocol Change Workflow:**
1. Update this file first (single source of truth)
2. Update `ots-shared/src/game.ts` (TypeScript types)
3. Update `ots-fw-main/include/protocol.h` (C firmware types)
4. Update implementations (server, userscript, firmware)

**For developer documentation, see:** [`doc/developer/websocket-protocol.md`](../doc/developer/websocket-protocol.md)

---

## Transport

- **Protocol**: WebSocket over TCP
- **Format**: UTF-8 JSON text frames
- **Endpoint**: `/ws` (single endpoint for all client types)

All messages use a common envelope with a `type` field:

```json
{
  "type": "handshake" | "state" | "event" | "cmd",
  "payload": { /* type-dependent structure */ }
}
```

---

## Message Types

### Handshake (Client Identification)

Sent by client immediately after WebSocket connection to identify client type.

```json
{
  "type": "handshake",
  "clientType": "userscript" | "ui" | "firmware"
}
```

**Server Response:**
```json
{
  "type": "handshake-ack",
  "clientType": "userscript" | "ui" | "firmware"
}
```

### Incoming Messages (Client → Server)

#### `event` - Game Events
Game-detected events from userscript or firmware.

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_START" | "GAME_END" | "NUKE_LAUNCHED" | /* ... */,
    "timestamp": 1234567890,
    "message": "Optional human-readable message",
    "data": { /* event-specific data */ }
  }
}
```

#### `cmd` - Commands
User-initiated actions from dashboard or firmware.

```json
{
  "type": "cmd",
  "payload": {
    "action": "send-nuke" | "set-troops-percent" | "hardware-diagnostic" | "ping",
    "params": { /* action-specific parameters */ }
  }
}
```

### Outgoing Messages (Server → Client)

#### `state` - Game State Updates
Periodic or on-change game state snapshots.

```json
{
  "type": "state",
  "payload": {
    "timestamp": 1234567890,
    "troops": {
      "current": 120000,
      "max": 1100000
    }
  }
}
```

#### `event` - Broadcasted Events
Events forwarded to all connected clients.

```json
{
  "type": "event",
  "payload": { /* same structure as incoming event */ }
}
```

---

## Commands Reference

### `send-nuke`
Launch a nuclear weapon (dashboard → userscript).

```json
{
  "type": "cmd",
  "payload": {
    "action": "send-nuke",
    "params": {
      "nukeType": "atom" | "hydro" | "mirv"
    }
  }
}
```

### `set-troops-percent`
Set troop deployment percentage (slider).

```json
{
  "type": "cmd",
  "payload": {
    "action": "set-troops-percent",
    "params": {
      "percent": 50  // 0-100
    }
  }
}
```

### `hardware-diagnostic`
Request hardware status report.

```json
{
  "type": "cmd",
  "payload": {
    "action": "hardware-diagnostic"
  }
}
```

### `ping`
Connection test (server responds with INFO event).

```json
{
  "type": "cmd",
  "payload": {
    "action": "ping"
  }
}
```

---

## Events Reference

### Game Lifecycle

#### `GAME_START`
Game session started.

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_START",
    "timestamp": 1234567890,
    "message": "Game started"
  }
}
```

#### `GAME_END`
Game session ended.

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "Game ended",
    "data": {
      "victory": true | false  // true if player won
    }
  }
}
```

#### `GAME_SPAWNING`
Player is in the spawning/joining phase before game starts.

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_SPAWNING",
    "timestamp": 1234567890,
    "message": "Player spawning",
    "data": {
      "playerID": "player-xyz",
      "playerName": "Username"
    }
  }
}
```

**Hardware Behavior**:
- System Status: Show "Spawning" or "Waiting" on LCD
- No active game modules (nukes, alerts disabled during spawn)

### Connection Status

#### `INFO`
General information events (heartbeats, pings, status).

```json
{
  "type": "event",
  "payload": {
    "type": "INFO",
    "timestamp": 1234567890,
    "message": "heartbeat"  // or "pong-from-userscript", "userscript-connected", etc.
  }
}
```

#### `ERROR`
Error events for reporting failures and exceptional conditions.

```json
{
  "type": "event",
  "payload": {
    "type": "ERROR",
    "timestamp": 1234567890,
    "message": "Game state error",
    "data": {
      "errorCode": "GAME_STATE_INVALID",
      "details": "Additional error context"
    }
  }
}
```

**Common Error Codes**:
- `GAME_STATE_INVALID` - Game state inconsistency
- `CONNECTION_ERROR` - WebSocket connection issue
- `HARDWARE_ERROR` - Hardware module failure
- `PROTOCOL_ERROR` - Message format error

### Nuke Launch Events

#### `NUKE_LAUNCHED`
Nuclear weapon launched by player. The specific nuke type (atom/hydro/MIRV) is indicated in `data.nukeType`.

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_LAUNCHED",
    "timestamp": 1234567890,
    "message": "Nuke launched",
    "data": {
      "nukeType": "Atom Bomb",   // "Atom Bomb", "Hydrogen Bomb", or "MIRV"
      "nukeUnitID": 12345,       // REQUIRED: unique unit ID for tracking
      "targetTile": 54321,
      "targetPlayerID": "player-xyz",
      "tick": 950,
      "coordinates": { "x": 123, "y": 456 }
    }
  }
}
```

**Hardware Behavior**:
- Nuke Module: Corresponding button LED stays ON while any nuke of that type is in flight
- Tracking: Count-based (not timer-based) - LED OFF when count reaches 0
- `nukeUnitID` is REQUIRED for proper multi-nuke tracking
- Each nuke type tracks independently based on `data.nukeType`

### Alert Events (Incoming Threats)

#### `ALERT_ATOM` / `ALERT_HYDRO` / `ALERT_MIRV`
Incoming nuclear weapon detected targeting player.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_ATOM",  // or ALERT_HYDRO, ALERT_MIRV
    "timestamp": 1234567890,
    "message": "Incoming nuclear strike detected!",
    "data": {
      "nukeType": "Atom Bomb",
      "launcherPlayerID": "player-abc",
      "launcherPlayerName": "EnemyPlayer",
      "nukeUnitID": 12345,      // REQUIRED: for tracking
      "targetTile": 54321,
      "targetPlayerID": "player-xyz",
      "tick": 950,
      "coordinates": { "x": 123, "y": 456 }
    }
  }
}
```

**Hardware Behavior:**
- Alert Module: Threat LEDs stay ON (solid) while any nuke of that type is incoming
- WARNING LED: Activates (fast blink) when ANY alert is active

#### `ALERT_LAND`
Land-based invasion detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_LAND",
    "timestamp": 1234567890,
    "message": "Land invasion detected!",
    "data": {
      "type": "land",
      "attackerPlayerID": "player-abc",
      "attackerPlayerName": "EnemyPlayer",
      "attackID": 789,
      "troops": 50000,
      "targetPlayerID": "player-xyz",
      "tick": 1000
    }
  }
}
```

#### `ALERT_NAVAL`
Naval invasion detected (transport ship).

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_NAVAL",
    "timestamp": 1234567890,
    "message": "Naval invasion detected!",
    "data": {
      "type": "boat",
      "attackerPlayerID": "player-abc",
      "attackerPlayerName": "EnemyPlayer",
      "transportShipUnitID": 98765,
      "troops": 25000,
      "targetTile": 54324,
      "targetPlayerID": "player-xyz",
      "tick": 1005,
      "coordinates": { "x": 126, "y": 459 }
    }
  }
}
```

### Troop Events

#### `TROOP_UPDATE`
Real-time troop count update for troops module.

```json
{
  "type": "event",
  "payload": {
    "type": "TROOP_UPDATE",
    "timestamp": 1234567890,
    "message": "Troop count updated",
    "data": {
      "current": 120000,  // Current troop count
      "max": 1100000      // Maximum troop capacity
    }
  }
}
```

**Hardware Behavior**:
- Troops Module: Updates LCD display immediately
- Line 1: Shows `current / max` with K/M/B unit scaling
- Line 2: Shows deployment percentage based on slider
- Sent periodically (every 100ms when troops change) or on request

### Nuke Outcome Events

#### `NUKE_EXPLODED`
Nuclear weapon reached target successfully.

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_EXPLODED",
    "timestamp": 1234567890,
    "message": "Nuclear weapon exploded",
    "data": {
      "nukeType": "Atom Bomb",
      "unitID": 12345,          // REQUIRED: for unregistering from tracker
      "ownerID": "player-abc",
      "ownerName": "PlayerName",
      "targetTile": 54321,
      "tick": 1000
    }
  }
}
```

**Hardware Behavior:**
- Unregister nuke from tracker by `unitID`
- Turn LED OFF only when tracker count reaches 0
- Applies to both Nuke Module (outgoing) and Alert Module (incoming)

#### `NUKE_INTERCEPTED`
Nuclear weapon destroyed before reaching target.

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_INTERCEPTED",
    "timestamp": 1234567890,
    "message": "Nuclear weapon intercepted",
    "data": {
      "nukeType": "Hydrogen Bomb",
      "unitID": 12346,          // REQUIRED: for unregistering from tracker
      "ownerID": "player-xyz",
      "ownerName": "EnemyPlayer",
      "targetTile": 54322,
      "tick": 950
    }
  }
}
```

**Hardware Behavior:** Same as `NUKE_EXPLODED` (both resolve tracked nukes).

### Sound Events

#### `SOUND_PLAY`
Request audio playback (sent to firmware/dashboard).

```json
{
  "type": "event",
  "payload": {
    "type": "SOUND_PLAY",
    "timestamp": 1234567890,
    "message": "Play alert sound",
    "data": {
      "soundId": "alert.atom",              // stable identifier
      "priority": "high",                    // "low" | "normal" | "high"
      "interrupt": true,                     // should interrupt current playback
      "requestId": "evt-9f3c2d"             // optional correlation ID
    }
  }
}
```

**Sound Catalog (soundId → soundIndex mapping):**

| soundId | soundIndex | Description |
|---------|------------|-------------|
| `game_start` | 1 | Game start sound |
| `game_player_death` | 2 | Player death sound |
| `game_victory` | 3 | Victory sound |
| `game_defeat` | 4 | Defeat sound |

**Architecture:**
- Userscript: Sends `SOUND_PLAY` events only (does NOT play sounds locally)
- Server: Relays events to firmware and dashboard
- Firmware: Maps `soundId` → `soundIndex` and sends CAN command to audio module
- Dashboard: Can play sounds in browser for emulation mode

### Hardware Diagnostic

#### `HARDWARE_DIAGNOSTIC`
Hardware status report from device or simulator.

```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_DIAGNOSTIC",
    "timestamp": 1234567890,
    "message": "Hardware diagnostic report",
    "data": {
      "version": "2025-12-29.1",
      "deviceType": "firmware" | "simulator",
      "serialNumber": "OTS-FW-ESP001",
      "owner": "DrZoid",
      "hardware": {
        "lcd": { "present": true, "working": true },
        "inputBoard": { "present": true, "working": true },
        "outputBoard": { "present": true, "working": true },
        "adc": { "present": true, "working": true },
        "soundModule": { "present": false, "working": false }
      }
    }
  }
}
```

**Trigger:** Sent in response to `hardware-diagnostic` command.

---

## Timing Constants

### Nuke Module
- Button LEDs: ON (solid) while nukes in flight, OFF when count reaches 0
- Button polling: 50ms debounce interval
- No cooldown (rapid fire allowed)

### Alert Module
- Threat LEDs: ON (solid) while incoming threats tracked
- WARNING LED: Fast blink (300ms on/off) when any alert active
- Nuke alerts: Resolved by `NUKE_EXPLODED` or `NUKE_INTERCEPTED` events
- Invasion alerts: 15-second timeout (timer-based, no unit tracking)

### Troops Module
- Slider polling: 100ms debounce
- Command threshold: Send `set-troops-percent` only on ≥1% change
- Display update: Immediate (no animation delay)

---

## Protocol Versions

**Current Version:** 1.0 (January 2026)

**Breaking Changes:**
- Require `nukeUnitID` in all nuke-related events (launch, alert, outcome)
- Count-based LED tracking (not timer-based)
- Single `/ws` endpoint with handshake-based client identification

**Backwards Compatibility:**
- Optional fields can be added without breaking existing clients
- New event types can be added (clients ignore unknown types)
- Command actions are validated by consumers

---

## Implementation Notes

### TypeScript (`ots-shared/src/game.ts`)
- Defines `IncomingMessage`, `OutgoingMessage`, `GameEvent`, `GameEventType` types
- Single source of truth for TypeScript implementations
- Used by: ots-userscript, ots-simulator

### C Firmware (`ots-fw-main/include/protocol.h`)
- Defines `game_event_type_t` enum matching `GameEventType`
- String conversions in `src/protocol.c`
- Event dispatcher routes events to hardware modules
- Used by: ots-fw-main firmware

### Synchronization
When updating protocol:
1. Update THIS file first (specification)
2. Update TypeScript types in `ots-shared/src/game.ts`
3. Update C types in `ots-fw-main/include/protocol.h`
4. Update string conversions in `ots-fw-main/src/protocol.c`
5. Update implementations in userscript, simulator, firmware
6. Test end-to-end message flow

---

## References

- **Developer Guide**: [`doc/developer/websocket-protocol.md`](../doc/developer/websocket-protocol.md)
- **TypeScript Implementation**: `ots-shared/src/game.ts`
- **C Firmware Implementation**: `ots-fw-main/include/protocol.h`
- **Userscript Context**: `ots-userscript/copilot-project-context.md`
- **Simulator Context**: `ots-simulator/copilot-project-context.md`
- **Firmware Context**: `ots-fw-main/copilot-project-context.md`
