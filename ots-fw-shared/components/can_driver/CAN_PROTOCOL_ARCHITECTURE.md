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

### Startup Sequence

```
1. Controller boots, initializes CAN bus
2. Controller broadcasts WHO_IS_THERE (0x10, addr 0xF)
3. Modules respond with I_AM_HERE (0x11, their addr)
4. Controller queries each module for detailed info (0x12)
5. Modules respond with type, version, capabilities (0x13)
6. Controller builds module registry
```

### Module Info Response Format

```c
typedef struct {
    uint8_t msg_class;        // 0x13 (MSG_DISC_INFO_RESPONSE)
    uint8_t module_type;      // Module type ID
    uint8_t hw_version;       // Hardware version
    uint8_t fw_version_major; // Firmware major version
    uint8_t fw_version_minor; // Firmware minor version
    uint8_t capabilities;     // Capability flags
    uint16_t vendor_id;       // Manufacturer ID (optional)
} __attribute__((packed)) module_info_t;
```

### Module Type Registry

```c
#define MODULE_TYPE_UNKNOWN     0x00
#define MODULE_TYPE_AUDIO       0x01
#define MODULE_TYPE_DISPLAY     0x02
#define MODULE_TYPE_INPUT       0x03
#define MODULE_TYPE_LIGHTING    0x04
#define MODULE_TYPE_HAPTIC      0x05
#define MODULE_TYPE_SENSOR      0x06
#define MODULE_TYPE_POWER       0x07
// 0x08-0x7F: Future module types
// 0x80-0xFF: Custom/experimental
```

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
