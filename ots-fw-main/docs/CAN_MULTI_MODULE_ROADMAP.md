# CAN Bus Multi-Module Support - Implementation Roadmap

## Current Status (January 2026)

✅ **Hardware Layer Complete**
- Generic `can_driver` component with auto-detection
- Mock mode for development without hardware
- TWAI physical bus support with fallback
- Statistics tracking and error handling

✅ **Audio Module Protocol Implemented**
- Application-specific protocol in `can_protocol.h`
- Message IDs 0x420-0x423 for audio commands
- Sound module integration working

## Future Multi-Module Architecture

### Problem Statement

The OTS suitcase will expand to support multiple CAN-based modules:
- Display modules (LCD/OLED screens)
- Lighting modules (LED strips, RGB indicators)
- Sensor modules (environmental, motion, etc.)
- Custom I/O modules (additional buttons, encoders)
- Power management modules

**Challenge:** We don't know what module types will be added in the future.

**Solution:** Generic protocol with automatic discovery and routing.

### Architecture Overview

See **`/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md`** for complete specification.

**Key concepts:**
1. **Priority-based CAN IDs**: Emergency → High → Normal → Low
2. **Module Discovery**: Automatic enumeration on bus startup
3. **Dynamic Registry**: Controller maintains list of active modules
4. **Message Routing**: Route incoming messages to appropriate handlers
5. **Module Type Adapters**: Each module type has its own protocol handler

### CAN ID Structure

```
11-bit ID: [PPP][TTTT][AAAA]
           Priority(3) Type(4) Address(4)

Example IDs:
0x00F - Emergency broadcast
0x223 - Command to module at address 3
0x345 - Status from module at address 5
```

### Message Classes

- **0x00-0x0F**: System (ping, reset, error)
- **0x10-0x1F**: Discovery & configuration
- **0x20-0x2F**: Generic commands
- **0x30-0x3F**: Responses
- **0x40-0x4F**: Status & events
- **0x50-0xFF**: Module-specific

## Implementation Phases

### Phase 1: Current (Audio Module Only)

**Status:** ✅ Complete

**Components:**
- `can_driver` - Generic hardware layer
- `can_protocol.{h,c}` - Audio-specific protocol
- `sound_module.c` - Audio module integration

**Limitations:**
- Single module type supported
- Fixed addressing (audio at 0x420-0x423)
- No discovery protocol
- Manual configuration

### Phase 2: Generic Protocol Foundation

**Status:** 📋 Planned

**Tasks:**
1. Add `can_protocol.h` to component (generic protocol definitions)
2. Implement `can_module_registry.c` (module discovery and tracking)
3. Implement `can_message_router.c` (dynamic message routing)
4. Add heartbeat/keepalive mechanism
5. Create module type registry

**Files to create:**
```
ots-fw-shared/components/can_driver/
├── include/
│   └── can_protocol.h       # Generic protocol (NEW)
├── can_module_registry.c    # Module management (NEW)
└── can_message_router.c     # Message routing (NEW)
```

**Backward compatibility:** Audio module protocol stays in fw-main as first module-specific adapter.

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
