# CAN Bus Protocol Architecture for Multi-Module OTS System

## Overview

This document defines a **generic, extensible CAN bus protocol** for the OpenFront Tactical Suitcase that supports multiple module types with automatic discovery, addressing, and message routing.

## Design Goals

1. **Module Agnostic**: Support unknown future module types
2. **Hot-Plug**: Detect modules joining/leaving the bus
3. **Scalable**: Support 10+ modules on a single bus
4. **Prioritized**: Critical messages take precedence
5. **Efficient**: Minimize bus overhead while maintaining responsiveness
6. **Reliable**: Handle errors, timeouts, and bus-off conditions

## CAN ID Allocation Strategy

### 11-bit Standard IDs (0x000-0x7FF)

**Priority-based addressing scheme:**

```
11-bit ID: [PPP][TTTT][AAAA]
           Priority(3) Type(4) Address(4)

PPP = Priority (0=highest, 7=lowest)
TTTT = Message Type (0-15)
AAAA = Module Address (0-15)
```

### Priority Levels (Bits 10-8)

| Priority | Value | Usage |
|----------|-------|-------|
| Emergency | 0 | Emergency stop, critical failures |
| High | 1 | Real-time game events (nuke launch, alerts) |
| Normal | 2-4 | Standard commands, status updates |
| Low | 5-7 | Bulk data, diagnostics, discovery |

### Message Types (Bits 7-4)

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| BROADCAST | 0x0 | Controller → All | Broadcast to all modules |
| DISCOVERY | 0x1 | Bidirectional | Module enumeration |
| COMMAND | 0x2 | Controller → Module | Unicast command |
| RESPONSE | 0x3 | Module → Controller | Command response |
| STATUS | 0x4 | Module → Controller | Periodic status |
| EVENT | 0x5 | Module → Controller | Asynchronous events |
| CONFIG | 0x6 | Bidirectional | Configuration read/write |
| DIAG | 0x7 | Bidirectional | Diagnostics |
| DATA | 0x8 | Bidirectional | Bulk data transfer |
| Reserved | 0x9-0xF | - | Future expansion |

### Module Addresses (Bits 3-0)

| Address | Module Type | Notes |
|---------|-------------|-------|
| 0x0 | Main Controller | Always present |
| 0x1-0xE | Modules | Auto-assigned or DIP switch |
| 0xF | Broadcast | All modules listen |

### Example CAN IDs

```c
// Emergency stop (priority 0, broadcast)
#define CAN_ID_EMERGENCY_STOP    0x00F  // 000_0000_1111

// Discovery request (priority 6, discovery, broadcast)
#define CAN_ID_DISCOVERY_REQ     0x61F  // 110_0001_1111

// Command to module 3 (priority 2, command, addr 3)
#define CAN_ID_CMD_MODULE_3      0x223  // 010_0010_0011

// Status from module 5 (priority 3, status, addr 5)
#define CAN_ID_STATUS_MODULE_5   0x345  // 011_0100_0101

// High-priority event from module 2 (priority 1, event, addr 2)
#define CAN_ID_EVENT_MODULE_2    0x152  // 001_0101_0010
```

## Generic Message Format

### Standard 8-Byte Payload Structure

```
Byte 0: Message Class (CMD/RSP/STATUS/etc.)
Byte 1: Subcommand / Status Code
Byte 2-7: Payload (format depends on message class)
```

### Message Classes

#### 0x00-0x0F: System Messages

```c
#define MSG_SYSTEM_PING         0x00  // Heartbeat/keepalive
#define MSG_SYSTEM_RESET        0x01  // Software reset
#define MSG_SYSTEM_BOOTLOADER   0x02  // Enter bootloader
#define MSG_SYSTEM_ERROR        0x03  // Error report
```

#### 0x10-0x1F: Discovery & Configuration

```c
#define MSG_DISC_WHO_IS_THERE   0x10  // Controller asks for modules
#define MSG_DISC_I_AM_HERE      0x11  // Module announces presence
#define MSG_DISC_GET_INFO       0x12  // Get module info
#define MSG_DISC_INFO_RESPONSE  0x13  // Module info payload

#define MSG_CFG_READ            0x14  // Read config parameter
#define MSG_CFG_WRITE           0x15  // Write config parameter
#define MSG_CFG_RESPONSE        0x16  // Config operation result
```

#### 0x20-0x2F: Generic Commands

```c
#define MSG_CMD_SET_STATE       0x20  // Set module state
#define MSG_CMD_GET_STATE       0x21  // Query module state
#define MSG_CMD_TRIGGER         0x22  // Trigger action
#define MSG_CMD_STOP            0x23  // Stop current action
```

#### 0x30-0x3F: Responses

```c
#define MSG_RSP_ACK             0x30  // Command acknowledged
#define MSG_RSP_NACK            0x31  // Command rejected
#define MSG_RSP_DATA            0x32  // Response with data
#define MSG_RSP_ERROR           0x33  // Error response
```

#### 0x40-0x4F: Status & Events

```c
#define MSG_STATUS_PERIODIC     0x40  // Periodic status update
#define MSG_STATUS_CHANGED      0x41  // State changed event
#define MSG_EVENT_BUTTON        0x42  // Button pressed
#define MSG_EVENT_SENSOR        0x43  // Sensor triggered
#define MSG_EVENT_ALERT         0x44  // Alert condition
```

#### 0x50-0xFF: Module-Specific

Reserved for module type-specific messages.

## Module Discovery Protocol

> **✅ IMPLEMENTED**: Boot-time discovery protocol is fully implemented in the shared component.  
> See: `/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`

### Startup Sequence

The OTS system uses a **boot-time only** discovery protocol to detect CAN modules:

```
1. Controller boots, initializes CAN bus
2. Controller broadcasts MODULE_QUERY (0x411) to all modules
3. Modules respond with MODULE_ANNOUNCE (0x410) containing their info
4. Controller waits 500ms for all responses
5. Controller builds module registry (no further discovery traffic)
```

**No ongoing heartbeat**: Discovery happens once at boot. This minimizes CAN bus traffic during gameplay.

### CAN IDs (Reserved for Discovery)

| CAN ID | Direction | Message Type | Description |
|--------|-----------|--------------|-------------|
| **0x410** | Module → Main | MODULE_ANNOUNCE | Module announces presence and info |
| **0x411** | Main → Module | MODULE_QUERY | Main controller queries for modules |

### Message Format

#### MODULE_QUERY (0x411)

**Direction**: Main controller → All modules (broadcast)  
**Purpose**: Request all modules to identify themselves

```
Byte 0: 0xFF (broadcast marker)
Bytes 1-7: Reserved (0x00)
```

**Example**:
```
CAN ID: 0x411
Data: [FF 00 00 00 00 00 00 00]
```

#### MODULE_ANNOUNCE (0x410)

**Direction**: Module → Main controller  
**Purpose**: Module identifies itself with type, version, capabilities

```
Byte 0: Module Type (0x00-0xFF)
Byte 1: Firmware Major Version
Byte 2: Firmware Minor Version
Byte 3: Capabilities (bit flags)
Byte 4: CAN Block (module's assigned CAN ID block)
Byte 5: Node ID (reserved, 0x00)
Bytes 6-7: Reserved (0x00)
```

**Example - Audio Module v1.0**:
```
CAN ID: 0x410
Data: [01 01 00 01 42 00 00 00]
       │  │  │  │  │
       │  │  │  │  └─ CAN block 0x42 (uses 0x420-0x42F)
       │  │  │  └─ Capabilities: 0x01 (STATUS)
       │  │  └─ Firmware minor: 0
       │  └─ Firmware major: 1
       └─ Module type: 0x01 (AUDIO)
```

### Module Type Registry

```c
#define MODULE_TYPE_NONE        0x00  // Reserved / uninitialized
#define MODULE_TYPE_AUDIO       0x01  // ✅ IMPLEMENTED
#define MODULE_TYPE_DISPLAY     0x02  // Future
#define MODULE_TYPE_INPUT       0x03  // Future
#define MODULE_TYPE_LIGHTING    0x04  // Future
#define MODULE_TYPE_HAPTIC      0x05  // Future
#define MODULE_TYPE_SENSOR      0x06  // Future
#define MODULE_TYPE_POWER       0x07  // Future
// 0x08-0x7F: Future module types
// 0x80-0xFF: Custom/experimental
```

### Capability Flags

```c
#define MODULE_CAP_STATUS       0x01  // Sends periodic status updates
#define MODULE_CAP_OTA          0x02  // Supports OTA firmware updates
#define MODULE_CAP_BATTERY      0x04  // Battery powered (low-power mode)
// bits 3-7: Reserved for future capabilities
```

### Discovery Flow Diagram

```
Main Controller                          Audio Module
     │                                        │
     │ ──────── MODULE_QUERY (0x411) ───────→│
     │                                        │
     │                                   [Parse query]
     │                                   [Prepare announce]
     │                                        │
     │ ←──── MODULE_ANNOUNCE (0x410) ────────│
     │       (type=AUDIO, v1.0, block=0x42)  │
     │                                        │
[Wait 500ms for more responses]              │
     │                                        │
[Build module registry]                      │
[Enable sound_module features]               │
     │                                        │
     ▼                                        ▼
  Ready                                   Ready
```

### Integration Example

**Module Side (e.g., audio module)**:
```c
#include "can_discovery.h"

void can_rx_task(void *arg) {
    can_frame_t frame;
    while (1) {
        if (can_driver_receive(&frame, portMAX_DELAY) == ESP_OK) {
            if (frame.id == CAN_ID_MODULE_QUERY) {
                // Auto-respond to discovery
                can_discovery_handle_query(&frame, MODULE_TYPE_AUDIO,
                                          1, 0, MODULE_CAP_STATUS, 0x42, 0);
            }
            // ... handle other messages ...
        }
    }
}
```

**Main Controller Side**:
```c
#include "can_discovery.h"

void discover_modules(void) {
    // Send discovery query
    can_discovery_query_all();
    
    // Wait for responses
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Process MODULE_ANNOUNCE messages in CAN RX task
    // (handled separately in can_rx_task)
}
```

For complete API reference and integration examples, see:  
**`/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`**

## Controller Implementation

### Module Registry

```c
typedef struct {
    uint8_t address;          // CAN bus address (1-14)
    uint8_t module_type;      // Module type ID
    uint8_t hw_version;       
    uint8_t fw_version_major;
    uint8_t fw_version_minor;
    uint8_t capabilities;
    bool online;              // Currently responding
    uint32_t last_seen_ms;    // Last heartbeat timestamp
    void *protocol_handler;   // Module-specific protocol handler
} can_module_entry_t;

#define CAN_MAX_MODULES 14

typedef struct {
    can_module_entry_t modules[CAN_MAX_MODULES];
    uint8_t module_count;
} can_module_registry_t;
```

### Message Routing

```c
typedef void (*can_message_handler_t)(const can_frame_t *frame, uint8_t module_addr);

// Register handler for specific message class
esp_err_t can_register_handler(uint8_t msg_class, can_message_handler_t handler);

// Route incoming message to appropriate handler
void can_route_message(const can_frame_t *frame);

// Broadcast message to all modules
esp_err_t can_broadcast(uint8_t msg_class, const uint8_t *data, uint8_t len);

// Send unicast message to specific module
esp_err_t can_send_to_module(uint8_t module_addr, uint8_t msg_class, 
                             const uint8_t *data, uint8_t len);
```

## Proposed Component Enhancements

### 1. Add can_protocol.h (Generic Protocol Layer)

Create `/components/can_driver/include/can_protocol.h`:

```c
#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include "can_driver.h"

// CAN ID construction macros
#define CAN_ID_MAKE(priority, msg_type, addr) \
    (((priority) << 8) | ((msg_type) << 4) | (addr))

#define CAN_ID_GET_PRIORITY(id) (((id) >> 8) & 0x07)
#define CAN_ID_GET_TYPE(id)     (((id) >> 4) & 0x0F)
#define CAN_ID_GET_ADDR(id)     ((id) & 0x0F)

// Priority levels
#define CAN_PRIORITY_EMERGENCY  0
#define CAN_PRIORITY_HIGH       1
#define CAN_PRIORITY_NORMAL     2
#define CAN_PRIORITY_LOW        6

// Message types
#define CAN_MSG_BROADCAST      0x0
#define CAN_MSG_DISCOVERY      0x1
#define CAN_MSG_COMMAND        0x2
#define CAN_MSG_RESPONSE       0x3
#define CAN_MSG_STATUS         0x4
#define CAN_MSG_EVENT          0x5
#define CAN_MSG_CONFIG         0x6

// Addresses
#define CAN_ADDR_CONTROLLER    0x0
#define CAN_ADDR_BROADCAST     0xF

// Message classes
#define MSG_SYSTEM_PING        0x00
#define MSG_DISC_WHO_IS_THERE  0x10
#define MSG_DISC_I_AM_HERE     0x11
#define MSG_CMD_SET_STATE      0x20
#define MSG_RSP_ACK            0x30
#define MSG_STATUS_PERIODIC    0x40

#endif // CAN_PROTOCOL_H
```

### 2. Add can_module_registry (Module Management)

Create `/components/can_driver/can_module_registry.c`:

```c
// Module discovery and registry management
esp_err_t can_discover_modules(can_module_registry_t *registry);
esp_err_t can_query_module_info(uint8_t addr, module_info_t *info);
bool can_module_is_online(uint8_t addr);
void can_module_heartbeat_task(void *pvParameters);
```

### 3. Add can_message_router (Message Routing)

Create `/components/can_driver/can_message_router.c`:

```c
// Dynamic message routing and handler registration
esp_err_t can_router_init(void);
esp_err_t can_register_handler(uint8_t msg_class, can_message_handler_t handler);
void can_router_task(void *pvParameters);
esp_err_t can_broadcast(uint8_t msg_class, const uint8_t *data, uint8_t len);
```

## Proposed fw-main Enhancements

### 1. Generic CAN Module Interface

Create `/ots-fw-main/include/can_module_interface.h`:

```c
typedef struct can_hardware_module {
    // Standard hardware_module_t interface
    hardware_module_t base;
    
    // CAN-specific fields
    uint8_t can_address;
    uint8_t module_type;
    bool discovered;
    
    // Module-specific protocol handler
    void (*handle_can_message)(const can_frame_t *frame);
} can_hardware_module_t;
```

### 2. Module Registry Service

Create `/ots-fw-main/src/can_module_service.c`:

```c
// Manages all CAN-based hardware modules
void can_module_service_init(void);
void can_module_service_discover(void);
void can_module_service_update(void);
can_hardware_module_t* can_module_get_by_address(uint8_t addr);
```

### 3. Protocol-Specific Adapters

Create adapters for each module type:

```
/ots-fw-main/src/can_adapters/
├── audio_module_adapter.c     # Wraps sound-module.md protocol
├── display_module_adapter.c   # Future display module
├── lighting_module_adapter.c  # Future lighting module
└── generic_module_adapter.c   # Fallback for unknown types
```

## Migration Path

### Phase 1: Current Implementation (Audio Module)
- ✅ Generic can_driver component exists
- ✅ Application-specific protocol in can_protocol.h (fw-main)
- ⏳ Add auto-discovery to audio module

### Phase 2: Generic Protocol Layer
- Add can_protocol.h to component
- Implement module registry
- Implement message router
- Keep audio protocol as first module-specific adapter

### Phase 3: Multi-Module Support
- Implement discovery protocol
- Add module registry to fw-main
- Create adapter pattern for module types
- Support hot-plug detection

### Phase 4: Advanced Features
- Firmware update over CAN
- Bulk data transfer (multi-frame)
- Time synchronization
- Bus monitoring/diagnostics

## Example: Adding a New Module Type

### 1. Define Module Type

```c
// In can_protocol.h
#define MODULE_TYPE_LIGHTING 0x04
```

### 2. Create Protocol Adapter

```c
// lighting_module_adapter.c
void lighting_module_init(uint8_t can_addr) {
    // Register handlers
    can_register_handler(MSG_CMD_SET_STATE, lighting_handle_set_state);
}

void lighting_handle_set_state(const can_frame_t *frame, uint8_t addr) {
    // Parse lighting-specific command
    uint8_t brightness = frame->data[2];
    uint8_t color_r = frame->data[3];
    uint8_t color_g = frame->data[4];
    uint8_t color_b = frame->data[5];
    // Apply to hardware
}
```

### 3. Register in fw-main

```c
// In module discovery callback
if (info.module_type == MODULE_TYPE_LIGHTING) {
    lighting_module_init(module_addr);
    module_manager_register(lighting_module_get());
}
```

## Benefits of This Architecture

1. **Zero Changes to can_driver**: Hardware layer stays generic
2. **Module Independence**: Each module type has its own adapter
3. **Automatic Discovery**: No manual configuration needed
4. **Hot-Plug Support**: Detect modules added/removed at runtime
5. **Backward Compatible**: Existing audio module works as-is
6. **Future Proof**: Easy to add new module types
7. **Testable**: Can simulate modules without hardware

## References

- CAN 2.0B Specification: ISO 11898
- CANopen: Generic device profile standard
- J1939: Heavy-duty vehicle CAN protocol (similar addressing scheme)
- Current implementation: `/components/can_driver/`
- Sound module protocol: `/ots-hardware/modules/sound-module.md`
