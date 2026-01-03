# CAN Driver - ESP-IDF Component

## Status: ✅ IMPLEMENTED (Auto-Detection)

Generic hardware-level CAN bus driver for ESP32 using TWAI interface.

**Current Mode:** Physical with automatic fallback to mock  
**Reliability:** Always succeeds - gracefully handles missing hardware

## Features

- **Automatic hardware detection** - tries physical, falls back to mock
- Generic CAN frame TX/RX (no application-specific protocol)
- Standard 11-bit CAN identifiers
- 8-byte payload support
- Statistics tracking (TX/RX counts, errors)
- Multiple bitrates (125k/250k/500k/1M)
- Thread-safe operation
- Zero configuration needed for development

## Directory Structure

```
components/can_driver/
├── CMakeLists.txt          # ESP-IDF component build config
├── idf_component.yml       # Component dependencies
├── README.md               # This file
├── include/
│   └── can_driver.h       # Public API
└── can_driver.c            # Implementation
```

## Usage

### In Your Firmware

**CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES can_driver  # Add this
)
```

**Code Example (Auto-Detection):**
```c
#include "can_driver.h"

void app_main(void) {
    // Initialize with default config
    // Attempts physical hardware, falls back to mock if not present
    can_config_t config = CAN_CONFIG_DEFAULT();
    can_driver_init(&config);  // Always succeeds!
    
    // Send a CAN frame
    can_frame_t frame = {
        .id = 0x123,
        .dlc = 8,
        .data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
        .extended = false,
        .rtr = false
    };
    can_driver_send(&frame);
    
    // Receive a frame (non-blocking)
    can_frame_t rx_frame;
    if (can_driver_receive(&rx_frame, 100) == ESP_OK) {
        // Process received frame
    }
}

## Configuration

**Default (Auto-Detection):**
```c
can_config_t config = CAN_CONFIG_DEFAULT();
// Attempts physical CAN on GPIO21/22 at 500kbps
// Falls back to mock if hardware not detected
```

**Explicit Mock Mode:**
```c
can_config_t config = {
   Automatic Hardware Detection

The driver intelligently handles CAN hardware presence:

**With Physical CAN Hardware:**
```
[CAN_DRV] Attempting to initialize CAN driver in PHYSICAL mode
[CAN_DRV] Physical CAN bus initialized successfully!
[CAN_DRV] Mode: NORMAL | Bitrate: 500000 bps
```

**Without Physical CAN Hardware:**
```
[CAN_DRV] Attempting to initialize CAN driver in PHYSICAL mode
[CAN_DRV] Failed to install TWAI driver: ESP_ERR_INVALID_STATE
[CAN_DRV] This is normal if no CAN transceiver hardware is present
[CAN_DRV] Falling back to MOCK mode
[CAN_DRV] CAN driver running in MOCK mode
```

**Result:** Your application always works, regardless of hardware!

## Modes Explained

### Mock Mode (Software Only)
- ✅ No physical CAN bus required
- ✅ Frames logged to serial output
- ✅ Perfect for protocol development
- ✅ Zero hardware cost
- ❌ No actual bus communication

**Use when:**
- Developing protocol logic
- No CAN transceiver hardware available
- Testing without physical device
- Debugging message formats

### Physical Mode (Hardware CAN)
- ✅ Real CAN bus communication via TWAI
- ✅ Supports 125k/250k/500k/1M bitrates
- ✅ Standard 11-bit and extended 29-bit IDs
- ✅ Hardware message filtering
- ⚠️ Requires external CAN transceiver
- ⚠️ Requires proper bus termination

**Use when:**
- Connecting to real CAN devices
- Production hardware deployment
- Multi-device CAN networkom RX pin
    .bitrate = 250000,      // 250 kbps
    .loopback = false,
   Hardware Requirements (Physical Mode)

**CAN Transceiver Options:**
- **SN65HVD230** - 3.3V, low-power, recommended for ESP32
- **MCP2551** - 5V, common, requires level shifters for ESP32
- **TJA1050** - 5V, robust, requires level shifters
- **VP230** - 3.3V compatible

**Wiring Example (SN65HVD230):**
```
ESP32 GPIO21 (TX) → CAN Transceiver TX
ESP32 GPIO22 (RX) ← CAN Transceiver RX
ESP32 3.3V        → CAN Transceiver VCC
ESP32 GND         → CAN Transceiver GND
CAN Transceiver CANH → CAN Bus High (with 120Ω termination at ends)
CAN Transceiver CANL → CAN Bus Low
```

**Bus Termination:**
- 120Ω resistor between CANH and CANL at **both ends** of bus
- Only two termination points per network
- Without termination: reflections cause errors

## Troubleshooting

### Driver Falls Back to Mock Mode

**Possible causes:**
1. No CAN transceiver connected to GPIO pins
2. Wrong GPIO pin numbers in config
3. CAN transceiver not powered
4. TWAI peripheral not available on chip variant

**Solution:** Check hardware connections, verify GPIO assignment

### Physical Mode: TX Timeout Errors

**Possible causes:**
1. No other device on bus (no ACK)
2. Missing bus termination (120Ω resistors)
3. Bus not properly wired (CANH/CANL swapped)
4. Wrong bitrate (mismatched with other devices)

**Solution:** Verify bus topology, check termination, confirm bitrate

### Physical Mode: RX No Messages

**Possible causes:**
1. No devices transmitting
2. Wrong bitrate configuration
3. Acceptance filter too restrictive
4. Bus wiring issues

**Solution:** Verify bitrate matches network, check ACCEPT_ALL filter is working
- Perfect for protocol development
- Zero hardware cost

**Physical Mode (Future):**
- Requires external CAN transceiver (SN65HVD230, MCP2551, etc.)
- 120Ω termination resistors at bus ends
- Real CAN bus communication via TWAI driver
- Set `mock_mode = false` in config

## API Reference

| Function | Description |
|----------|-------------|
| `can_driver_init()` | Initialize driver with config |
| `can_driver_deinit()` | Deinitialize driver |
| `can_driver_send()` | Send a CAN frame |
| `can_driver_receive()` | Receive a frame (non-blocking) |
| `can_driver_get_stats()` | Get TX/RX statistics |
| `can_driver_reset_stats()` | Reset statistics counters |
| `can_driver_rx_available()` | Check RX queue depth |
| `can_driver_set_filter()` | Set acceptance filter |

## Used By

- **ots-fw-main** - Main controller (ESP32-S3)
- **ots-fw-audiomodule** - Audio module (ESP32-A1S)
- **Future modules** - Any CAN-capable hardware module

## Architecture & Extensibility

This component provides the **hardware layer** only. For multi-module systems with automatic discovery and routing, see:

**[CAN_PROTOCOL_ARCHITECTURE.md](CAN_PROTOCOL_ARCHITECTURE.md)** - Complete protocol architecture for:
- Module discovery and addressing
- Generic message routing
- Module type registry
- Hot-plug support
- Priority-based arbitration
- Example implementation patterns

The current audio module protocol (0x420-0x423) is application-specific and lives in `ots-fw-main/can_protocol.h`. Future modules will use the generic protocol defined in the architecture document.

## Future Implementation

Physical TWAI driver implementation is commented out in the code with `// TODO:` markers. To enable:

1. Uncomment TWAI initialization code in `can_driver_init()`
2. Uncomment TWAI TX code in `can_driver_send()`
3. Uncomment TWAI RX code in `can_driver_receive()`
4. Add external CAN transceiver hardware
5. Set `mock_mode = false` in configuration

## References

- ESP-IDF TWAI Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html
- CAN 2.0B Specification: ISO 11898
- Hardware spec: `/ots-hardware/modules/sound-module.md`
