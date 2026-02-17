# Main Controller Firmware Update Plan

This document outlines the changes needed in **ots-fw-main** firmware to support the keypad module.

## Current Status (Feb 2026)

- ✅ Implemented in fw-main as `keypad_module` (module architecture)
- ✅ CAN key events (0x431-0x43F) forwarded to WebSocket as keypad events
- ✅ WebSocket payload aligned with spec: `keyId`, `state`, `timestamp`
- ✅ CAN discovery integration emits `KEYPAD_CONNECTED` / `KEYPAD_DISCONNECTED`
- ⏳ Remaining: physical end-to-end validation with real keypad hardware

## Overview

The main controller must:
1. Receive raw key events from keypad via CAN bus (key ID 1-15, press/release)
2. Forward key events to WebSocket clients (userscript, dashboard)
3. Optionally: Send LED state commands to keypad based on game state (TBD)

**Note**: Key mapping is handled entirely by the userscript, not the main controller. The controller just forwards raw key events.

---

## 1. Keypad Module (`keypad_module.c/h`)

**Location**: `ots-fw-main/src/keypad_module.c`

### Responsibilities:
- Listen for keypad key events via CAN bus
- Forward raw key events to WebSocket clients
- No mapping logic (userscript handles this)
- Optionally handle LED commands (future feature)

### Data Structures:

```c
typedef struct {
    uint8_t key_id;           // 1-15
    uint8_t state;            // 0=released, 1=pressed
    uint16_t timestamp;       // ms since boot
} keypad_event_t;
```

### API / Module Interface:

```c
extern hardware_module_t keypad_module;
// Registered by module_manager and updated in periodic module task
```

### Event Flow:

```
CAN: KEY_EVENT (K5, PRESSED) 
  → keypad_module CAN RX handler
  → websocket_broadcast(KEYPAD_KEY_PRESSED, {keyId: 5, state: "pressed"})
  → Userscript receives event
  → Userscript maps K5 → game action
  → Userscript sends action to game
```

### CAN Message Handling:

Uses `can_protocol_keypad` shared component and `can_bus_manager` for RX dispatch.

**Pattern** (same module architecture as nuke/alert/sound modules):
```c
// In keypad_module_init():
// Register handlers for keypad CAN IDs
for (uint8_t key_id = 1; key_id <= 15; key_id++) {
    uint32_t can_id = CAN_ID_KEY_EVENT_BASE + key_id;
    can_bus_manager_register_handler(can_id, 0x7FF, on_key_event, NULL);
}

// RX handler callback
static void on_key_event(const can_frame_t *frame, void *ctx) {
    uint8_t key_id, state;
    uint16_t timestamp;
    
    if (can_keypad_parse_key_event(frame, &key_id, &state, &timestamp)) {
        keypad_broadcast_event(key_id, state == 1, timestamp);
    }
}
```

---

## 2. WebSocket Broadcast Module (existing)

**Location**: Existing WebSocket handler code

### Responsibilities:
- Broadcast keypad events to all connected clients
- Forward raw key press/release events
- No interpretation or mapping

### Implementation:

```c
void keypad_broadcast_event(uint8_t key_id, bool pressed, uint16_t timestamp) {
    cJSON *event = cJSON_CreateObject();
    cJSON_AddStringToObject(event, "event", pressed ? "KEYPAD_KEY_PRESSED" : "KEYPAD_KEY_RELEASED");
    
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "keyId", key_id);
    cJSON_AddStringToObject(data, "state", pressed ? "pressed" : "released");
    cJSON_AddNumberToObject(data, "timestamp", timestamp);
    cJSON_AddItemToObject(event, "data", data);
    
    websocket_broadcast_json(event);
    cJSON_Delete(event);
}
```

---

## 3. Module Manager Integration

Register keypad module in `main.c` with other modules:

```c
module_manager_register(&keypad_module);
```

Subscribe to relevant events:
- `INTERNAL_EVENT_CAN_MESSAGE` (keypad key events from CAN bus)
- Forward to WebSocket clients immediately

---

## 4. Files to Modify/Create

### New Files (ots-fw-main):
- `src/keypad_module.c` + `include/keypad_module.h`

### Modified Files (ots-fw-main):
- `src/main.c` - Register keypad module
- `include/protocol.h` - Add keypad events
- `src/protocol.c` - Add string conversions for keypad events
- `src/CMakeLists.txt` - Add keypad_module.c + can_protocol_keypad dependency

### New Shared Component:
- `ots-fw-shared/components/can_protocol_keypad/` (CAN message definitions)

### Existing Shared Components Used:
- `can_bus_manager` - CAN bus manager with RX dispatch (already initialized)
- `can_protocol_discovery` - Module discovery (already used for audio module)

---

## 5. CAN Protocol Component

**Component**: `can_protocol_keypad`  
**Location**: `ots-fw-shared/components/can_protocol_keypad/`

### Purpose:
Shared CAN message definitions for keypad ↔ controller communication.

### Files:
```
can_protocol_keypad/
  include/
    can_protocol_keypad.h    - Message structs, CAN IDs, helpers
  CMakeLists.txt
  COMPONENT_PROMPT.md        - Documentation
```

### CAN Message Definitions:

```c
// CAN IDs (0x430-0x43F range for keypad)
#define CAN_ID_KEYPAD_EVENT_BASE    0x430  // 0x430 + key_id (1-15)

// Key event message (keypad → controller)
typedef struct {
    uint8_t key_id;      // 1-15
    uint8_t state;       // 0=released, 1=pressed
    uint16_t timestamp;  // ms since boot
} __attribute__((packed)) can_keypad_event_t;
```

**Note**: LED messages (0x430, 0x440) are reserved for future features.

---

## Implementation Order

### Phase 1: CAN Protocol Component
1. ✅ Create `ots-fw-shared/components/can_protocol_keypad/`
2. ✅ Define message structures

### Phase 2: Main Controller - Key Event Forwarding
1. ✅ Implement `keypad_module.c` (receive key events from CAN)
2. ✅ Register keypad module in module manager flow (`main.c`)
3. ✅ Add WebSocket broadcast for key events
4. ✅ Add WebSocket broadcast for keypad connected/disconnected from CAN discovery
5. ⏳ Pending: Test on physical hardware (keypad press → WebSocket client)

### Phase 3: Userscript Integration
1. ⏳ Pending in this stage plan (see `07-userscript-integration.md`)
2. ⏳ Userscript key mapping and UI verification tracked separately

---

## Testing Requirements

### Integration Tests:
- CAN bus communication with real keypad hardware
- Key event broadcast via WebSocket to all clients
- Userscript receives events and maps to game actions

### Hardware Tests:
- Physical keypad connected via CAN
- All 15 keys trigger WebSocket events
- Events arrive at userscript with <50ms latency

---

## Related Documentation

- **Keypad Firmware**: `01-keypad-firmware-architecture.md`
- **CAN Bus Protocol**: `/prompts/CANBUS_MESSAGE_SPEC.md`
- **WebSocket Protocol**: `04-websocket-protocol-integration.md`
- **Userscript Integration**: `07-userscript-integration.md`
- **Module System**: `ots-fw-main/docs/MODULE_SYSTEM.md`
