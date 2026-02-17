# Main Controller Firmware Update Plan

This document outlines the changes needed in **ots-fw-main** firmware to support the keypad module.

## Overview

The main controller must:
1. Receive raw key events from keypad via CAN bus (key ID 1-15, press/release)
2. Forward key events to WebSocket clients (userscript, dashboard)
3. Optionally: Send LED state commands to keypad based on game state (TBD)

**Note**: Key mapping is handled entirely by the userscript, not the main controller. The controller just forwards raw key events.

---

## 1. Keypad CAN Handler (`keypad_can_handler.c/h`)

**Location**: `ots-fw-main/src/keypad_can_handler.c`

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

### API:

```c
esp_err_t keypad_can_handler_init(void);
void keypad_can_handler_process_event(const can_msg_key_event_t *event);
// Forward to WebSocket - no mapping logic
```

### Event Flow:

```
CAN: KEY_EVENT (K5, PRESSED) 
  → keypad_can_handler_process_event()
  → websocket_broadcast(KEYPAD_KEY_PRESSED, {keyId: 5, state: "pressed"})
  → Userscript receives event
  → Userscript maps K5 → game action
  → Userscript sends action to game
```

### CAN Message Handling:

Uses `can_protocol_keypad` shared component and `can_bus_manager` for RX dispatch.

**Pattern** (same as sound module):
```c
// In keypad_can_handler_init():
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

Register keypad handlers in `module_manager.c`:

```c
// In module_manager_init():
keypad_can_handler_init();
```

Subscribe to relevant events:
- `INTERNAL_EVENT_CAN_MESSAGE` (keypad key events from CAN bus)
- Forward to WebSocket clients immediately

---

## 4. Files to Modify/Create

### New Files (ots-fw-main):
- `src/keypad_can_handler.c` + `include/keypad_can_handler.h`

### Modified Files (ots-fw-main):
- `src/module_manager.c` - Register keypad CAN handler
- `include/protocol.h` - Add KEYPAD_KEY_PRESSED/RELEASED events only
- `src/protocol.c` - Add string conversions for key events
- `src/websocket_handler.c` - Broadcast keypad events
- `src/CMakeLists.txt` - Add keypad_can_handler.c

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
1. ✅ Implement `keypad_can_handler.c` (receive key events from CAN)
2. ✅ Register in module manager
3. ✅ Add WebSocket broadcast for key events
4. ✅ Test: Keypad press → WebSocket broadcast

### Phase 3: Userscript Integration
1. ✅ Userscript receives KEYPAD_KEY_PRESSED/RELEASED events
2. ✅ Userscript handles key mapping (localStorage)
3. ✅ Userscript configuration UI (new tab)
4. ✅ See `07-userscript-integration.md` for details

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
