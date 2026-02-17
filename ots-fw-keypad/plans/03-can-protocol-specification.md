# CAN Bus Specification Update Plan - Keypad Module

This document outlines the changes needed to the CAN bus specification to support the keypad module.

## Overview

The keypad module requires bidirectional CAN communication:
- **Outgoing**: Raw key events (K1-K15, press/release)
- **Incoming**: LED control commands (set color, on/off)
- **Discovery**: Module announcement (reuses existing discovery protocol)

## Files to Update

### 1. `/prompts/CANBUS_MESSAGE_SPEC.md`
**Purpose**: Single source of truth for all CAN message formats

**Changes Required**:
- Add keypad module type ID (`0x05`)
- Define CAN ID range for keypad messages (`0x300-0x311`)
- Add key event message format (outgoing)
- Add LED control message formats (incoming)
- Update CAN ID allocation table

### 2. `/doc/developer/canbus-protocol.md`
**Purpose**: Developer implementation guide with C code examples

**Changes Required**:
- Add C implementation examples for keypad messages
- Add debugging tips for keypad-specific issues
- Add timing/latency requirements for LED updates

---

## Detailed Changes

### Change 1: Module Type ID

**Add to CANBUS_MESSAGE_SPEC.md** (Module Types section):

```markdown
### Module Types

| ID | Module | Description |
|----|--------|-------------|
| 0x01 | Audio Module | Sound playback via CAN commands |
| 0x02 | Alert Module | Incoming threat indicators |
| 0x03 | Nuke Module | Launch buttons and LEDs |
| 0x04 | Main Power | Power distribution and status |
| 0x05 | Keypad Module | 15-key RGB keyboard (NEW) |
```

---

### Change 2: CAN ID Allocation

**Add to CANBUS_MESSAGE_SPEC.md** (CAN ID Ranges section):

```markdown
### CAN ID Allocation

| Range | Purpose | Description |
|-------|---------|-------------|
| 0x000-0x0FF | Reserved | Standard CAN IDs |
| 0x100-0x1FF | Audio Module | Sound commands and status |
| 0x200-0x21F | Keypad Module | Key events and LED control (NEW) |
| 0x300-0x7EF | Reserved | Future modules |
| 0x7F0-0x7FF | System | Discovery, diagnostics, broadcast |
```

**Specific Keypad IDs**:
```markdown
| CAN ID | Direction | Purpose |
|--------|-----------|---------|
| 0x200 | Reserved | Module base ID |
| 0x201-0x20F | Keypad → Controller | Key events (0x200 + key_id) |
| 0x210 | Controller → Keypad | LED set (single key) |
| 0x211 | Controller → Keypad | LED set (bulk/bitmask) |
| 0x212-0x21F | Reserved | Future keypad features |
```

---

### Change 3: Key Event Message

**Add to CANBUS_MESSAGE_SPEC.md** (Messages section):

```markdown
## Keypad Module Messages

### KEY_EVENT (Keypad → Controller)

**Purpose**: Report key press/release events

**CAN ID**: `0x201` to `0x20F` (0x200 + key_id)
- Key 1: 0x201
- Key 2: 0x202
- ...
- Key 15: 0x20F

**DLC**: 4 bytes

**Data Format**:
```
Byte 0: Key ID (1-15)
Byte 1: State (0x00=released, 0x01=pressed)
Byte 2: Timestamp LSB (milliseconds since boot, low byte)
Byte 3: Timestamp MSB (milliseconds since boot, high byte)
```

**C Structure**:
```c
typedef struct {
    uint8_t key_id;      // 1-15
    uint8_t state;       // 0=released, 1=pressed
    uint16_t timestamp;  // ms since boot (for debounce verification)
} __attribute__((packed)) can_msg_key_event_t;
```

**Example**: Key 1 pressed at 1234ms (Build City)
```
CAN ID: 0x201
DLC: 4
Data: [01 01 D2 04]
      └─┘ └┘ └──┴─┘
       |  |    └─ 0x04D2 = 1234ms
       |  └─ Pressed
       └─ Key 1
```

**Example**: Key 8 pressed at 5678ms (Zoom In)
```
CAN ID: 0x208
DLC: 4
Data: [08 01 2E 16]
      └─┘ └┘ └──┴─┘
       |  |    └─ 0x162E = 5678ms
       |  └─ Pressed
       └─ Key 8
```

**Example**: Key 15 pressed at 9012ms (Toggle View / Spacebar)
```
CAN ID: 0x20F
DLC: 4
Data: [0F 01 34 23]
      └─┘ └┘ └──┴─┘
       |  |    └─ 0x2334 = 9012ms
       |  └─ Pressed
       └─ Key 15
```

**Timing**: 
- Sent immediately after debounce period (20ms)
- Maximum rate: ~50 events/second per key (limited by human input)
- Latency: <5ms from physical press to CAN transmission
```

---

### Change 4: LED Set (Single Key) Message

**Add to CANBUS_MESSAGE_SPEC.md** (Messages section):

```markdown
### LED_SET (Controller → Keypad)

**Purpose**: Set RGB color and state for a single key LED

**CAN ID**: `0x210`

**DLC**: 5 bytes

**Data Format**:
```
Byte 0: Key ID (1-15, or 0xFF for all keys)
Byte 1: State (0x00=off, 0x01=on)
Byte 2: Red component (0-255)
Byte 3: Green component (0-255)
Byte 4: Blue component (0-255)
```

**C Structure**:
```c
typedef struct {
    uint8_t key_id;      // 1-15 (0xFF = all)
    uint8_t state;       // 0=off, 1=on
    uint8_t r, g, b;     // RGB color
} __attribute__((packed)) can_msg_led_set_t;
```

**Example 1**: Set Key 5 to green, ON
```
CAN ID: 0x210
DLC: 5
Data: [05 01 00 FF 00]
      └─┘ └┘ └──┴──┴─┘
       |  |    └─ RGB: (0, 255, 0) = Green
       |  └─ On
       └─ Key 5
```

**Example 2**: Turn off all keys
```
CAN ID: 0x210
DLC: 5
Data: [FF 00 00 00 00]
      └─┘ └┘ └──┴──┴─┘
       |  |    └─ RGB: (0, 0, 0) = Black
       |  └─ Off
       └─ All keys (0xFF)
```

**Timing**:
- LED update latency: <10ms from message receipt to physical LED
- No acknowledgment required
- Fire-and-forget (controller assumes success)
```

---

### Change 5: LED Set (Bulk) Message

**Add to CANBUS_MESSAGE_SPEC.md** (Messages section):

```markdown
### LED_SET_BULK (Controller → Keypad)

**Purpose**: Set RGB color and state for multiple keys using bitmask

**CAN ID**: `0x211`

**DLC**: 6 bytes

**Data Format**:
```
Byte 0: Bitmask LSB (Keys 1-8)
Byte 1: Bitmask MSB (Keys 9-15)
Byte 2: State (0x00=off, 0x01=on)
Byte 3: Red component (0-255)
Byte 4: Green component (0-255)
Byte 5: Blue component (0-255)
```

**Bitmask Layout**:
```
Byte 0 (LSB):  Bit 0=K1, Bit 1=K2, ..., Bit 7=K8
Byte 1 (MSB):  Bit 0=K9, Bit 1=K10, ..., Bit 6=K15, Bit 7=unused
```

**C Structure**:
```c
typedef struct {
    uint16_t key_bitmask;  // Bit 0=K1, Bit 14=K15
    uint8_t state;         // 0=off, 1=on
    uint8_t r, g, b;       // RGB color
} __attribute__((packed)) can_msg_led_bulk_t;
```

**Example 1**: Set Keys 1, 2, 3 to red, ON
```
Bitmask: 0b0000000000000111 = 0x0007
CAN ID: 0x211
DLC: 6
Data: [07 00 01 FF 00 00]
      └──┴─┘ └┘ └──┴──┴─┘
        |   |    └─ RGB: (255, 0, 0) = Red
        |   └─ On
        └─ Keys 1,2,3 (bits 0,1,2 set)
```

**Example 2**: Turn off all keys
```
Bitmask: 0x7FFF (all 15 bits set)
CAN ID: 0x211
DLC: 6
Data: [FF 7F 00 00 00 00]
      └──┴─┘ └┘ └──┴──┴─┘
        |   |    └─ RGB doesn't matter
        |   └─ Off
        └─ All keys
```

**Use Cases**:
- Set multiple keys to same color in one message
- More efficient than multiple LED_SET messages
- Example: Turn off all nuke button LEDs at once
```

---

### Change 6: Developer Guide Examples

**Add to `/doc/developer/canbus-protocol.md`** (Keypad Module section):

```markdown
## Keypad Module

### Sending Key Events (Keypad Firmware)

```c
#include "can_protocol_keypad.h"

void send_key_event(uint8_t key_id, bool pressed) {
    can_msg_key_event_t event = {
        .key_id = key_id,
        .state = pressed ? 1 : 0,
        .timestamp = (uint16_t)(esp_timer_get_time() / 1000)
    };
    
    twai_message_t msg = {
        .identifier = CAN_ID_KEY_EVENT_BASE + key_id,
        .data_length_code = sizeof(event),
        .flags = TWAI_MSG_FLAG_NONE
    };
    memcpy(msg.data, &event, sizeof(event));
    
    esp_err_t ret = twai_transmit(&msg, pdMS_TO_TICKS(10));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send key event: %s", esp_err_to_name(ret));
    }
}
```

### Receiving Key Events (Main Controller)

```c
void handle_key_event(const twai_message_t *msg) {
    // Check if CAN ID is in keypad range
    if (msg->identifier < CAN_ID_KEY_EVENT_BASE || 
        msg->identifier > (CAN_ID_KEY_EVENT_BASE + 15)) {
        return;  // Not a key event
    }
    
    can_msg_key_event_t event;
    memcpy(&event, msg->data, sizeof(event));
    
    uint8_t key_id = event.key_id;
    bool pressed = (event.state == 1);
    
    ESP_LOGI(TAG, "Key K%d %s (timestamp: %u ms)", 
             key_id, pressed ? "PRESSED" : "RELEASED", event.timestamp);
    
    // Map to game action
    keypad_action_t action = keypad_mapper_get_action(key_id);
    if (pressed && action != ACTION_NONE) {
        execute_action(action);
    }
}
```

### Sending LED Commands (Main Controller)

```c
// Set single key LED
void set_key_led(uint8_t key_id, bool on, uint8_t r, uint8_t g, uint8_t b) {
    can_msg_led_set_t cmd = {
        .key_id = key_id,
        .state = on ? 1 : 0,
        .r = r, .g = g, .b = b
    };
    
    twai_message_t msg = {
        .identifier = CAN_ID_LED_SET,
        .data_length_code = sizeof(cmd),
        .flags = TWAI_MSG_FLAG_NONE
    };
    memcpy(msg.data, &cmd, sizeof(cmd));
    
    twai_transmit(&msg, pdMS_TO_TICKS(10));
}

// Set multiple keys with bitmask
void set_keys_led_bulk(uint16_t key_mask, bool on, uint8_t r, uint8_t g, uint8_t b) {
    can_msg_led_bulk_t cmd = {
        .key_bitmask = key_mask,
        .state = on ? 1 : 0,
        .r = r, .g = g, .b = b
    };
    
    twai_message_t msg = {
        .identifier = CAN_ID_LED_BULK,
        .data_length_code = sizeof(cmd),
        .flags = TWAI_MSG_FLAG_NONE
    };
    memcpy(msg.data, &cmd, sizeof(cmd));
    
    twai_transmit(&msg, pdMS_TO_TICKS(10));
}

// Example: Set all nuke keys (K1-K3) to green
void update_nuke_leds_available(void) {
    uint16_t nuke_keys = 0x0007;  // Keys 1,2,3
    set_keys_led_bulk(nuke_keys, true, 0, 255, 0);  // Green = available
}
```

### Receiving LED Commands (Keypad Firmware)

```c
void handle_led_set(const twai_message_t *msg) {
    if (msg->identifier == CAN_ID_LED_SET) {
        // Single key LED set
        can_msg_led_set_t cmd;
        memcpy(&cmd, msg->data, sizeof(cmd));
        
        if (cmd.key_id == 0xFF) {
            // Set all keys
            led_rgb_t color = {cmd.r, cmd.g, cmd.b};
            led_controller_set_all(cmd.state, color);
        } else {
            // Set single key
            led_rgb_t color = {cmd.r, cmd.g, cmd.b};
            led_controller_set_key(cmd.key_id, cmd.state, color);
        }
        led_controller_update();
        
    } else if (msg->identifier == CAN_ID_LED_BULK) {
        // Bulk LED set
        can_msg_led_bulk_t cmd;
        memcpy(&cmd, msg->data, sizeof(cmd));
        
        led_rgb_t color = {cmd.r, cmd.g, cmd.b};
        
        // Apply to each key in bitmask
        for (uint8_t i = 0; i < 15; i++) {
            if (cmd.key_bitmask & (1 << i)) {
                led_controller_set_key(i + 1, cmd.state, color);
            }
        }
        led_controller_update();
    }
}
```

### Debugging Tips

**Key events not received:**
- Check physical key matrix (ground col pin manually)
- Verify debounce timing (should be ~20ms)
- Monitor CAN bus with logic analyzer or cantest tool
- Check CAN ID range (0x201-0x20F)

**LED updates laggy:**
- Measure LED update latency (<10ms target)
- Check if `led_controller_update()` is called after setting state
- Verify RMT peripheral priority
- Check CAN bus load (too many messages?)

**Wrong LED colors:**
- SK6812 uses GRB order (not RGB)
- Verify color byte order in message vs LED controller
- Check global brightness setting
- Test with known colors (pure red, green, blue)

**Bitmask confusion:**
- Bit 0 = Key 1, Bit 14 = Key 15
- Byte order: LSB first (Keys 1-8), MSB second (Keys 9-15)
- Example: Key 5 = bit 4 = 0x0010 = [10 00]
- Example: Keys 1+15 = bits 0+14 = 0x4001 = [01 40]
```

---

## Implementation Order

### Step 1: Update CAN Specification
1. ✅ Edit `/prompts/CANBUS_MESSAGE_SPEC.md`
   - Add module type 0x05
   - Add CAN ID range 0x300-0x31F
   - Add KEY_EVENT message format
   - Add LED_SET message format
   - Add LED_SET_BULK message format

### Step 2: Update Developer Guide
2. ✅ Edit `/doc/developer/canbus-protocol.md`
   - Add keypad section with C examples
   - Add debugging tips
   - Add timing requirements

### Step 3: Create Shared Component
3. ✅ Create `ots-fw-shared/components/can_protocol_keypad/`
   - Define structs matching spec
   - Define CAN IDs as constants
   - Add helper functions for encoding/decoding

### Step 4: Integration
4. ✅ Use in keypad firmware (`ots-fw-keypad`)
5. ✅ Use in main controller (`ots-fw-main`)
6. ✅ Test end-to-end communication

---

## Verification Checklist

- [ ] Module type 0x05 added to CANBUS_MESSAGE_SPEC.md
- [ ] CAN ID range 0x300-0x31F documented
- [ ] KEY_EVENT format matches implementation (4 bytes, timestamp)
- [ ] LED_SET format matches implementation (5 bytes, RGB)
- [ ] LED_SET_BULK format matches implementation (6 bytes, bitmask)
- [ ] C code examples in developer guide
- [ ] Bitmask layout documented clearly (Bit 0=K1, Bit 14=K15)
- [ ] Both files show same CAN IDs and byte layouts
- [ ] Debugging tips added for common issues

---

## Related Files

- **CAN Spec**: `/prompts/CANBUS_MESSAGE_SPEC.md` (single source of truth)
- **Developer Guide**: `/doc/developer/canbus-protocol.md` (implementation patterns)
- **Shared Component**: `ots-fw-shared/components/can_protocol_keypad/COMPONENT_PROMPT.md`
- **Keypad Architecture**: `ots-fw-keypad/ARCHITECTURE.md`
- **Main Controller Plan**: `ots-fw-keypad/MAIN_CONTROLLER_UPDATE_PLAN.md`
