# can_protocol_keypad - Component Prompt

## Overview

The `can_protocol_keypad` component provides keypad module-specific CAN message definitions and parsing/building helpers for OTS 15-key keyboard module communication.

## Purpose

This shared component enables both the **main controller** (`ots-fw-main`) and **keypad module** (`ots-fw-keypad`) to:
- Send key press/release events (keypad → main)
- Receive LED control commands (main → keypad)
- Use consistent message formats and constants
- Handle per-key RGB LED state

## Location

- **Component**: `/ots-fw-shared/components/can_protocol_keypad/`
- **Protocol Spec**: `/prompts/CANBUS_MESSAGE_SPEC.md`
- **Developer Guide**: `/doc/developer/canbus-protocol.md`

## Files

- `can_protocol_keypad.h` - Keypad message IDs, constants, structures, function declarations
- `can_protocol_keypad.c` - Message parsing and building implementation
- `CMakeLists.txt` - ESP-IDF component registration
- `COMPONENT_PROMPT.md` - This file

## Dependencies

Requires the `can_driver` component for:
- `can_frame_t` structure
- Basic CAN TX/RX functions

## Message IDs

Keypad protocol uses the **0x430-0x43F** CAN ID block:

| ID | Name | Direction | Description |
|----|------|-----------|-------------|
| 0x431-0x43F | KEY_EVENT | keypad → main | Key press/release (dynamic: 0x430 + key_id) |
| 0x430 | LED_SET | main → keypad | Set single key LED color and state |
| 0x440 | LED_BULK | main → keypad | Set multiple key LEDs with bitmask |

**Note**: KEY_EVENT uses dynamic CAN IDs. Each key (1-15) sends on its own ID:
- Key 1: 0x431
- Key 2: 0x432
- ...
- Key 15: 0x43F

## Key Constants

### Key States
- `CAN_KEYPAD_STATE_RELEASED` (0x00) - Key released
- `CAN_KEYPAD_STATE_PRESSED` (0x01) - Key pressed

### Key ID Range
- `CAN_KEYPAD_KEY_MIN` (1) - First key ID
- `CAN_KEYPAD_KEY_MAX` (15) - Last key ID
- `CAN_KEYPAD_NUM_KEYS` (15) - Total keys
- `CAN_KEYPAD_ALL_KEYS` (0xFF) - Special ID for "all keys" in LED commands

### LED States
- `CAN_KEYPAD_LED_OFF` (0x00) - LED off
- `CAN_KEYPAD_LED_ON` (0x01) - LED on

### LED Color Presets
Convenient RGB triplets for common colors:
- `CAN_KEYPAD_COLOR_OFF` - 0, 0, 0
- `CAN_KEYPAD_COLOR_RED` - 255, 0, 0
- `CAN_KEYPAD_COLOR_GREEN` - 0, 255, 0
- `CAN_KEYPAD_COLOR_BLUE` - 0, 0, 255
- `CAN_KEYPAD_COLOR_YELLOW` - 255, 255, 0
- `CAN_KEYPAD_COLOR_CYAN` - 0, 255, 255
- `CAN_KEYPAD_COLOR_MAGENTA` - 255, 0, 255
- `CAN_KEYPAD_COLOR_WHITE` - 255, 255, 255

## Message Structures

### KEY_EVENT (0x431-0x43F)

```c
typedef struct {
    uint8_t key_id;      // Key ID (1-15)
    uint8_t state;       // 0=released, 1=pressed
    uint16_t timestamp;  // Timestamp in milliseconds (little-endian)
} __attribute__((packed)) can_keypad_event_t;
```

**DLC**: 4 bytes  
**Direction**: Keypad → Main Controller  
**CAN ID**: Dynamic (0x430 + key_id)

### LED_SET (0x430)

```c
typedef struct {
    uint8_t key_id;      // Key ID (1-15) or 0xFF for all keys
    uint8_t state;       // 0=off, 1=on
    uint8_t r;           // Red component (0-255)
    uint8_t g;           // Green component (0-255)
    uint8_t b;           // Blue component (0-255)
} __attribute__((packed)) can_keypad_led_set_t;
```

**DLC**: 5 bytes  
**Direction**: Main Controller → Keypad  
**CAN ID**: 0x430

### LED_BULK (0x440)

```c
typedef struct {
    uint16_t mask;       // Key bitmask (bit 0=K1, bit 14=K15, little-endian)
    uint8_t r;           // Red component (0-255)
    uint8_t g;           // Green component (0-255)
    uint8_t b;           // Blue component (0-255)
    uint8_t state;       // 0=off, 1=on
    uint8_t reserved[2]; // Reserved for future use
} __attribute__((packed)) can_keypad_led_bulk_t;
```

**DLC**: 8 bytes  
**Direction**: Main Controller → Keypad  
**CAN ID**: 0x440

## API Functions

### Building Functions (Keypad Module Sends)

```c
void can_keypad_build_key_event(uint8_t key_id, 
                                 uint8_t state, 
                                 uint16_t timestamp, 
                                 can_frame_t *frame);
```
Build KEY_EVENT message for key press/release. Sets CAN ID dynamically to `0x430 + key_id`.

**Example (keypad firmware):**
```c
can_frame_t frame;
uint16_t timestamp = (uint16_t)(esp_timer_get_time() / 1000);
can_keypad_build_key_event(5, CAN_KEYPAD_STATE_PRESSED, timestamp, &frame);
// frame.id = 0x435, frame.dlc = 4
can_driver_send(&frame, 100);
```

### Parsing Functions (Keypad Module Receives)

```c
bool can_keypad_parse_led_set(const can_frame_t *frame, 
                               uint8_t *key_id,
                               uint8_t *state, 
                               uint8_t *r, 
                               uint8_t *g, 
                               uint8_t *b);
```
Parse LED_SET message from main controller. Returns `true` if valid, `false` otherwise.

**Example (keypad firmware):**
```c
if (frame.id == CAN_ID_KEYPAD_LED_SET) {
    uint8_t key_id, state, r, g, b;
    if (can_keypad_parse_led_set(&frame, &key_id, &state, &r, &g, &b)) {
        if (key_id == CAN_KEYPAD_ALL_KEYS) {
            // Set all LEDs
            for (int i = 1; i <= 15; i++) {
                set_led(i, state, r, g, b);
            }
        } else {
            set_led(key_id, state, r, g, b);
        }
    }
}
```

```c
bool can_keypad_parse_led_bulk(const can_frame_t *frame, 
                                uint16_t *mask,
                                uint8_t *r, 
                                uint8_t *g, 
                                uint8_t *b, 
                                uint8_t *state);
```
Parse LED_BULK message from main controller. Returns `true` if valid, `false` otherwise.

**Example (keypad firmware):**
```c
if (frame.id == CAN_ID_KEYPAD_LED_BULK) {
    uint16_t mask;
    uint8_t r, g, b, state;
    if (can_keypad_parse_led_bulk(&frame, &mask, &r, &g, &b, &state)) {
        // Apply to all keys in bitmask
        for (int i = 0; i < 15; i++) {
            if (mask & (1 << i)) {
                set_led(i + 1, state, r, g, b);
            }
        }
    }
}
```

### Building Functions (Main Controller Sends)

```c
void can_keypad_build_led_set(uint8_t key_id, 
                               uint8_t state,
                               uint8_t r, 
                               uint8_t g, 
                               uint8_t b, 
                               can_frame_t *frame);
```
Build LED_SET message to control single key LED.

**Example (main controller):**
```c
can_frame_t frame;

// Set key 1 to green, ON
can_keypad_build_led_set(1, CAN_KEYPAD_LED_ON, CAN_KEYPAD_COLOR_GREEN, &frame);
can_driver_send(&frame, 100);

// Turn off all LEDs
can_keypad_build_led_set(CAN_KEYPAD_ALL_KEYS, CAN_KEYPAD_LED_OFF, 0, 0, 0, &frame);
can_driver_send(&frame, 100);
```

```c
void can_keypad_build_led_bulk(uint16_t mask, 
                                uint8_t state,
                                uint8_t r, 
                                uint8_t g, 
                                uint8_t b, 
                                can_frame_t *frame);
```
Build LED_BULK message to control multiple key LEDs at once using bitmask.

**Example (main controller):**
```c
can_frame_t frame;

// Set keys 1, 2, 3 to red, ON
uint16_t mask = 0x0007;  // Bits 0, 1, 2 set
can_keypad_build_led_bulk(mask, CAN_KEYPAD_LED_ON, CAN_KEYPAD_COLOR_RED, &frame);
can_driver_send(&frame, 100);
```

### Utility Functions

```c
bool can_keypad_is_key_event(uint32_t can_id);
```
Check if CAN ID is a key event (0x431-0x43F).

```c
uint8_t can_keypad_get_key_id_from_can_id(uint32_t can_id);
```
Extract key ID (1-15) from key event CAN ID. Returns 0 if invalid.

**Example (main controller):**
```c
if (can_keypad_is_key_event(frame.id)) {
    uint8_t key_id = can_keypad_get_key_id_from_can_id(frame.id);
    uint8_t state = frame.data[1];
    // Handle key event...
}
```

```c
bool can_keypad_is_valid_key_id(uint8_t key_id);
```
Validate key ID is in range (1-15).

```c
uint16_t can_keypad_key_to_mask(uint8_t key_id);
```
Convert single key ID to bitmask. Key 1 → 0x0001, Key 15 → 0x4000.

```c
uint16_t can_keypad_keys_to_mask(const uint8_t *key_ids, uint8_t count);
```
Convert array of key IDs to combined bitmask.

**Example:**
```c
uint8_t keys[] = {1, 5, 7};  // Keys to highlight
uint16_t mask = can_keypad_keys_to_mask(keys, 3);  // mask = 0x0051
can_keypad_build_led_bulk(mask, CAN_KEYPAD_LED_ON, CAN_KEYPAD_COLOR_GREEN, &frame);
```

```c
uint16_t can_keypad_all_keys_mask(void);
```
Get bitmask for all 15 keys (0x7FFF).

## Usage Examples

### Keypad Firmware: Send Key Press

```c
#include "can_protocol_keypad.h"
#include "can_driver.h"

void on_key_pressed(uint8_t key_id) {
    can_frame_t frame;
    uint16_t timestamp = (uint16_t)(esp_timer_get_time() / 1000);
    
    can_keypad_build_key_event(key_id, CAN_KEYPAD_STATE_PRESSED, timestamp, &frame);
    can_driver_send(&frame, 100);
    
    ESP_LOGI(TAG, "KEY_PRESSED: K%d (CAN ID 0x%03X)", key_id, frame.id);
}
```

### Keypad Firmware: Receive LED Commands

```c
void keypad_can_rx_task(void *arg) {
    can_frame_t frame;
    
    while (1) {
        if (can_driver_receive(&frame, portMAX_DELAY) == ESP_OK) {
            
            // LED_SET: Single key
            if (frame.id == CAN_ID_KEYPAD_LED_SET) {
                uint8_t key_id, state, r, g, b;
                if (can_keypad_parse_led_set(&frame, &key_id, &state, &r, &g, &b)) {
                    if (key_id == CAN_KEYPAD_ALL_KEYS) {
                        for (int i = 1; i <= 15; i++) {
                            keypad_led_set(i, state, r, g, b);
                        }
                    } else if (can_keypad_is_valid_key_id(key_id)) {
                        keypad_led_set(key_id, state, r, g, b);
                    }
                }
            }
            
            // LED_BULK: Multiple keys
            else if (frame.id == CAN_ID_KEYPAD_LED_BULK) {
                uint16_t mask;
                uint8_t r, g, b, state;
                if (can_keypad_parse_led_bulk(&frame, &mask, &r, &g, &b, &state)) {
                    for (int i = 0; i < 15; i++) {
                        if (mask & (1 << i)) {
                            keypad_led_set(i + 1, state, r, g, b);
                        }
                    }
                }
            }
        }
    }
}
```

### Main Controller: Receive Key Events

```c
void main_controller_can_rx_task(void *arg) {
    can_frame_t frame;
    
    while (1) {
        if (can_driver_receive(&frame, portMAX_DELAY) == ESP_OK) {
            
            // Check if key event
            if (can_keypad_is_key_event(frame.id)) {
                uint8_t key_id = can_keypad_get_key_id_from_can_id(frame.id);
                uint8_t state = frame.data[1];
                uint16_t timestamp = frame.data[2] | (frame.data[3] << 8);
                
                ESP_LOGI(TAG, "KEY_%s: K%d @%ums", 
                         state ? "PRESSED" : "RELEASED", key_id, timestamp);
                
                // Forward to WebSocket
                if (state == CAN_KEYPAD_STATE_PRESSED) {
                    broadcast_keypad_event("KEYPAD_KEY_PRESSED", key_id);
                } else {
                    broadcast_keypad_event("KEYPAD_KEY_RELEASED", key_id);
                }
            }
        }
    }
}
```

### Main Controller: Send LED Commands

```c
// Example: Indicate available actions (green LEDs)
void update_available_keys(bool can_build[] /* 15 keys */) {
    can_frame_t frame;
    
    for (int i = 1; i <= 15; i++) {
        if (can_build[i-1]) {
            can_keypad_build_led_set(i, CAN_KEYPAD_LED_ON, CAN_KEYPAD_COLOR_GREEN, &frame);
        } else {
            can_keypad_build_led_set(i, CAN_KEYPAD_LED_OFF, CAN_KEYPAD_COLOR_OFF, &frame);
        }
        can_driver_send(&frame, 100);
        vTaskDelay(pdMS_TO_TICKS(5));  // Small delay between commands
    }
}

// Example: Flash key on press (visual feedback)
void flash_key_white(uint8_t key_id) {
    can_frame_t frame;
    
    // White flash
    can_keypad_build_led_set(key_id, CAN_KEYPAD_LED_ON, CAN_KEYPAD_COLOR_WHITE, &frame);
    can_driver_send(&frame, 100);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Off
    can_keypad_build_led_set(key_id, CAN_KEYPAD_LED_OFF, CAN_KEYPAD_COLOR_OFF, &frame);
    can_driver_send(&frame, 100);
}

// Example: Set multiple keys at once (bulk)
void highlight_build_keys(void) {
    can_frame_t frame;
    
    // Keys 1-7 are building actions
    uint16_t mask = 0x007F;  // Bits 0-6 (keys 1-7)
    can_keypad_build_led_bulk(mask, CAN_KEYPAD_LED_ON, CAN_KEYPAD_COLOR_GREEN, &frame);
    can_driver_send(&frame, 100);
}
```

## Integration with CMakeLists.txt

In your firmware project's component CMakeLists.txt:

```cmake
idf_component_register(
    SRCS "main.c" "keypad_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        driver
        can_driver
        can_protocol_discovery
        can_protocol_keypad  # Add this
)
```

And in your project's main CMakeLists.txt:

```cmake
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_SOURCE_DIR}/../ots-fw-shared/components"
)
```

## Testing

### Test Key Event Transmission

```c
// Keypad firmware: Test all keys
for (uint8_t key_id = 1; key_id <= 15; key_id++) {
    can_frame_t frame;
    can_keypad_build_key_event(key_id, CAN_KEYPAD_STATE_PRESSED, key_id * 100, &frame);
    can_driver_send(&frame, 100);
    vTaskDelay(pdMS_TO_TICKS(200));
}
```

### Test LED Control

```c
// Main controller: Test LED commands
can_frame_t frame;

// Test individual keys (rainbow)
const uint8_t colors[][3] = {
    {255, 0, 0},    // Red
    {255, 127, 0},  // Orange
    {255, 255, 0},  // Yellow
    {0, 255, 0},    // Green
    {0, 0, 255},    // Blue
};

for (int i = 0; i < 5; i++) {
    can_keypad_build_led_set(i+1, CAN_KEYPAD_LED_ON, colors[i][0], colors[i][1], colors[i][2], &frame);
    can_driver_send(&frame, 100);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

## Hardware Notes

- **Matrix**: 3 rows × 7 columns (3×7 = 21 positions, 15 used)
- **Key Layout**: 7 + 7 + 1 (rows 0, 1, 2)
- **Key 15**: Spacebar position (Row 2, Column 3)
- **RGB LEDs**: SK6812-MINI-E (WS2812-compatible protocol)
- **Module Type**: MODULE_TYPE_KEYPAD (0x02)
- **Discovery**: Uses standard `can_protocol_discovery` component

## Related Documentation

- **Protocol Spec**: `/prompts/CANBUS_MESSAGE_SPEC.md` - Complete message format reference
- **Developer Guide**: `/doc/developer/canbus-protocol.md` - Implementation patterns and examples
- **Hardware Spec**: `/ots-hardware/modules/keypad-module.md` - Physical design and GPIO mapping
- **Keypad Firmware**: `/ots-fw-keypad/` - Complete firmware implementation
- **Planning Docs**: `/ots-fw-keypad/plans/03-can-protocol-specification.md` - Integration plan

---

**Questions or issues?** Check the protocol spec or developer guide, or consult the keypad firmware implementation for working examples.
