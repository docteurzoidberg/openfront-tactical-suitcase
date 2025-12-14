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
  hwState: HWState;       // hardware module states
}
```

### GameEvent

Represents a single event in the game.

```ts
GameEventType =
  | "INFO"
  | "GAME_START"
  | "GAME_END"
  | "WIN"
  | "LOOSE"
  | "NUKE_LAUNCHED"
  | "HYDRO_LAUNCHED"
  | "MIRV_LAUNCHED"
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

**Flow**: Hardware → Server → Userscript → Game

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
1. Receive `send-nuke` command
2. Call game's nuke method with specified type
3. Send back `nuke-sent` event (see Event Definitions below)

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
Emitted when a game ends.

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "Game ended"
  }
}
```

#### `WIN`
Emitted when the player wins.

```json
{
  "type": "event",
  "payload": {
    "type": "WIN",
    "timestamp": 1234567890,
    "message": "Victory!"
  }
}
```

#### `LOOSE`
Emitted when the player loses.

```json
{
  "type": "event",
  "payload": {
    "type": "LOOSE",
    "timestamp": 1234567890,
    "message": "Defeat"
  }
}
```

### Nuke Module Events

#### `NUKE_LAUNCHED`
Emitted when an atomic bomb is launched.

**Flow**: Hardware Button → Firmware → Server → Userscript → Game → Server → Hardware LED

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_LAUNCHED",
    "timestamp": 1234567890,
    "message": "Atomic bomb launched"
  }
}
```

**Hardware behavior**:
1. Button press detected on ATOM button
2. Firmware sends `NUKE_LAUNCHED` event to server
3. Server forwards to userscript
4. Userscript calls game nuke method
5. Server echoes event back to firmware
6. Firmware triggers ATOM LED blink for 10 seconds (500ms interval)

#### `HYDRO_LAUNCHED`
Emitted when a hydrogen bomb is launched.

```json
{
  "type": "event",
  "payload": {
    "type": "HYDRO_LAUNCHED",
    "timestamp": 1234567890,
    "message": "Hydrogen bomb launched"
  }
}
```

**Hardware behavior**: Same as `NUKE_LAUNCHED` but for HYDRO button/LED.

#### `MIRV_LAUNCHED`
Emitted when a MIRV is launched.

```json
{
  "type": "event",
  "payload": {
    "type": "MIRV_LAUNCHED",
    "timestamp": 1234567890,
    "message": "MIRV launched"
  }
}
```

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
    "message": "Incoming nuclear strike detected!"
  }
}
```

**Hardware behavior**:
1. Receive `ALERT_ATOM` event
2. Activate ATOM alert LED with pulse animation
3. Activate WARNING LED (fast blink when any alert active)
4. Auto-expire after 10 seconds
5. WARNING LED remains active if other alerts still active

#### `ALERT_HYDRO`
Incoming hydrogen bomb threat detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_HYDRO",
    "timestamp": 1234567890,
    "message": "Incoming hydrogen bomb detected!"
  }
}
```

**Hardware behavior**: Same as `ALERT_ATOM` but for HYDRO alert LED (10 seconds).

#### `ALERT_MIRV`
Incoming MIRV threat detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_MIRV",
    "timestamp": 1234567890,
    "message": "Incoming MIRV strike detected!"
  }
}
```

**Hardware behavior**: Same as `ALERT_ATOM` but for MIRV alert LED (10 seconds).

#### `ALERT_LAND`
Land invasion detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_LAND",
    "timestamp": 1234567890,
    "message": "Land invasion detected!"
  }
}
```

**Hardware behavior**: Same as `ALERT_ATOM` but for LAND alert LED (15 seconds).

#### `ALERT_NAVAL`
Naval invasion detected.

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_NAVAL",
    "timestamp": 1234567890,
    "message": "Naval invasion detected!"
  }
}
```

**Hardware behavior**: Same as `ALERT_ATOM` but for NAVAL alert LED (15 seconds).

**Alert Module Behavior Summary**:
- WARNING LED: Activates when ANY other alert is active (300ms blink)
- Threat LEDs: Individual timeouts (10s for nukes, 15s for invasions, 500ms blink)
- Multiple alerts can be active simultaneously
- All alerts auto-expire after their timeout
- Output-only module (no commands sent to game)

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

**Common INFO message patterns**:
- `"Nuke sent"` - Legacy nuke confirmation (with `data.nukeType`)
- `"userscript-connected"` - Userscript established connection
- `"pong-from-userscript"` - Response to ping command
- `"Command: {action}"` - Server logged a command

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

**Purpose**: Launch nuclear weapons via physical buttons with LED confirmation.

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

**Incoming (Event → LED Blink)**:
1. Receive launch event from server (game confirmation)
2. Activate corresponding LED
3. Blink pattern: 500ms ON / 500ms OFF
4. Duration: 10 seconds
5. Auto-off after timeout

**Important**:
- LEDs are event-driven (only respond to server events, not local presses)
- No cooldown restriction (rapid fire allowed)
- Multiple LEDs can blink simultaneously

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

**Purpose**: Visual threat notification system for incoming attacks.

**Hardware Components**:
- 1x WARNING LED (large, 16x16, red)
- 5x Threat LEDs (8x8, red): ATOM, HYDRO, MIRV, LAND, NAVAL

**Protocol Behavior**:

**Incoming (Event → LED Activation)**:
Receives alert events from server (game-generated threats):

```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_ATOM",  // or ALERT_HYDRO, ALERT_MIRV, ALERT_LAND, ALERT_NAVAL
    "timestamp": 1234567890,
    "message": "Incoming nuclear strike detected!"
  }
}
```

**LED Behavior**:
1. Activate corresponding threat LED
2. Activate WARNING LED (auto-managed)
3. Blink pattern:
   - Threat LEDs: 500ms ON / 500ms OFF
   - WARNING LED: 300ms ON / 300ms OFF (faster)
4. Duration:
   - Nuke alerts (ATOM/HYDRO/MIRV): 10 seconds
   - Invasion alerts (LAND/NAVAL): 15 seconds
5. Auto-expire after timeout
6. WARNING LED remains active while ANY threat LED is active

**Important**:
- Output-only module (no commands sent)
- Multiple alerts can be active simultaneously
- WARNING LED is computed (on when any other alert active)
- Each alert has independent timeout

**Pin Mapping** (MCP23017 Board 1):
```cpp
LED_WARNING = {1, 0}  // Large warning indicator
LED_ATOM    = {1, 1}  // Incoming nuke alerts
LED_HYDRO   = {1, 2}
LED_MIRV    = {1, 3}
LED_LAND    = {1, 4}  // Invasion alerts
LED_NAVAL   = {1, 5}
```

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
// Alert durations
#define ALERT_DURATION_ATOM   10000  // 10 seconds
#define ALERT_DURATION_HYDRO  10000  // 10 seconds
#define ALERT_DURATION_MIRV   10000  // 10 seconds
#define ALERT_DURATION_LAND   15000  // 15 seconds
#define ALERT_DURATION_NAVAL  15000  // 15 seconds

// Blink intervals
#define ALERT_BLINK_INTERVAL     500  // 500ms for threat LEDs
#define WARNING_BLINK_INTERVAL   300  // 300ms for WARNING LED (fast)
```

---

## Server-Side Behavior (`ots-server`)

The Nuxt/Nitro server has dual roles:

### Role 1: WebSocket Server (for userscript and UI)

The server exposes:

- `/ws-script` – WebSocket endpoint for game bridges (userscript).
- `/ws-ui` – WebSocket endpoint for dashboard UI clients.

Key responsibilities:

- Accept `IncomingMessage` from userscript (game state and events).
- Keep the latest `GameState` in memory.
- Broadcast `state` and `event` messages to all subscribed UI peers via a dedicated channel (e.g. `"ui"`).
- Forward `cmd` messages from UI peers to userscript via another channel (e.g. `"script"`).

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
