# CAN Bus Multi-Module Support - Implementation Roadmap

## Current Status (January 2026)

✅ **Hardware Layer Complete**
- Generic `can_driver` component with auto-detection
- Mock mode for development without hardware
- TWAI physical bus support with fallback
- Statistics tracking and error handling
- Shared between main controller and audio module
- Hardware auto-detection at runtime
- **Location:** `/ots-fw-shared/components/can_driver/`
- **Documentation:** `/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md`

✅ **Discovery Protocol Complete** ✅ NEW
- Boot-time module detection (no heartbeat overhead)
- Module type identification (AUDIO = 0x01)
- Version tracking and capability flags
- CAN block allocation (audio uses 0x420-0x42F)
- Graceful degradation if modules missing
- **Location:** `/ots-fw-shared/components/can_discovery/`
- **Documentation:** `/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`

✅ **Audio Module Protocol Implemented**
- Application-specific protocol in CAN block 0x420-0x42F
- Discovery integration (responds to MODULE_QUERY)
- Sound play/stop commands with queue tracking
- Both main controller and audio module integrated
- **Build Status:** Both projects compile successfully
- **Documentation:** `/ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md`

## Future Multi-Module Architecture

### Problem Statement

The OTS suitcase will expand to support multiple CAN-based modules:
- Display modules (LCD/OLED screens)
- Lighting modules (LED strips, RGB indicators)
- Sensor modules (environmental, motion, etc.)
- Custom I/O modules (additional buttons, encoders)
- Power management modules

**Challenge:** We don't know what module types will be added in the future.

**Solution:** ✅ Generic discovery protocol implemented + future routing enhancement.

### Architecture Overview

See **`/ots-fw-shared/prompts/CAN_PROTOCOL_ARCHITECTURE.md`** for complete specification.

**Key concepts:**
1. ✅ **Boot-time Discovery**: Automatic module detection at startup (implemented)
2. ✅ **Module Type Registry**: Each module has type ID and version (implemented)
3. ✅ **CAN Block Allocation**: Modules get dedicated CAN ID ranges (implemented)
4. 📋 **Message Routing**: Route incoming messages to appropriate handlers (future)
5. 📋 **Module Type Adapters**: Each module type has its own protocol handler (future)

### Discovery Protocol (✅ Implemented)

**CAN IDs:**
- **0x410**: MODULE_ANNOUNCE (module → main)
- **0x411**: MODULE_QUERY (main → all modules)

**Flow:**
```
Main Controller                Audio Module
     │                              │
     │ ──── MODULE_QUERY (0x411) ──→│
     │                              │
     │ ←── MODULE_ANNOUNCE (0x410) ─│
     │     (type=AUDIO, v1.0)       │
     │                              │
[Wait 500ms]                        │
[Build registry]                    │
     ▼                              ▼
  Ready                          Ready
```

**Module Types:**
- 0x00: NONE (reserved)
- 0x01: AUDIO (✅ implemented)
- 0x02-0xFF: Future modules

**Capabilities:**
- bit 0: MODULE_CAP_STATUS (sends periodic status)
- bit 1: MODULE_CAP_OTA (supports OTA updates)
- bit 2: MODULE_CAP_BATTERY (battery powered)

### CAN ID Allocation

```
0x000-0x00F - System broadcast
0x010-0x3FF - Reserved for future generic protocol
0x410-0x411 - Discovery protocol (✅ implemented)
0x420-0x42F - Audio module (✅ allocated)
0x430-0x43F - Display module (future)
0x440-0x44F - Lighting module (future)
0x450-0x7FF - Other module types (future)
```

## Implementation Phases

### Phase 1: Current (Audio Module + Discovery)

**Status:** ✅ Complete (January 2026)

**Components:**
- ✅ `can_driver` - Generic hardware layer
- ✅ `can_discovery` - Boot-time module detection
- ✅ `can_protocol.{h,c}` - Audio-specific protocol
- ✅ `sound_module.c` - Audio module integration with discovery

**Features:**
- ✅ Audio module auto-detected at boot
- ✅ Main controller disables sound if no module found
- ✅ Version tracking (audio module v1.0)
- ✅ CAN block allocation (0x42 = 0x420-0x42F)
- ✅ Zero runtime discovery overhead (boot-time only)

**Build Status:**
- ✅ ots-fw-main: 1,068,176 bytes (50.9%)
- ✅ ots-fw-audiomodule: 1,161,563 bytes (27.7%)

## Physical CAN Bus Hardware

### Hardware Requirements

- External CAN transceiver chip (e.g., SN65HVD230, MCP2551)
- Connect ESP32 GPIO to CAN TX/RX
- 120Ω termination resistors at bus ends

### Pin Assignments

**Main Controller (ESP32-S3):**
- CAN TX: GPIO21
- CAN RX: GPIO22

**Audio Module (ESP32-A1S):**
- CAN TX: GPIO17 (or other available)
- CAN RX: GPIO16 (or other available)

### TWAI Configuration

The CAN driver uses ESP-IDF's TWAI (Two-Wire Automotive Interface) peripheral:

```c
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    GPIO_NUM_21,  // CAN TX pin
    GPIO_NUM_22,  // CAN RX pin
    TWAI_MODE_NORMAL
);

twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

twai_driver_install(&g_config, &t_config, &f_config);
twai_start();
```

**API Mapping:**
- `can_driver_init()` → Install TWAI driver, configure 500kbps
- `can_driver_send()` → `twai_transmit()` with timeout
- `can_driver_receive()` → `twai_receive()` non-blocking poll

### Testing Strategy

**Mock Mode Testing:**
- Protocol development without hardware
- Message format validation
- Integration with sound module events

**Physical CAN Testing:**
- Loopback test (TX → RX on same device)
- Two-device communication
- Bus error handling
- Performance validation (latency, throughput)

### Phase 2: Multi-Module Registry & Routing

**Status:** 📋 Planned (Future Enhancement)

**Tasks:**
1. ✅ Discovery protocol (DONE)
2. 📋 Implement module registry service in fw-main
3. 📋 Add dynamic message routing
4. 📋 Create adapter pattern for module types
5. 📋 Add module enumeration API
6. 📋 Support hot-plug detection (optional)

**Files to create:**
```
ots-fw-main/src/
├── can_module_service.c      # Module registry and lifecycle (NEW)
└── can_adapters/             # Protocol adapters (NEW)
    ├── audio_adapter.c       # Wraps existing sound_module
    ├── display_adapter.c     # Future display module
    └── generic_adapter.c     # Fallback for unknown types
```

**Discovery already provides:**
- ✅ Module type identification
- ✅ Version tracking
- ✅ CAN block allocation
- ✅ Capability flags

**Future routing would add:**
- 📋 Automatic handler registration
- 📋 Message dispatch to correct adapter
- 📋 Multiple module instances (e.g., 2 displays)
- 📋 Hot-plug detection (optional)

### Phase 3: Multi-Module Support in fw-main

**Status:** 📋 Planned

**Tasks:**
1. Create `can_module_interface.h` (generic CAN module interface)
2. Implement `can_module_service.c` (module registry management)
3. Create `can_adapters/` directory for module-specific protocols
4. Migrate audio protocol to adapter pattern
5. Add discovery on boot and periodic polling

**File structure:**
```
ots-fw-main/
├── include/
│   └── can_module_interface.h
├── src/
│   ├── can_module_service.c
│   └── can_adapters/
│       ├── audio_module_adapter.c
│       ├── display_module_adapter.c
│       └── generic_module_adapter.c
```

**Example adapter registration:**
```c
void can_module_service_init(void) {
    // Register known module type handlers
    can_register_adapter(MODULE_TYPE_AUDIO, &audio_adapter);
    can_register_adapter(MODULE_TYPE_DISPLAY, &display_adapter);
    // Unknown types use generic adapter
    
    // Start discovery
    can_discover_modules();
}
```

### Phase 4: Advanced Features

**Status:** 🔮 Future

**Potential features:**
- Hot-plug detection (modules added/removed at runtime)
- Firmware update over CAN (bootloader protocol)
- Multi-frame bulk data transfer (>8 bytes)
- Time synchronization across modules
- Bus diagnostics and monitoring
- Module configuration persistence
- Error recovery and retry logic

## Adding a New Module Type

### Step 1: Define Module Type

In `components/can_driver/include/can_protocol.h`:
```c
#define MODULE_TYPE_DISPLAY 0x02
```

### Step 2: Create Protocol Adapter

In `ots-fw-main/src/can_adapters/display_module_adapter.c`:
```c
static void display_handle_command(const can_frame_t *frame, uint8_t addr);
static void display_handle_status(const can_frame_t *frame, uint8_t addr);

can_module_adapter_t display_adapter = {
    .module_type = MODULE_TYPE_DISPLAY,
    .init = display_adapter_init,
    .handle_message = display_adapter_handle_message,
    .get_status = display_adapter_get_status
};
```

### Step 3: Register in Service

In `ots-fw-main/src/can_module_service.c`:
```c
can_register_adapter(MODULE_TYPE_DISPLAY, &display_adapter);
```

### Step 4: Hardware Module Integration

```c
// In main.c module registration
if (can_module_is_discovered(MODULE_TYPE_DISPLAY)) {
    module_manager_register(display_module_get());
}
```

That's it! The framework handles discovery, routing, and lifecycle.

## Migration Strategy

### Backward Compatibility

**Audio module continues working unchanged during migration:**

1. Phase 2: Add generic protocol alongside existing audio protocol
2. Phase 3: Migrate audio to adapter pattern (transparent to audio module)
3. New modules use generic protocol from day one

### Testing Strategy

1. **Phase 1 (Current)**: Test audio module in isolation
2. **Phase 2**: Add mock modules with different types, test discovery
3. **Phase 3**: Test multiple real modules on same bus
4. **Phase 4**: Test hot-plug, error recovery, edge cases

### Rollout Plan

1. ✅ Generic can_driver component (done)
2. Implement generic protocol in component
3. Add discovery and registry to component
4. Create adapter pattern in fw-main
5. Migrate audio module to adapter
6. Add second module type (display or lighting)
7. Validate multi-module operation
8. Document module development guide

## Benefits

**For Developers:**
- Add new modules without touching can_driver
- Each module type is independent
- Clear separation of concerns
- Easy to test modules in isolation

**For System:**
- Automatic module detection
- No manual configuration
- Hot-plug support
- Resilient to module failures
- Scalable to 10+ modules

**For Future:**
- Unknown module types supported via generic adapter
- Easy to add new features
- Backward compatible
- Standard protocol for all modules

## Current Component Status

The `can_driver` component (in `/ots-fw-shared/components/`) is **already generic and suitable** for multi-module use:

✅ Hardware abstraction complete  
✅ Auto-detection with fallback  
✅ Statistics and error handling  
✅ Generic frame TX/RX  
✅ No application-specific logic  

**No changes needed to component for multi-module support.**

All multi-module logic will be added as **optional layers on top** of the existing component.

## Next Steps

1. Review [CAN_PROTOCOL_ARCHITECTURE.md](/ots-fw-shared/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md)
2. Decide if multi-module support needed now or later
3. If now: Implement Phase 2 (generic protocol in component)
4. If later: Continue with audio module only

## References

- Component: `/ots-fw-shared/components/can_driver/`
- Architecture: `/ots-fw-shared/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md`
- Audio protocol: `/ots-fw-main/include/can_protocol.h`
- Sound module spec: `/ots-hardware/modules/sound-module.md`
- Hardware spec: `/ots-hardware/hardware-spec.md`
