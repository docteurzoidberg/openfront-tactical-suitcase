# CAN Discovery Component

Boot-time module detection protocol for OTS CAN bus.

## Overview

Simple discovery protocol for detecting modules on the CAN bus at startup:
1. Main controller sends `MODULE_QUERY` broadcast on boot
2. Each module responds with `MODULE_ANNOUNCE` containing type, version, and CAN ID block
3. Main controller waits 500ms for all responses
4. Done - no heartbeat or runtime tracking

## Usage

### Audio Module (or any other module)

```c
#include "can_discovery.h"

void app_main(void) {
    // Initialize CAN...
    can_driver_init();
    
    // Start CAN RX task to listen for queries
    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
}

void can_rx_task(void *pvParameters) {
    can_message_t msg;
    
    while (1) {
        if (can_driver_receive(&msg, portMAX_DELAY) == ESP_OK) {
            // Handle discovery query
            if (msg.identifier == CAN_ID_MODULE_QUERY) {
                can_discovery_handle_query(&msg,
                    MODULE_TYPE_AUDIO,      // Module type
                    1,                      // Version 1.0
                    0,
                    MODULE_CAP_STATUS,      // Has status messages
                    0x42,                   // CAN block: 0x420-0x42F
                    0                       // Node ID (single module)
                );
            }
            
            // Handle other messages...
        }
    }
}
```

### Main Controller

```c
#include "can_discovery.h"

typedef struct {
    module_info_t audio;
    module_info_t light;
    // ... other modules
} system_modules_t;

system_modules_t g_modules = {0};

void discover_modules(void) {
    ESP_LOGI(TAG, "Discovering CAN modules...");
    
    // Send query broadcast
    can_discovery_query_all();
    
    // Wait for responses
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Check what we found
    if (g_modules.audio.discovered) {
        ESP_LOGI(TAG, "✓ Audio module v%d.%d detected on CAN block 0x%02X",
                 g_modules.audio.version_major,
                 g_modules.audio.version_minor,
                 g_modules.audio.can_block_base);
    } else {
        ESP_LOGW(TAG, "✗ No audio module detected");
    }
}

void can_rx_task(void *pvParameters) {
    can_message_t msg;
    
    while (1) {
        if (can_driver_receive(&msg, portMAX_DELAY) == ESP_OK) {
            // Handle module announcements
            if (msg.identifier == CAN_ID_MODULE_ANNOUNCE) {
                module_info_t info;
                if (can_discovery_parse_announce(&msg, &info) == ESP_OK) {
                    // Store module info
                    switch (info.module_type) {
                        case MODULE_TYPE_AUDIO:
                            g_modules.audio = info;
                            break;
                        case MODULE_TYPE_LIGHT:
                            g_modules.light = info;
                            break;
                    }
                }
            }
            
            // Handle other messages...
        }
    }
}
```

## Message Format

### MODULE_QUERY (0x411) - Main → All modules

```
Broadcast query to discover modules
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ 0xFF│ 0x00│ 0x00│ 0x00│ 0x00│ 0x00│ 0x00│ 0x00│
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
  │
  └─ Magic byte: 0xFF = enumerate all modules
```

### MODULE_ANNOUNCE (0x410) - Module → Main

```
Module identification response
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│Type │Ver  │Ver  │Caps │CAN  │Node │ Reserved  │
│     │Major│Minor│     │Block│ ID  │           │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘

B0: Module Type
    - 0x01 = Audio Module
    - 0x02 = Light Module
    - 0x03 = Motor Module
    - 0x04 = Display Module
    
B1-B2: Firmware Version (major.minor)
    - Example: v1.2 = [0x01, 0x02]
    
B3: Capabilities (bitfield)
    - bit 0: MODULE_CAP_STATUS (has status messages)
    - bit 1: MODULE_CAP_OTA (supports OTA updates)
    - bit 2: MODULE_CAP_BATTERY (battery powered)
    
B4: CAN ID Block Base
    - Audio: 0x42 (uses 0x420-0x42F)
    - Light: 0x43 (uses 0x430-0x43F)
    
B5: Node ID
    - 0x00 = primary/single module
    - 0x01-0xFF = multiple modules of same type
    
B6-B7: Reserved (must be 0)
```

## Module Types

```c
#define MODULE_TYPE_NONE        0x00
#define MODULE_TYPE_AUDIO       0x01  // Audio playback module
#define MODULE_TYPE_LIGHT       0x02  // LED/lighting control
#define MODULE_TYPE_MOTOR       0x03  // Motor/actuator control
#define MODULE_TYPE_DISPLAY     0x04  // Display module
```

## Capability Flags

```c
#define MODULE_CAP_STATUS       (1 << 0)  // Sends periodic status
#define MODULE_CAP_OTA          (1 << 1)  // Supports firmware updates
#define MODULE_CAP_BATTERY      (1 << 2)  // Battery powered
```

## CAN ID Block Allocation

Each module reserves a 16-ID block:

| Module | CAN Block | IDs Used | Description |
|--------|-----------|----------|-------------|
| Discovery | 0x400-0x40F | 0x410, 0x411 | System messages |
| Audio | 0x420-0x42F | 0x420-0x425 | Sound control |
| Light | 0x430-0x43F | (future) | LED control |
| Motor | 0x440-0x44F | (future) | Actuator control |
| Display | 0x450-0x45F | (future) | Display control |

## API Reference

### Module Side

**`can_discovery_announce()`** - Send module announcement
- Call on boot or in response to query
- Fills in module type, version, CAN block

**`can_discovery_handle_query()`** - Auto-respond to queries
- Call in CAN RX handler when MODULE_QUERY received
- Automatically sends ANNOUNCE if query matches

### Main Controller Side

**`can_discovery_query_all()`** - Broadcast module query
- Call once on boot
- Wait 500ms for responses

**`can_discovery_parse_announce()`** - Parse module announcement
- Call in CAN RX handler when MODULE_ANNOUNCE received
- Fills `module_info_t` structure with module details

**`can_discovery_get_module_name()`** - Get module type name
- Returns human-readable string for logging
- Example: "Audio Module", "Light Module"

## Integration Notes

- **Boot sequence**: Main controller must wait 500ms after query for all modules to respond
- **No heartbeat**: Modules don't send periodic beacons (keeps bus traffic low)
- **Multiple modules**: Use `node_id` field to distinguish multiple modules of same type
- **CAN block allocation**: Reserve 16 IDs per module type for future expansion

## See Also

- `/ots-fw-shared/components/can_driver/` - Generic CAN driver
- `/ots-fw-shared/components/can_audiomodule/` - Audio protocol
- `/prompts/CANBUS_MESSAGE_SPEC.md` - CAN bus protocol specification
- `/doc/developer/canbus-protocol.md` - Implementation guide with examples
