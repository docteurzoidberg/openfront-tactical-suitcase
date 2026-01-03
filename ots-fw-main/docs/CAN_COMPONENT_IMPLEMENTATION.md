# CAN Driver Component - Implementation Summary

## ✅ Generic Hardware-Level CAN Driver Created

Successfully extracted CAN driver into a reusable ESP-IDF component at `/components/can_driver/`.

## Architecture

### Generic Hardware Layer (Shared Component)
**Location:** `/components/can_driver/`
- **can_driver.h** - Generic CAN frame TX/RX interface
- **can_driver.c** - Hardware abstraction (mock mode + TWAI placeholders)
- **CMakeLists.txt** - ESP-IDF component build config
- **idf_component.yml** - Component dependencies

**Features:**
- Generic `can_frame_t` structure (ID, DLC, data, extended, RTR)
- Hardware-agnostic API (no protocol specifics)
- Mock mode for development without hardware
- Statistics tracking (TX/RX counts, errors)
- Filter configuration support
- Thread-safe operation

### Application Protocol Layer (Firmware-Specific)
**Location:** `/ots-fw-main/include/can_protocol.h` + `src/can_protocol.c`
- Sound module CAN protocol definitions (IDs 0x420-0x423)
- Helper functions: `can_build_play_sound()`, `can_build_stop_sound()`
- Parser functions: `can_parse_sound_status()`, `can_parse_sound_ack()`
- Protocol constants and flags

## Component Structure

```
components/can_driver/
├── CMakeLists.txt              # Component build config
├── idf_component.yml           # Dependencies (idf >= 5.0)
├── README.md                   # Usage documentation
├── include/
│   └── can_driver.h           # Public API
└── can_driver.c                # Implementation
```

## Usage in Firmware

### ots-fw-main

**CMakeLists.txt:**
```cmake
REQUIRES can_driver  # Added to component requirements
```

**Code:**
```c
#include "can_driver.h"      // Generic driver
#include "can_protocol.h"    // Protocol definitions

// Initialize
can_config_t config = CAN_CONFIG_DEFAULT();
can_driver_init(&config);

// Build protocol message
can_frame_t frame;
can_build_play_sound(sound_index, flags, volume, req_id, &frame);

// Send via generic driver
can_driver_send(&frame);
```

### ots-fw-audiomodule (Future)

**CMakeLists.txt:**
```cmake
REQUIRES can_driver  # Same generic driver
```

**Code:**
```c
#include "can_driver.h"

// Receive frames
can_frame_t frame;
if (can_driver_receive(&frame, 100) == ESP_OK) {
    // Parse PLAY_SOUND command
    if (frame.id == 0x420 && frame.data[0] == 0x01) {
        uint16_t sound_idx = frame.data[2] | (frame.data[3] << 8);
        // Play sound from SD card
    }
}
```

## Current Mode: Mock

**Behavior:**
- CAN frames logged to serial output
- No physical bus transmission
- Perfect for protocol development
- Statistics tracking functional

**Serial Output:**
```
[CAN_DRV] Initializing CAN driver in MOCK mode
[CAN_PROTO] Build PLAY_SOUND: idx=10 flags=0x02 vol=255 reqID=1
[CAN_DRV] TX: ID=0x420 DLC=8 RTR=0 EXT=0 DATA=[01 02 0A 00 FF 00 01 00]
```

## Future: Physical TWAI Implementation

**Hardware Requirements:**
- External CAN transceiver (SN65HVD230, MCP2551, TJA1050)
- 120Ω termination resistors at bus ends
- Connect ESP32 GPIOs to transceiver TX/RX

**Code Changes:**
1. Uncomment TWAI initialization in `can_driver_init()`
2. Uncomment TWAI TX/RX code
3. Set `mock_mode = false` in config
4. Define GPIO pins for TX/RX

All TWAI code is present but commented out with `// TODO:` markers.

## API Reference

### Generic Driver Functions

| Function | Description |
|----------|-------------|
| `can_driver_init()` | Initialize with config |
| `can_driver_deinit()` | Shutdown driver |
| `can_driver_send()` | Send CAN frame |
| `can_driver_receive()` | Receive frame (non-blocking) |
| `can_driver_get_stats()` | Get TX/RX statistics |
| `can_driver_reset_stats()` | Clear statistics |
| `can_driver_rx_available()` | Check RX queue depth |
| `can_driver_set_filter()` | Configure acceptance filter |

### Protocol Helper Functions (fw-main)

| Function | Description |
|----------|-------------|
| `can_build_play_sound()` | Build PLAY_SOUND frame (0x420) |
| `can_build_stop_sound()` | Build STOP_SOUND frame (0x421) |
| `can_parse_sound_status()` | Parse STATUS frame (0x422) |
| `can_parse_sound_ack()` | Parse ACK frame (0x423) |

## Files Modified

**Created:**
- `/components/can_driver/CMakeLists.txt`
- `/components/can_driver/idf_component.yml`
- `/components/can_driver/include/can_driver.h`
- `/components/can_driver/can_driver.c`
- `/components/can_driver/README.md`

**Updated:**
- `/ots-fw-main/include/can_driver.h` → renamed to `can_protocol.h`
- `/ots-fw-main/src/can_driver.c` → renamed to `can_protocol.c`
- `/ots-fw-main/src/CMakeLists.txt` - Added `can_driver` component requirement
- `/ots-fw-main/src/sound_module.c` - Updated includes

## Benefits

✅ **Reusable:** Both firmwares use identical generic driver  
✅ **Modular:** Protocol logic separated from hardware layer  
✅ **Testable:** Mock mode enables development without hardware  
✅ **Maintainable:** Single source of truth for CAN operations  
✅ **Extensible:** Easy to add physical TWAI support later  
✅ **Standard:** Follows ESP-IDF component conventions

## Next Steps

1. **Audio Firmware:** Add `REQUIRES can_driver` and implement RX handler
2. **Physical CAN:** Uncomment TWAI code and test with hardware
3. **Protocol:** Add STATUS/ACK parsing to fw-main
4. **Testing:** Validate end-to-end communication between firmwares

## References

- Component: `/components/can_driver/`
- Protocol Layer: `/ots-fw-main/include/can_protocol.h`
- Sound Module: `/ots-fw-main/src/sound_module.c`
- Hardware Spec: `/ots-hardware/modules/sound-module.md`
- ESP-IDF TWAI: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html
