# OTS Protocol Context – Source of Truth for All Message Definitions

**This file is the single source of truth for the WebSocket protocol used across all OTS components.**

This protocol defines the **shared WebSocket message format** for:

- `ots-server` (Nuxt 4 app + Nitro WebSocket server)
- `ots-userscript` (Tampermonkey userscript bridge)
- `ots-fw-main` (ESP32-S3 device firmware)

**All implementations must comply with this document.** When evolving the protocol:
1. Update this file first
2. Update `ots-shared/src/game.ts` (TypeScript implementation)
3. Update `ots-fw-main/include/protocol.h` (C++ implementation)
4. Update all consuming code in server, userscript, and firmware

---

## Transport

- Transport: **WebSocket** over TCP.
- Message format: **UTF-8 JSON text frames**.
- Each message is a single JSON object with at least a `type` field.

```jsonc
{
  "type": "state" | "event" | "cmd" | "ack",
  "payload": { /* structure depends on type */ }
}
```

The same high-level message envelope applies everywhere. Platform-specific implementations (TypeScript vs C++) may have different type systems, but the JSON on the wire must remain consistent.

---

## Core Types

### GameState

Represents the current game snapshot.

```ts
ModuleGeneralState = {
  m_link: boolean;       // hardware link status
}

ModuleAlertState = {
  m_alert_warning: boolean;
  m_alert_atom: boolean;
  m_alert_hydro: boolean;
  m_alert_mirv: boolean;
  m_alert_land: boolean;
  m_alert_naval: boolean;
}

ModuleNukeState = {
  m_nuke_launched: boolean;
  m_hydro_launched: boolean;
  m_mirv_launched: boolean;
}

TroopsData = {
  current: number;        // current available troops
  max: number;            // maximum troops capacity
}

HWState = {
  m_general: ModuleGeneralState;
  m_alert: ModuleAlertState;
  m_nuke: ModuleNukeState;
}

GameState = {
  timestamp: number;      // ms since epoch
  mapName: string;
  mode: string;
  playerCount: number;
  troops?: TroopsData;    // optional: current troop counts
  hwState: HWState;       // hardware module states
}
```

### GameEvent

Represents a single event in the game.

```ts
GameEventType =
  | "INFO"
  | "GAME_START"
  | "GAME_END"          // Game ended (see data.victory for win/loss)
  | "SOUND_PLAY"        // Request sound playback (userscript → server → firmware)
  | "HARDWARE_DIAGNOSTIC" // Hardware diagnostic response from device
  | "NUKE_LAUNCHED"
  | "HYDRO_LAUNCHED"
  | "MIRV_LAUNCHED"
  | "NUKE_EXPLODED"
  | "NUKE_INTERCEPTED"
  | "ALERT_ATOM"
  | "ALERT_HYDRO"
  | "ALERT_MIRV"
  | "ALERT_LAND"
  | "ALERT_NAVAL"
  | "HARDWARE_TEST";

GameEvent = {
  type: GameEventType;
  timestamp: number;      // ms since epoch
  message?: string;       // optional human readable
  data?: unknown;         // optional structured payload (implementation-specific)
}
```

---

## Messages

### Incoming from game/clients → server

These are messages sent by `ots-userscript` and `ots-fw-main` to the server (`/ws-script` endpoint or a future `/ws-device` endpoint).

```ts
// Envelope from game bridge/device to server
IncomingMessage =
  | { type: "state"; payload: GameState }
  | { type: "event"; payload: GameEvent };
```

Semantics:
- `state` messages replace the current known game state on the server and are broadcast to UI clients.
- `event` messages are appended to the event log and broadcast to UI clients.

### Outgoing from server/UI → game bridge/device

These are messages sent by the dashboard UI (via `/ws-ui`) or server back to the userscript/firmware.

```ts
// Envelope from UI/server to game bridge/device
OutgoingMessage =
  | { type: "cmd"; payload: Command }
  | { type: "ack"; payload?: AckPayload };

Command = {
  action: string;                  // e.g. "ping", "send-nuke", "focus-player:123"
  params?: Record<string, unknown>;
};

AckPayload = {
  ok: boolean;
  requestId?: string;
  message?: string;                // optional human-readable info
  data?: Record<string, unknown>;  // optional structured payload
};
```

Semantics:
- `cmd` messages represent high-level actions the device/bridge should perform.
- `ack` messages represent acknowledgements or responses to previous commands.

---

## Command Definitions

Commands sent via `cmd` messages with specific `action` values and expected `params`.

### Hardware Module Commands

#### `send-nuke`
Send a nuclear weapon in the game (from hardware nuke module).

**Flow**: Hardware Button → Firmware → Server → Userscript → Game → Userscript → Server → Firmware LED

```ts
{
  type: "cmd",
  payload: {
    action: "send-nuke",
    params: {
      nukeType: "atom" | "hydro" | "mirv"
    }
  }
}
```

**Userscript behavior**:
1. Receive `send-nuke` command from server
2. Attempt to call game API (tries `game.sendNuke()`, `game.launchNuke()`, `game.useNuke()`)
3. On success:
   - Emits `NUKE_LAUNCHED`/`HYDRO_LAUNCHED`/`MIRV_LAUNCHED` event
   - Event flows back to server, then to firmware to activate LED confirmation
4. On failure:
   - Emits `INFO` event with error details

**Note**: The game API for launching nukes is not yet fully reverse-engineered. The userscript tries multiple common patterns and logs available methods for debugging.

#### `set-troops-percent`
Set the troop deployment percentage (from hardware troops module).

**Flow**: Hardware Slider → Firmware → Server → Userscript → Game UI

```ts
{
  type: "cmd",
  payload: {
    action: "set-troops-percent",
    params: {
      percent: 50  // 0-100
    }
  }
}
```

**Userscript behavior**:
1. Receive `set-troops-percent` command from server
2. Locate the game's troop deployment slider UI element
3. Set the slider value to the specified percentage
4. Trigger any necessary game events to update the deployment amount

**Command throttling**: 
- Firmware should only send commands when slider changes by ≥1%
- Debounce slider readings (100ms minimum interval)
- Prevents command flooding during rapid slider movement

**Game behavior**:
- Updates the visual slider position in the game UI
- Recalculates troop deployment amount based on percentage
- Updates any UI displays showing selected troop count

#### `hardware-diagnostic`
Request hardware diagnostic information from the device (firmware or ots-server simulator).

**Flow**: Userscript → Server/Firmware → Response with `HARDWARE_DIAGNOSTIC` event

```ts
{
  type: "cmd",
  payload: {
    action: "hardware-diagnostic",
    params: {}  // no parameters needed
  }
}
```

**Device behavior**:
1. Receive `hardware-diagnostic` command
2. Gather diagnostic information about hardware/software status
3. Respond with `HARDWARE_DIAGNOSTIC` event containing full diagnostic data

**Response** (see `HARDWARE_DIAGNOSTIC` event definition below):
```ts
{
  type: "event",
  payload: {
    type: "HARDWARE_DIAGNOSTIC",
    timestamp: 1234567890,
    message: "Hardware diagnostic report",
    data: {
      version: "2025-12-20.1",
      deviceType: "firmware" | "simulator",
      serialNumber: "OTS-FW-001234",
      owner: "PUSH Team",
      hardware: {
        lcd: { present: true, working: true },
        inputBoard: { present: true, working: true },
        outputBoard: { present: true, working: true },
        adc: { present: true, working: true },
        soundModule: { present: false, working: false }
      }
    }
  }
}
```

### Info / Status Events

Certain `INFO`-typed `GameEvent`s are reserved for connection and status signaling:

- `userscript-ws-open` / `userscript-ws-close` – emitted by the server when a userscript peer connects/disconnects.
- `device-ws-open` / `device-ws-close` – **reserved** for future firmware-specific connections.
- `userscript-connected` – emitted by the userscript on first successful WebSocket connect.
- `pong-from-userscript` – demo response to `ping` command.

Firmware should mirror similar patterns using `GameEvent` with `type: "INFO"` and appropriate `message`.

---

## Event Definitions

Events sent via `event` messages with specific event types and structured `data` payloads.

### Game Lifecycle Events

#### `GAME_START`
Emitted when a new game begins.

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
Emitted when a game ends (victory or defeat). Contains result information in the data payload.

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "You died",
    "data": {
      "victory": false,
      "phase": "game-lost",
      "reason": "death"
    }
  }
}
```

**Data Fields:**
- `victory` (boolean|null): `true` = player won, `false` = player lost, `null` = game ended but outcome unknown
- `phase` (string, optional): Game phase at end - `"game-won"`, `"game-lost"`, or `"ended"`
- `reason` (string, optional): Why game ended - `"death"` for elimination, `"victory"` for win, etc.

**Examples:**

Victory in solo/multiplayer:
```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "Victory!",
    "data": {
      "victory": true,
      "phase": "game-won"
    }
  }
}
```

Death/elimination:
```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "You were eliminated",
    "data": {
      "victory": false,
      "phase": "game-lost",
      "reason": "death"
    }
  }
}
```

Unknown outcome:
```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "Game ended",
    "data": {
      "victory": null
    }
  }
}
```

**Game outcome sounds:**
After `GAME_END`, emit `SOUND_PLAY` events with appropriate `soundId` to trigger audio:
- Player death: `SOUND_PLAY` with `soundId: "game_player_death"`
- Victory: `SOUND_PLAY` with `soundId: "game_victory"`
- Defeat: `SOUND_PLAY` with `soundId: "game_defeat"`

### Nuke Module Events

#### `NUKE_LAUNCHED`
Emitted when an atomic bomb is launched by the player.

**Flow**: Hardware Button → Firmware → Server → Userscript → Game → Userscript → Server → Hardware LED

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_LAUNCHED",
    "timestamp": 1234567890,
    "message": "Atomic bomb launched",
    "data": {
      "nukeType": "Atom Bomb",
      "nukeUnitID": 12345,
      "targetTile": 54321,
      "targetPlayerID": "player-abc",
      "tick": 950,
      "coordinates": {
        "x": 123,
        "y": 456
      }
    }
  }
}
```

**Data fields**:
- `nukeType`: Game unit type name ("Atom Bomb", "Hydrogen Bomb", "MIRV")
- `nukeUnitID`: Unique unit ID of the launched nuke (required for tracking)
- `targetTile`: Game tile ID being targeted
- `targetPlayerID`: ID of the target player
- `tick`: Game tick when nuke was launched
- `coordinates`: Map coordinates of target tile

**Hardware behavior**:
1. Button press detected on ATOM button
2. Firmware sends `NUKE_LAUNCHED` event to server (without unitID)
3. Server forwards to userscript
4. Userscript detects launch in game and sends event WITH nukeUnitID
5. Firmware receives event and:
   - Registers nuke in tracker with unitID
   - Turns LED ON (solid, not blinking)
   - LED stays ON until nuke explodes/intercepted
6. Multiple simultaneous nukes → LED stays ON until ALL resolved

#### `HYDRO_LAUNCHED`
Emitted when a hydrogen bomb is launched by the player.

```json
{
  "type": "event",
  "payload": {
    "type": "HYDRO_LAUNCHED",
    "timestamp": 1234567890,
    "message": "Hydrogen bomb launched",
    "data": {
      "nukeType": "Hydrogen Bomb",
      "nukeUnitID": 12346,
      "targetTile": 54322,
      "targetPlayerID": "player-def",
      "tick": 955,
      "coordinates": {
        "x": 124,
        "y": 457
      }
    }
  }
}
```

**Data fields**: Same as `NUKE_LAUNCHED` (see above).

**Hardware behavior**: Same as `NUKE_LAUNCHED` but for HYDRO button/LED.

#### `MIRV_LAUNCHED`
Emitted when a MIRV is launched by the player.

```json
{
  "type": "event",
  "payload": {
    "type": "MIRV_LAUNCHED",
    "timestamp": 1234567890,
    "message": "MIRV launched",
    "data": {
      "nukeType": "MIRV",
      "nukeUnitID": 12347,
      "targetTile": 54323,
      "targetPlayerID": "player-ghi",
      "tick": 960,
      "coordinates": {
        "x": 125,
        "y": 458
      }
    }
  }
}
```

**Data fields**: Same as `NUKE_LAUNCHED` (see above).

**Hardware behavior**: Same as `NUKE_LAUNCHED` but for MIRV button/LED.

**Important**: 
- LEDs do NOT respond to local button presses directly
- LEDs only activate when server confirms launch by echoing the event back
- Multiple buttons can blink simultaneously
- No cooldown restriction - rapid fire allowed

### Alert Module Events

#### `ALERT_ATOM`
Incoming atomic bomb threat detected.

**Flow**: Game → Userscript → Server → Firmware → Alert Module LEDs

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_ATOM",
    "timestamp": 1234567890,
    "message": "Incoming nuclear strike detected!",
    "data": {
      "nukeType": "Atom Bomb",
      "launcherPlayerID": "player-abc",
      "launcherPlayerName": "EnemyPlayer",
      "nukeUnitID": 12345,
      "targetTile": 54321,
      "targetPlayerID": "player-xyz",
      "tick": 950,
      "coordinates": {
        "x": 123,
        "y": 456
      }
    }
  }
}
```

**Data fields**:
- `nukeType`: Game unit type name ("Atom Bomb", "Hydrogen Bomb", "MIRV", "MIRV Warhead")
- `launcherPlayerID`: ID of the player who launched the nuke
- `launcherPlayerName`: Display name of the launching player
- `nukeUnitID`: Unique unit ID of the nuclear weapon
- `targetTile`: Game tile ID being targeted
- `targetPlayerID`: ID of the target player (current player)
- `tick`: Game tick when nuke was launched
- `coordinates`: Map coordinates of target tile

**Hardware behavior**:
1. Receive `ALERT_ATOM` event with nukeUnitID
2. Register incoming nuke in tracker
3. Turn ATOM alert LED ON (solid)
4. Activate WARNING LED when any alert active
5. LED stays ON until ALL atom nukes resolve (explode/intercept)
6. Multiple simultaneous nukes → LED stays ON until last one resolves
7. WARNING LED turns OFF when all nuke alerts cleared

#### `ALERT_HYDRO`
Incoming hydrogen bomb threat detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_HYDRO",
    "timestamp": 1234567890,
    "message": "Incoming hydrogen bomb detected!",
    "data": {
      "nukeType": "Hydrogen Bomb",
      "launcherPlayerID": "player-abc",
      "launcherPlayerName": "EnemyPlayer",
      "nukeUnitID": 12346,
      "targetTile": 54322,
      "targetPlayerID": "player-xyz",
      "tick": 955,
      "coordinates": {
        "x": 124,
        "y": 457
      }
    }
  }
}
```

**Data fields**: Same as `ALERT_ATOM` (see above).

**Hardware behavior**: Same as `ALERT_ATOM` but for HYDRO alert LED (15 seconds).

#### `ALERT_MIRV`
Incoming MIRV threat detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_MIRV",
    "timestamp": 1234567890,
    "message": "Incoming MIRV strike detected!",
    "data": {
      "nukeType": "MIRV",
      "launcherPlayerID": "player-def",
      "launcherPlayerName": "AnotherEnemy",
      "nukeUnitID": 12347,
      "targetTile": 54323,
      "targetPlayerID": "player-xyz",
      "tick": 960,
      "coordinates": {
        "x": 125,
        "y": 458
      }
    }
  }
}
```

**Data fields**: Same as `ALERT_ATOM` (see above).

**Hardware behavior**: Same as `ALERT_ATOM` but for MIRV alert LED (15 seconds).

#### `ALERT_LAND`
Land invasion detected.

**Flow**: Game → Userscript → Server → Firmware → Alert Module LEDs

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

**Data fields**:
- `type`: "land" (attack type identifier)
- `attackerPlayerID`: ID of the attacking player
- `attackerPlayerName`: Display name of the attacking player
- `attackID`: Unique ID of this attack in the game
- `troops`: Number of troops in the attack
- `targetPlayerID`: ID of the target player (current player)
- `tick`: Game tick when attack was launched

**Hardware behavior**: Same as `ALERT_ATOM` but for LAND alert LED (15 seconds).

#### `ALERT_NAVAL`
Naval invasion detected.

**Flow**: Game → Userscript → Server → Firmware → Alert Module LEDs

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
      "coordinates": {
        "x": 126,
        "y": 459
      }
    }
  }
}
```

**Data fields**:
- `type`: "boat" (attack type identifier)
- `attackerPlayerID`: ID of the attacking player
- `attackerPlayerName`: Display name of the attacking player
- `transportShipUnitID`: Unique unit ID of the transport ship
- `troops`: Number of troops being transported
- `targetTile`: Game tile ID being targeted
- `targetPlayerID`: ID of the target player (current player)
- `tick`: Game tick when transport ship launched
- `coordinates`: Map coordinates of target tile

**Hardware behavior**: Same as `ALERT_ATOM` but for NAVAL alert LED (15 seconds).

**Alert Module Behavior Summary**:
- WARNING LED: Activates when ANY other alert is active (300ms blink)
- Threat LEDs: Individual timeouts (10s for nukes, 15s for invasions, 500ms blink)
- Multiple alerts can be active simultaneously
- All alerts auto-expire after their timeout
- Output-only module (no commands sent to game)

### Sound Module Events

#### `SOUND_PLAY`
Emitted to request that the Sound Module play an audio asset.

**Flow**: Game/userscript → Server → Firmware → CAN → Sound Module

This is an **intent** event ("play this sound"), not a guarantee. The Sound Module may be powered off.

```json
{
  "type": "event",
  "payload": {
    "type": "SOUND_PLAY",
    "timestamp": 1234567890,
    "message": "Play alert sound",
    "data": {
      "soundId": "alert.atom",
      "priority": "high",
      "interrupt": true,
      "requestId": "evt-9f3c2d"
    }
  }
}
```

**Data fields**:
- `soundId` (string, required): Stable identifier for the requested sound. Examples:
  - `"alert.atom"`, `"alert.hydro"`, `"alert.mirv"`
  - `"nuke.launch.atom"`, `"nuke.launch.hydro"`, `"nuke.launch.mirv"`
  - `"game.start"`, `"game.win"`, `"game.lose"`
- `priority` ("low" | "normal" | "high", optional): Hint for playback policy.
- `interrupt` (boolean, optional): Hint indicating whether this sound should interrupt current playback.
- `requestId` (string, optional): Correlation ID for logs/acks (best-effort).
- `context` (object, optional): Any extra context for debugging (ignored by firmware).

**Userscript guidance**:
- Emit `SOUND_PLAY` for events you want to be audible.
- Prefer a **small, stable `soundId` set** (avoid embedding player names, etc.).
- Keep `data` small; firmware will typically use only `soundId`, `priority`, `interrupt`.

### Sound Catalog (Canonical Mapping Table)

This table is the single source of truth for mapping `soundId` → `soundIndex` and expected filenames.

Notes:
- **Sound Module (SD card)** uses `/sounds/NNNN.mp3` preferred, `/sounds/NNNN.wav` fallback.
- **Local dev assets (dashboard)** live in `ots-server/public/sounds/` and can include a descriptive suffix.

| soundId | soundIndex | Sound Module SD path | Local dev file (recommended) |
|---|---:|---|---|
| `game_start` | 1 | `/sounds/0001.mp3` (or `.wav`) | `ots-server/public/sounds/0001-game_start.mp3` |
| `game_player_death` | 2 | `/sounds/0002.mp3` (or `.wav`) | `ots-server/public/sounds/0002-game_player_death.mp3` |
| `game_victory` | 3 | `/sounds/0003.mp3` (or `.wav`) | `ots-server/public/sounds/0003-game_victory.mp3` |
| `game_defeat` | 4 | `/sounds/0004.mp3` (or `.wav`) | `ots-server/public/sounds/0004-game_defeat.mp3` |

#### `HARDWARE_DIAGNOSTIC`
Response event containing hardware diagnostic information from the device (firmware or simulator).

**Flow**: Device/Simulator → Server → Userscript (display in HUD or log)

```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_DIAGNOSTIC",
    "timestamp": 1234567890,
    "message": "Hardware diagnostic report",
    "data": {
      "version": "2025-12-20.1",
      "deviceType": "firmware",
      "serialNumber": "OTS-FW-001234",
      "owner": "PUSH Team",
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

**Data Fields:**
- `version` (string): Firmware version or software version (e.g., "2025-12-20.1")
- `deviceType` (string): `"firmware"` for ESP32 device, `"simulator"` for ots-server
- `serialNumber` (string): Unique device identifier (hardcoded, immutable)
  - Format: `OTS-FW-XXXXXX` for firmware devices
  - Format: `OTS-SIM-XXXXXX` for simulators
- `owner` (string): Device owner name (hardcoded, immutable, e.g., "PUSH Team", "DrZoid")
- `hardware` (object): Hardware component status
  - `lcd` (object): LCD display status
    - `present` (boolean): LCD hardware detected
    - `working` (boolean): LCD functional/responding
  - `inputBoard` (object): MCP23017 input board (buttons/sensors)
    - `present` (boolean): Input board detected on I2C bus
    - `working` (boolean): Input board responding correctly
  - `outputBoard` (object): MCP23017 output board (LEDs/relays)
    - `present` (boolean): Output board detected on I2C bus
    - `working` (boolean): Output board responding correctly
  - `adc` (object): ADS1015/ADS1115 ADC module
    - `present` (boolean): ADC detected on I2C bus
    - `working` (boolean): ADC providing valid readings
  - `soundModule` (object): Audio playback module
    - `present` (boolean): Sound module detected
    - `working` (boolean): Sound module operational

**Example - Firmware with sound module not yet implemented:**
```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_DIAGNOSTIC",
    "timestamp": 1735482000000,
    "message": "ESP32-S3 Hardware Diagnostic",
    "data": {
      "version": "2025-12-29.1",
      "deviceType": "firmware",
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

**Example - ots-server simulator with all features:**
```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_DIAGNOSTIC",
    "timestamp": 1735482000000,
    "message": "OTS Simulator Diagnostic",
    "data": {
      "version": "2025-12-20.1",
      "deviceType": "simulator",
      "serialNumber": "OTS-SIM-000001",
      "owner": "PUSH Team",
      "hardware": {
        "lcd": { "present": true, "working": true },
        "inputBoard": { "present": true, "working": true },
        "outputBoard": { "present": true, "working": true },
        "adc": { "present": true, "working": true },
        "soundModule": { "present": true, "working": true }
      }
    }
  }
}
```

**Usage:**
- Userscript can request diagnostic via `hardware-diagnostic` command
- Device responds with current hardware status
- Useful for debugging hardware issues remotely
- Serial number and owner are immutable identifiers set at compile time

### Nuke Outcome Events

#### `NUKE_EXPLODED`
Emitted when a tracked nuclear weapon successfully reaches its target.

**Flow**: Game → Userscript → Server → Dashboard (Info only)

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_EXPLODED",
    "timestamp": 1234567890,
    "message": "Nuclear weapon exploded",
    "data": {
      "nukeType": "Atom Bomb",
      "unitID": 12345,
      "ownerID": "player-abc",
      "ownerName": "PlayerName",
      "targetTile": 54321,
      "tick": 1000
    }
  }
}
```

**Hardware behavior**:
1. Receive explosion event with unitID
2. Look up nuke in tracker by unitID
3. Remove from tracker and update LED state
4. Alert LED turns OFF only when ALL nukes of that type resolved
5. Nuke module button LED turns OFF only when ALL outgoing nukes of that type resolved

**Note**: This event is sent for BOTH incoming and outgoing nukes. The firmware uses the `unitID` field to:
- Track individual nukes in flight
- Determine when to turn LEDs off (when count reaches 0)
- Support multiple simultaneous nukes of the same type

#### `NUKE_INTERCEPTED`
Emitted when a tracked nuclear weapon is destroyed before reaching its target.

**Flow**: Game → Userscript → Server → Dashboard (Info only)

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_INTERCEPTED",
    "timestamp": 1234567890,
    "message": "Nuclear weapon intercepted",
    "data": {
      "nukeType": "Hydrogen Bomb",
      "unitID": 12346,
      "ownerID": "player-xyz",
      "ownerName": "EnemyPlayer",
      "targetTile": 54322,
      "tick": 950
    }
  }
}
```

**Hardware behavior**: Same as `NUKE_EXPLODED` (see above).

### Hardware Testing

#### `HARDWARE_TEST`
Test event for hardware diagnostics.

```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_TEST",
    "timestamp": 1234567890,
    "message": "Hardware test initiated"
  }
}
```

### Info Events

#### `INFO`
General information events for logging and status.

```json
{
  "type": "event",
  "payload": {
    "type": "INFO",
    "timestamp": 1234567890,
    "message": "Descriptive message",
    "data": {
      // Optional structured data
    }
  }
}
```

**Common INFO message patterns from userscript**:

1. **Connection and Status**:
   - `"userscript-connected"` - Userscript established WebSocket connection
     ```json
     { "data": { "url": "https://openfront.io/game/..." } }
     ```
   - `"heartbeat"` - Periodic heartbeat (sent every 5 seconds)
   - `"pong-from-userscript"` - Response to ping command
   - `"Game instance detected"` - Game object became available
     ```json
     { "data": { "timestamp": 1234567890 } }
     ```

2. **Command Execution**:
   - `"Unknown command: {action}"` - Received unrecognized command
     ```json
     { "data": { "action": "unknown-action", "params": {...} } }
     ```
   - `"send-nuke failed: missing nukeType"` - Missing required parameter
   - `"send-nuke failed: game not available"` - Game object not ready
   - `"send-nuke failed: API not found"` - Game doesn't have nuke API
     ```json
     { 
       "data": { 
         "nukeType": "atom",
         "availableMethods": ["method1", "method2", ...]
       } 
     }
     ```

3. **Server-Generated**:
   - `"Command: {action}"` - Server logged a command execution
   - `"userscript-disconnected"` - Userscript peer disconnected
     ```json
     { "data": { "peerId": "peer-123" } }
     ```

---

## Userscript Event Detection

The userscript (`ots-userscript`) monitors the game in real-time and automatically detects threats and actions targeting the current player. It uses polling (100ms interval) to track game state changes.

### Nuke Detection (NukeTracker)

**What it tracks**: Nuclear weapons (Atom Bomb, Hydrogen Bomb, MIRV, MIRV Warhead) targeting the current player.

**Detection logic**:
1. Polls all nuke units in the game
2. For each new nuke, checks if `targetTile` belongs to current player
3. If yes, emits `ALERT_ATOM`/`ALERT_HYDRO`/`ALERT_MIRV` event
4. Continues tracking until:
   - Unit reaches target → emits `NUKE_EXPLODED`
   - Unit is deleted before target → emits `NUKE_INTERCEPTED`

**Event data includes**:
- Attacker player info (ID, name)
- Nuke details (type, unit ID, target coordinates)
- Game tick information

### Naval Detection (BoatTracker)

**What it tracks**: Transport ships targeting the current player's territory.

**Detection logic**:
1. Polls all Transport units in the game
2. For each new transport, checks if `targetTile` belongs to current player
3. If yes, emits `ALERT_NAVAL` event
4. Tracks until arrival or destruction (logged but no additional events)

**Event data includes**:
- Attacker player info (ID, name)
- Transport ship ID and troop count
- Target coordinates

### Land Attack Detection (LandAttackTracker)

**What it tracks**: Land invasions via the player's `incomingAttacks` list.

**Detection logic**:
1. Polls `myPlayer.incomingAttacks()` array
2. For each new attack, emits `ALERT_LAND` event
3. Tracks until attack disappears from list (completion or cancellation)

**Event data includes**:
- Attacker player info (ID, name)
- Attack ID and troop count
- Game tick information

### Polling Architecture

All trackers run on a shared 100ms polling interval:
```javascript
setInterval(() => {
  nukeTracker.detectLaunches(gameAPI, myPlayerID)
  nukeTracker.detectExplosions(gameAPI)
  
  boatTracker.detectLaunches(gameAPI, myPlayerID)
  boatTracker.detectArrivals(gameAPI)
  
  landTracker.detectLaunches(gameAPI)
  landTracker.detectCompletions(gameAPI)
}, 100)
```

**Important**: Events are only emitted for threats targeting the current player. Attacks on other players are ignored.

---

## Hardware Modules Protocol Reference

This section defines the complete protocol behavior for each hardware module.

### Module Sizes (Standard Units)

- **16U** - Standard full-height module (Nuke Module, Alert Module)
- **8U** - Half-height module (Main Power Module)

### Main Power Module (8U)

**Purpose**: Central power distribution and connection status indication.

**Hardware Components**:
- POWER LED (red) - Shows system power status (hardware-controlled, always on when powered)
- LINK LED (red) - Shows userscript WebSocket connection status (firmware-controlled)
- Power toggle switch - Controls 12V power to all modules

**Protocol Behavior**:
- **Input**: No commands sent (status indicator only)
- **Output**: No events received (LEDs reflect local state)
- **LINK LED**: ON when userscript heartbeat is active, OFF otherwise
- **Power state**: When OFF, all other modules are disabled

**Pin Mapping** (MCP23017 Board 0):
- LINK LED: Board 0, Pin 0

### Nuke Module (16U)

**Purpose**: Launch nuclear weapons via physical buttons with LED confirmation showing in-flight status.

**Hardware Components**:
- 3x Buttons: ATOM, HYDRO, MIRV (Board 0, Pins 1-3, INPUT_PULLUP active-low)
- 3x LEDs: ATOM, HYDRO, MIRV (Board 0, Pins 8-10)

**Protocol Behavior**:

**Outgoing (Button Press → Event)**:
1. Player presses button
2. Firmware debounces (50ms polling)
3. Sends event to server:
```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_LAUNCHED",  // or HYDRO_LAUNCHED, MIRV_LAUNCHED
    "timestamp": 1234567890
  }
}
```

**Incoming (Event → LED State Tracking)**:
1. Receive launch event from server with nukeUnitID
2. Register nuke in tracker (max 32 simultaneous nukes)
3. Turn LED ON (solid, not blinking)
4. Track all in-flight nukes of that type
5. On NUKE_EXPLODED/NUKE_INTERCEPTED:
   - Remove specific nuke from tracker by unitID
   - Turn LED OFF only when count reaches 0
6. Multiple simultaneous nukes → LED stays ON until ALL resolved

**Important**:
- LEDs show real-time in-flight nuke count
- LEDs are event-driven (only respond to server events)
- No cooldown restriction (rapid fire allowed)
- Supports multiple simultaneous nukes per type
- LED state persists across multiple launches

**Pin Mapping** (MCP23017 Board 0):
```cpp
// Buttons (INPUT_PULLUP, active-low)
BTN_ATOM  = {0, 1}
BTN_HYDRO = {0, 2}
BTN_MIRV  = {0, 3}

// LEDs
LED_ATOM  = {0, 8}
LED_HYDRO = {0, 9}
LED_MIRV  = {0, 10}
```

### Alert Module (16U)

**Purpose**: Visual threat notification system for incoming attacks with real-time tracking.

**Hardware Components**:
- 1x WARNING LED (large, 16x16, red)
- 5x Threat LEDs (8x8, red): ATOM, HYDRO, MIRV, LAND, NAVAL

**Protocol Behavior**:

**Incoming (Event → LED State Tracking)**:
Receives alert events from server (game-generated threats):

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_ATOM",  // or ALERT_HYDRO, ALERT_MIRV, ALERT_LAND, ALERT_NAVAL
    "timestamp": 1234567890,
    "message": "Incoming nuclear strike detected!",
    "data": {
      "nukeUnitID": 12345  // Required for nuke tracking
    }
  }
}
```

**LED Behavior (Nuclear Threats)**:
1. Receive alert event with nukeUnitID
2. Register incoming nuke in tracker (max 32 simultaneous)
3. Turn threat LED ON (solid)
4. Track all incoming nukes of that type
5. On NUKE_EXPLODED/NUKE_INTERCEPTED:
   - Remove specific nuke from tracker by unitID
   - Turn LED OFF only when count reaches 0
6. Multiple simultaneous nukes → LED stays ON until ALL resolved
7. WARNING LED turns ON when any threat LED active

**LED Behavior (Land/Naval Invasions)**:
- These use 15-second timeout (not tracked by nuke tracker)
- Simpler alerts that auto-expire after duration

**Important**:
- Output-only module (no commands sent)
- Nuclear threats tracked individually by unitID
- Multiple simultaneous threats supported
- WARNING LED is computed (on when any threat LED active)
- Nuke LEDs stay on until ALL nukes of that type resolved

**Pin Mapping** (MCP23017 Board 1):
```cpp
LED_WARNING = {1, 0}  // Large warning indicator
LED_ATOM    = {1, 1}  // Incoming nuke alerts
LED_HYDRO   = {1, 2}
LED_MIRV    = {1, 3}
LED_LAND    = {1, 4}  // Invasion alerts
LED_NAVAL   = {1, 5}
```

### Troops Module (16U)

**Purpose**: Real-time troop visualization and deployment percentage control.

**Hardware Components**:
- 1x LCD Display (2×16 character, I2C)
- 1x Potentiometer Slider (via I2C ADC - ADS1115)

**Protocol Behavior**:

**Incoming (State → Display Update)**:
Receives game state updates containing troop information:

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

**Display Format**:
```
120K / 1.1M     ← Line 1: current / max (with unit scaling)
50% (60K)       ← Line 2: percent (calculated amount)
```

**Unit Scaling**: K (thousands), M (millions), B (billions) with 1 decimal place

**Outgoing (Slider → Command)**:
When slider position changes by ≥1%, send command:

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

**Important**:
- Slider polling: 100ms debounce interval
- Command throttling: Only on ≥1% change
- Display updates: Immediate (no animations)
- Both input and output module

**I2C Addresses**:
```cpp
LCD_ADDRESS = 0x27  // (or 0x3F if conflict)
ADC_ADDRESS = 0x48  // ADS1115 ADC
```

**Hardware Connection**: Both devices on shared I2C bus (main 2×05 IDC header)

### Sound Module (8U)

**Purpose**: Play audio cues for important game events.

**Hardware Components**:
- ESP32-A1S (AI Thinker ESP32 Audio Kit v2.2) running `ots-fw-audiomodule`
- SD card with sound assets
- 3Ω speaker
- Volume potentiometer (local)
- On/off switch (hard power cut)

**Protocol Behavior**:

**Incoming (Event → CAN playback request)**:
- Firmware receives `SOUND_PLAY` events.
- Firmware maps `soundId` → `soundIndex` (numeric, local mapping table).
- Firmware sends CAN `PLAY_SOUND` frame to the Sound Module.

**CAN Bus (recommended defaults)**:
- CAN 2.0 (classic), 500 kbps, standard 11-bit IDs, 8-byte payloads.

**CAN IDs**:
- `0x420` PLAY_SOUND (main → sound)
- `0x421` STOP_SOUND (main → sound)
- `0x422` SOUND_STATUS (sound → main)
- `0x423` SOUND_ACK (sound → main, optional)

**PLAY_SOUND payload** (`0x420`, little-endian):
- `cmd=0x01`, `flags`, `soundIndex (u16)`, `volumeOverride (u8, 0xFF=ignore)`, `requestId (u16)`

**SD naming** (recommended):
- `/sounds/0020.mp3` preferred, `/sounds/0020.wav` fallback.

---

## Timing Constants

### Nuke Module
```cpp
#define NUKE_BLINK_DURATION_MS  10000  // 10 seconds
#define NUKE_BLINK_INTERVAL_MS  500    // 500ms (1Hz blink)
#define BUTTON_POLL_INTERVAL_MS 50     // 50ms debounce
```

### Alert Module
```cpp
// Alert durations (all alerts use 15 seconds for consistency)
#define ALERT_DURATION_ATOM   15000  // 15 seconds
#define ALERT_DURATION_HYDRO  15000  // 15 seconds
#define ALERT_DURATION_MIRV   15000  // 15 seconds
#define ALERT_DURATION_LAND   15000  // 15 seconds
#define ALERT_DURATION_NAVAL  15000  // 15 seconds

// Blink intervals
#define ALERT_BLINK_INTERVAL     500  // 500ms for threat LEDs
#define WARNING_BLINK_INTERVAL   300  // 300ms for WARNING LED (fast)
```

### Troops Module
```cpp
#define SLIDER_POLL_INTERVAL_MS  100   // 100ms debounce
#define SLIDER_CHANGE_THRESHOLD  1     // 1% minimum change to send command
#define DISPLAY_UPDATE_IMMEDIATE true  // No animation delay
```

---

## Server-Side Behavior (`ots-server`)

The Nuxt/Nitro server has dual roles:

### Role 1: WebSocket Server (for userscript and UI)

The server exposes a single WebSocket endpoint at `/api/ws` with client-type identification via handshake:

**Client Identification**:
```json
// Sent by client on connect
{
  "type": "handshake",
  "clientType": "userscript" | "ui"
}

// Server response
{
  "type": "handshake-ack",
  "clientType": "userscript" | "ui"
}
```

**Key responsibilities**:

1. **Event Broadcasting**:
   - Accept `event` messages from userscript (game-detected threats and actions)
   - Broadcast all events to ALL connected clients (userscript and UI)
   - Events flow: Userscript → Server → All clients (including back to userscript)

2. **Command Routing**:
   - Accept `cmd` messages from UI peers
   - Broadcast commands to ALL clients (userscript receives and executes)
   - Commands flow: UI → Server → All clients (userscript processes)

3. **Connection Management**:
   - Track client types (userscript vs UI)
   - Generate INFO events on userscript connect/disconnect:
     - `userscript-connected` when userscript connects
     - `userscript-disconnected` when userscript disconnects
   - Maintain separate pub/sub channels: `broadcast`, `ui`, `userscript`

4. **State Management** (future):
   - Keep latest `GameState` in memory (not yet implemented)
   - Broadcast state updates to UI clients

### Role 2: WebSocket Client (for hardware)

The server also acts as a **WebSocket client** connecting to the firmware's WebSocket server:

- Connects to hardware at configured address (e.g., `ws://192.168.1.100:8080`).
- Sends `state` and `event` messages to hardware (same format as received from userscript).
- Receives `cmd` messages from hardware modules and forwards them to userscript.

### Emulator Mode

When hardware is not available, `ots-server` can run in **emulator mode**:
- Simulates hardware firmware behavior
- Vue components in `app/components/hardware/` mimic physical modules
- User interactions in UI generate `cmd` messages as if from real hardware

---

## Firmware Behavior (`ots-fw-main`)

**Note**: The firmware acts as a **WebSocket server**, not a client. The `ots-server` connects TO the firmware.

The firmware will:

- Host a WebSocket server for the OTS backend to connect to.
- Encode/decode JSON messages that match the `IncomingMessage` / `OutgoingMessage` envelopes above.
- Receive from ots-server:
  - `state` messages containing current game state.
  - `event` messages for game events.
- Send to ots-server:
  - `cmd` messages from hardware modules (button presses, sensor inputs, etc.).
  - `event` messages for hardware-specific events (errors, status changes).

The firmware distributes received game state to hardware modules via I2C/CAN bus (see `/ots-hardware/hardware-spec.md`).

Implementation details (e.g., ArduinoJson, ESP-IDF WebSocket server, etc.) are up to the firmware but must not change the JSON payloads defined here.

---

## Versioning & Evolution

- This document is the **single source of truth** for the protocol.
- When adding new features (new `GameEventType`, new command actions, new fields):
  - Update this file with the new shapes/semantics.
  - Then update:
    - `ots-shared/src/game.ts` (TypeScript types)
    - `ots-server` routes and composables
    - `ots-userscript` message producers/consumers
    - `ots-fw-main` C++ structures and parsers
- Wherever possible, extend the protocol in a **backwards-compatible** way (e.g., optional fields, new event types) instead of breaking changes.
