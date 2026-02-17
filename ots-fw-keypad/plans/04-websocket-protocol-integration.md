# WebSocket Protocol Integration - Keypad Module

This document defines the WebSocket protocol updates needed to support keypad configuration and real-time status in the dashboard.

## Overview

The keypad module requires WebSocket communication for:
- **Main controller firmware** → **Dashboard UI** (key event visualization)
- **Main controller firmware** → **Userscript** (key events for mapping)

**Key mapping is handled entirely by the userscript** (localStorage + UI), not by firmware or dashboard.

## Files to Update

### 1. `/prompts/WEBSOCKET_MESSAGE_SPEC.md`
**Purpose**: Single source of truth for all WebSocket message formats

**Changes Required**:
- Add keypad key event types (press/release only)
- Remove binding configuration messages (userscript handles this)
- Optionally add LED state sync for dashboard visualization

### 2. TypeScript Types (`ots-shared/src/game.ts`)
**Changes Required**:
- Add keypad event enums
- Add key binding data structures
- Export types for dashboard/userscript

### 3. Firmware Protocol (`ots-fw-main/include/protocol.h`)
**Changes Required**:
- Add C enums for keypad events
- Add struct definitions for message payloads

---

## New WebSocket Events

### Event Type Additions

Add to `GameEventType` enum:

```typescript
// Keypad events (real-time, broadcast only)
KEYPAD_KEY_PRESSED = 'KEYPAD_KEY_PRESSED',
KEYPAD_KEY_RELEASED = 'KEYPAD_KEY_RELEASED',

// Keypad status (optional)
KEYPAD_CONNECTED = 'KEYPAD_CONNECTED',
KEYPAD_DISCONNECTED = 'KEYPAD_DISCONNECTED',
```

**Note**: No binding configuration events - userscript handles key mapping locally.

---

## Message Format Definitions

### 1. KEYPAD_KEY_PRESSED / KEYPAD_KEY_RELEASED

**Direction**: Main Controller → All Clients (broadcast)

**Purpose**: Real-time key event notification

**Note**: Userscript receives these events and maps them to game actions locally.

**Payload**:
```typescript
interface KeypadKeyEventData {
  keyId: number;           // 1-15
  state: 'pressed' | 'released';
  timestamp: number;       // Unix timestamp (ms)
}
```

**Example**:
```json
{
  "event": "KEYPAD_KEY_PRESSED",
  "timestamp": 1234567890123,
  "data": {
    "keyId": 1,
    "state": "pressed",
    "timestamp": 1234567890123
  }
}
```

**Note**: Key 1 = "Build City" in default userscript configuration. Firmware/controller only know raw keyId.

---

### 2. KEYPAD_CONNECTED / KEYPAD_DISCONNECTED

**Direction**: Main Controller → All Clients (broadcast)

**Purpose**: Notify when keypad connects/disconnects from CAN bus

**Payload**:
```typescript
interface KeypadConnectionData {
  firmwareVersion?: string; // Only for CONNECTED event
  timestamp: number;        // When event occurred
}
```

**Example (Connected)**:
```json
{
  "event": "KEYPAD_CONNECTED",
  "timestamp": 1234567890123,
  "data": {
    "firmwareVersion": "0.1.0-dev",
    "timestamp": 1234567890123
  }
}
```

---

## Implementation Steps

### Step 1: Update TypeScript Types

**File**: `ots-shared/src/game.ts`

```typescript
// Add to GameEventType enum
export enum GameEventType {
  // ... existing events
  
  // Keypad events (minimal set)
  KEYPAD_KEY_PRESSED = 'KEYPAD_KEY_PRESSED',
  KEYPAD_KEY_RELEASED = 'KEYPAD_KEY_RELEASED',
  KEYPAD_CONNECTED = 'KEYPAD_CONNECTED',
  KEYPAD_DISCONNECTED = 'KEYPAD_DISCONNECTED',
}

// Add data structures
export interface KeypadKeyEventData {
  keyId: number;
  state: 'pressed' | 'released';
  timestamp: number;
}

export interface KeypadConnectionData {
  firmwareVersion?: string;
  timestamp: number;
}

// Update GameEvent discriminated union
export type GameEvent =
  | { event: GameEventType.KEYPAD_KEY_PRESSED; data: KeypadKeyEventData }
  | { event: GameEventType.KEYPAD_KEY_RELEASED; data: KeypadKeyEventData }
  | { event: GameEventType.KEYPAD_CONNECTED; data: KeypadConnectionData }
  | { event: GameEventType.KEYPAD_DISCONNECTED; data: KeypadConnectionData }
  // ... existing event types
```

---

### Step 2: Update Firmware Protocol

**File**: `ots-fw-main/include/protocol.h`

```c
// Add to game_event_type_t enum
typedef enum {
    // ... existing events
    
    // Keypad events (minimal set)
    GAME_EVENT_KEYPAD_KEY_PRESSED,
    GAME_EVENT_KEYPAD_KEY_RELEASED,
    GAME_EVENT_KEYPAD_CONNECTED,
    GAME_EVENT_KEYPAD_DISCONNECTED,
} game_event_type_t;
```

**File**: `ots-fw-main/src/protocol.c`

Add string conversions for new event types.

---

### Step 3: Main Controller WebSocket Handler

**File**: `ots-fw-main/src/websocket_handler.c`

Broadcast handlers:
- On key event received from CAN → Broadcast `KEYPAD_KEY_PRESSED/RELEASED`
- On keypad CAN discovery → Broadcast `KEYPAD_CONNECTED`
- On keypad timeout → Broadcast `KEYPAD_DISCONNECTED`

**No handlers needed for:**
- Binding configuration (userscript handles locally)
- LED control (future feature)

---

## Testing

### Test 1: Key Event Broadcast
1. Press physical key on keypad
2. Verify WebSocket message broadcast to all clients
3. Check dashboard UI updates in real-time
4. Check userscript receives event

### Test 2: Userscript Receives Events
1. Userscript connects to WebSocket
2. Press key on keypad
3. Verify userscript receives KEYPAD_KEY_PRESSED
4. Verify userscript maps to game action
5. Verify game action sent to game

---

## Related Documentation

- **Protocol Spec**: `/prompts/WEBSOCKET_MESSAGE_SPEC.md`
- **Main Controller Plan**: `02-main-controller-integration.md`
- **Dashboard UI Plan**: `05-dashboard-ui-integration.md` (visualization only)
- **Userscript Plan**: `07-userscript-integration.md` (key mapping logic)
- **TypeScript Types**: `ots-shared/src/game.ts`
