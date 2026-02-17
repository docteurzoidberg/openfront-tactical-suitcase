# OTS Keypad Module Firmware

Firmware for the 15-key RGB mechanical keyboard module with dual-mode operation.

## Hardware Platform

- **MCU**: M5Stack Stamp S3 (ESP32-S3)
- **Keys**: 15× Cherry MX compatible switches
- **Matrix**: 3 rows × 7 columns (3×7 = 21 positions, 15 used)
- **RGB LEDs**: 15× SK6812-MINI-E (WS2812-compatible)
- **CAN**: TJA1050 transceiver
- **USB**: USB-C (HID keyboard mode)
- **Power**: 5V from CAN bus or USB

## Operating Modes

### CAN Bus Mode (Integrated OTS)
- Connected to main controller via CAN bus
- Sends key events to main controller
- Receives RGB LED commands from main controller
- Game state indication via RGB LEDs

### USB HID Mode (Standalone)
- Acts as standard USB keyboard
- User-programmable key bindings
- RGB LED patterns for visual feedback
- No CAN bus communication

## Firmware Architecture

The keypad firmware follows a simple, layered architecture:

```
[Physical Matrix] → matrix_scanner.c (debounce, event detection)
                         ↓
                    [Key Events: K1-K15, press/release]
                         ↓
                    can_handler.c → CAN bus → Main Controller
                         ↑
                    [LED Commands from Main Controller]
                         ↓
                    led_controller.c → RGB LEDs
```

**Design Principles:**
- **Stateless**: Keypad has no game logic or key bindings
- **Simple reporting**: Send raw key events (K1-K15) to main controller
- **LED follower**: Display LED states commanded by main controller
- **USB-ready**: Matrix scanner is generic, output layer swappable (CAN/USB HID)

**Key mapping & game logic**: Handled by main controller (see `MAIN_CONTROLLER_UPDATE_PLAN.md`)

**Module Structure:**
```
src/
  main.c              - Init, mode detection, task coordination
  matrix_scanner.c    - Key matrix scanning with debounce
  led_controller.c    - RGB LED control (SK6812 via RMT)
  can_handler.c       - CAN communication (events out, LED in)
include/
  config.h            - Hardware configuration (GPIO pins)
  matrix_scanner.h    - Matrix scanning API
  led_controller.h    - LED control API
  can_handler.h       - CAN protocol API
```

## Features to Implement

### Phase 1: Hardware Basics
- [ ] **Key Matrix Scanning**
  - 3×7 matrix (3 row outputs, 7 column inputs with pull-ups)
  - Debouncing algorithm
  - Key press/release event generation
  - GPIO pin assignments (see PCB documentation)

- [ ] **RGB LED Control**
  - SK6812-MINI-E driver (WS2812-compatible protocol)
  - 15 LEDs daisy-chained on single data line
  - Individual LED color control (24-bit RGB)
  - Brightness control
  - LED pattern/animation support

### Phase 2: CAN Bus Communication
- [ ] **CAN Driver Integration**
  - Use shared `can_driver` component from `ots-fw-shared`
  - TJA1050 transceiver configuration
  - CAN message TX/RX

- [ ] **CAN Protocol**
  - Module discovery/announcement
  - Key event messages (press/release with key ID)
  - RGB LED command handling (set color, pattern)
  - Status reporting

- [ ] **Module Discovery**
  - Use shared `can_protocol_discovery` component
  - Respond to MODULE_QUERY from main controller
  - Send MODULE_ANNOUNCE on boot

### Phase 3: USB HID Mode
- [ ] **USB HID Keyboard**
  - USB device stack configuration
  - HID keyboard descriptor
  - Key code mapping (configurable)
  - Modifier keys support (if needed)

- [ ] **Mode Detection**
  - Auto-detect CAN bus vs USB mode
  - Boot into appropriate mode
  - Status LED indication (CAN = pattern A, USB = pattern B)

### Phase 4: Configuration & Storage
- [ ] **NVS Configuration**
  - Key bindings (for USB mode)
  - RGB LED preferences
  - Operating mode preference
  - Calibration data (if needed)

- [ ] **Configuration Interface**
  - Special key combination to enter config mode
  - RGB LED feedback during configuration
  - Save/load settings from NVS

### Phase 5: Advanced Features
- [ ] **RGB LED Effects**
  - Breathing effect
  - Rainbow cycle
  - Wave patterns
  - Key-press reactive animations
  - Game state indicators (cooldowns, availability)

- [ ] **Power Management**
  - Sleep mode when idle
  - USB suspend handling
  - Low-power LED dimming

- [ ] **Diagnostics**
  - Self-test on boot (test all keys + LEDs)
  - Error reporting via CAN or serial
  - Factory reset function

## Key Matrix Layout

```
        Col0  Col1  Col2  Col3  Col4  Col5  Col6
Row0:    K1    K2    K3    K4    K5    K6    K7
Row1:    K8    K9    K10   K11   K12   K13   K14
Row2:    --    --    --    K15   --    --    --
```

**Pin Assignments** (from PCB schematic):
- Row 0: GPIO 6 (output, active high)
- Row 1: GPIO 7 (output, active high)
- Row 2: GPIO 8 (output, active high)
- Col 0: GPIO 9 (input with pull-up)
- Col 1: GPIO 10 (input with pull-up)
- Col 2: GPIO 11 (input with pull-up)
- Col 3: GPIO 12 (input with pull-up)
- Col 4: GPIO 13 (input with pull-up)
- Col 5: GPIO 14 (input with pull-up)
- Col 6: GPIO 21 (input with pull-up)
- RGB Data: GPIO 3 (SK6812 data line)
- CAN TX: GPIO 5 (to TJA1050)
- CAN RX: GPIO 4 (from TJA1050)

## CAN Protocol Messages

**Note**: The keypad firmware reports **raw key events only** (K1-K15). The main controller handles all key mapping, game logic, and LED state decisions. See `MAIN_CONTROLLER_UPDATE_PLAN.md` for details.

### Outgoing (Keypad → Main Controller)

**Key Event:**
```
CAN ID: 0x430 + key_id (1-15)
DLC: 4
Data[0]: Key ID (1-15)
Data[1]: State (0x01=pressed, 0x00=released)
Data[2-3]: Timestamp (uint16_t, milliseconds since boot, for debounce verification)
```

**Module Announcement:**
Uses `can_protocol_discovery` component:
```
CAN ID: 0x7FD (MODULE_ANNOUNCE)
Data: Module type = 0x02 (keypad), firmware version, capabilities
```

### Incoming (Main Controller → Keypad)

**Set LED State (Single Key):**
```
CAN ID: 0x430
DLC: 5
Data[0]: Key ID (1-15, or 0xFF for all)
Data[1]: State (0=off, 1=on)
Data[2]: Red (0-255)
Data[3]: Green (0-255)
Data[4]: Blue (0-255)
```

**Set LED State (Bulk):**
```
CAN ID: 0x440
DLC: 8
Data[0-1]: Key bitmask (uint16_t, bit 0=K1, bit 14=K15, little-endian)
Data[2]: Red (0-255)
Data[3]: Green (0-255)
Data[4]: Blue (0-255)
Data[5]: State (0=off, 1=on)
Data[6-7]: Reserved
```

## Directory Structure

```
ots-fw-keypad/
  CMakeLists.txt              # ESP-IDF project config
  platformio.ini              # PlatformIO environment
  sdkconfig.defaults          # ESP-IDF default config
  partitions_no_ota.csv       # Partition table
  PROJECT_PROMPT.md           # This file
  README.md                   # Build instructions
  src/
    CMakeLists.txt            # Component definition
    main.c                    # Entry point
    key_matrix.c/h            # Key scanning (TODO)
    rgb_led.c/h               # LED control (TODO)
    can_interface.c/h         # CAN communication (TODO)
    usb_hid.c/h               # USB keyboard mode (TODO)
    config.c/h                # NVS configuration (TODO)
```

## Build Instructions

```bash
cd ots-fw-keypad

# PlatformIO
pio run -e m5stack-stamps3-espidf          # Build
pio run -e m5stack-stamps3-espidf -t upload # Flash
pio device monitor                          # Serial monitor

# ESP-IDF (if not using PlatformIO)
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Testing

1. **Matrix Test**: Short each row/column combination, verify key detection
2. **RGB Test**: Cycle through all 15 LEDs with different colors
3. **CAN Test**: Connect to CAN bus, verify message TX/RX
4. **USB Test**: Connect via USB, verify keyboard enumeration and key presses

## Related Documentation

- **Hardware Module**: [/ots-hardware/modules/keypad-module.md](../ots-hardware/modules/keypad-module.md)
- **PCB Design**: [/ots-hardware/pcbs/keypad.md](../ots-hardware/pcbs/keypad.md)
- **CAN Protocol**: [/prompts/CANBUS_MESSAGE_SPEC.md](../prompts/CANBUS_MESSAGE_SPEC.md)
- **Shared Components**: [/ots-fw-shared/components/](../ots-fw-shared/components/)

## TODO

- [ ] Define exact GPIO pin assignments (match PCB)
- [ ] Implement key matrix scanning
- [ ] Implement SK6812 LED driver
- [ ] Integrate CAN bus components
- [ ] Implement USB HID mode
- [ ] Create configuration system
- [ ] Add LED effects library
- [ ] Implement mode detection/switching
- [ ] Add diagnostic/self-test features
