# CAN Driver - Future Shared Component

## Overview

The CAN driver is currently implemented as mock code within `ots-fw-main` for development purposes. This document outlines the plan to extract it into a shared ESP-IDF component that can be used by both:

- **ots-fw-main** (main controller ESP32-S3)
- **ots-fw-audiomodule** (audio controller ESP32-A1S)

## Current Status (January 2026)

**Location:** 
- `ots-fw-main/include/can_driver.h`
- `ots-fw-main/src/can_driver.c`

**Implementation:** MOCK MODE
- CAN messages are logged to serial output
- No physical CAN bus transmission
- Used for protocol development and testing

## Future Implementation Plan

### 1. Create Shared Component

Create a new ESP-IDF component at project root:

```
ots/
  components/
    can_driver/
      CMakeLists.txt
      idf_component.yml
      include/
        can_driver.h
      src/
        can_driver.c
      README.md
```

### 2. Physical CAN Bus Implementation

The real implementation will use ESP-IDF's **TWAI (Two-Wire Automotive Interface)** driver:

**Hardware Requirements:**
- External CAN transceiver chip (e.g., SN65HVD230, MCP2551)
- Connect ESP32 GPIO to CAN TX/RX
- 120Ω termination resistors at bus ends

**TWAI Configuration:**
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

### 3. Integration Steps

**Step 1: Extract Component**
1. Move `can_driver.h` and `can_driver.c` to `components/can_driver/`
2. Create `CMakeLists.txt`:
   ```cmake
   idf_component_register(
       SRCS "src/can_driver.c"
       INCLUDE_DIRS "include"
       REQUIRES driver
   )
   ```
3. Create `idf_component.yml`:
   ```yaml
   dependencies:
     idf: ">=5.0"
   ```

**Step 2: Update ots-fw-main**
1. Remove local `can_driver.{c,h}` files
2. Add component dependency in `CMakeLists.txt`:
   ```cmake
   REQUIRES can_driver
   ```

**Step 3: Update ots-fw-audiomodule**
1. Add component dependency
2. Implement CAN receive handler for incoming commands
3. Send STATUS messages periodically (1 Hz heartbeat)

**Step 4: Implement Physical TWAI**
1. Add hardware mode flag (compile-time or runtime)
2. Implement TWAI initialization and frame TX/RX
3. Keep mock mode available for testing without hardware

### 4. Pin Assignments

**Main Controller (ESP32-S3):**
- CAN TX: GPIO21
- CAN RX: GPIO22

**Audio Module (ESP32-A1S):**
- CAN TX: GPIO17 (or other available)
- CAN RX: GPIO16 (or other available)

### 5. Testing Strategy

**Mock Mode Testing:**
- Protocol development without hardware
- Message format validation
- Integration with sound module events

**Physical CAN Testing:**
- Loopback test (TX → RX on same device)
- Two-device communication
- Bus error handling
- Performance validation (latency, throughput)

## Protocol Reference

See `/ots-hardware/modules/sound-module.md` for complete CAN protocol specification:
- Message IDs: 0x420-0x423
- Payload formats (8-byte frames)
- Little-endian multi-byte integers
- Flag definitions

## Migration Checklist

- [ ] Create `components/can_driver/` directory structure
- [ ] Move source files to component
- [ ] Create component CMakeLists.txt
- [ ] Update ots-fw-main dependencies
- [ ] Update ots-fw-audiomodule dependencies
- [ ] Implement TWAI driver support
- [ ] Add compile-time mock/physical mode selection
- [ ] Test mock mode still works
- [ ] Test physical CAN with hardware
- [ ] Document pin assignments in both firmware READMEs
- [ ] Update protocol-context.md with CAN implementation status

## References

- ESP-IDF TWAI Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html
- Sound Module Spec: `/ots-hardware/modules/sound-module.md`
- CAN Protocol: ISO 11898 (CAN 2.0B)
