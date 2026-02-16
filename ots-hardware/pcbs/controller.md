# Controller Board (ESP32-S3)

Main controller PCB for the Openfront Tactical Suitcase.

## Overview

The Controller Board is the central hub of the OTS system, hosting an ESP32-S3 microcontroller that:
- Runs the main firmware (`ots-fw-main`)
- Hosts a WebSocket server for communication with `ots-simulator`
- Acts as I2C and CAN bus master
- Coordinates all hardware modules
- Manages power distribution

**KiCAD Project**: `kicad/controller_rev2.zip`

## Specifications

| Property | Value |
|----------|-------|
| **MCU** | Espressif ESP32-S3-WROOM-1 (or DevKit) |
| **Flash** | 4MB+ (8MB recommended) |
| **PSRAM** | 2MB+ (optional but recommended) |
| **WiFi** | 802.11 b/g/n (2.4GHz) |
| **PCB Size** | TBD |
| **Layers** | 2-layer FR4 (TBD) |
| **Power Input** | 12V (from main power module) |
| **Power Output** | 5V and 3.3V rails for modules |

## Block Diagram

```
+12V Input --> [Power Section] --> +5V, +3.3V
                                     |
                           +---------+---------+
                           |   ESP32-S3 Core   |
                           | - WiFi            |
                           | - WebSocket       |
                           | - Firmware        |
                           +---------+---------+
                                     |
                   +-----------------+-----------------+
                   |                 |                 |
             [I2C Master]      [CAN Master]     [Debug/USB]
                   |                 |                 |
            to MCP23017         to CAN Bus        USB-C Port
            to LCD (0x27)       (Sound, Keypad)   (programming)
            to ADC (0x48)
```

## Power Design

### Input
- **12V from Main Power Module**: Nominal 12V, 2A capable

### Regulators
- **Buck Converter**: 12V → 5V, 2A (e.g., LM2596 or switching module)
- **LDO Regulator**: 5V → 3.3V, 1A (e.g., AMS1117-3.3)

### Power Tree

```
+12V Main Power
   |
   +--[Buck 12V→5V]---> +5V Rail (for modules, 2A max)
         |
         +--[LDO 5V→3.3V]---> +3.3V Rail (ESP32-S3, MCP23017, 1A)
```

### Current Budget

| Load | Current | Rail |
|------|---------|------|
| ESP32-S3 (WiFi active) | ~200-300mA | 3.3V |
| MCP23017 Boards (2x) | ~160mA | 3.3V |
| LCD Display | ~50mA | 5V |
| LEDs (via MCP23017) | ~150mA | 3.3V/5V |
| **Total 3.3V** | ~600-700mA | - |
| **Total 5V** | ~200mA | - |

**Design margin**: 1.5x safety factor, use 1A LDO for 3.3V, 2A buck for 5V.

## Interfaces

### I2C Bus
- **Master**: ESP32-S3 GPIO pins (configurable, typically GPIO21=SDA, GPIO22=SCL)
- **Pull-ups**: 4.7kΩ to +3.3V on SDA/SCL
- **Devices**:
  - MCP23017 Board 0 (0x20)
  - MCP23017 Board 1 (0x21)
  - LCD Display (0x27 or 0x3F)
  - ADC (ADS1115, 0x48)

### CAN Bus
- **Controller**: ESP32-S3 built-in TWAI (CAN) peripheral
- **Transceiver**: TJA1050 or MCP2551 (3.3V to 5V level conversion)
- **Termination**: 120Ω resistor (switchable or permanent)
- **Devices**:
  - Sound Module (ESP32-A1S)
  - Keypad Module (M5Stack Stamp S3)

### USB
- **Connector**: USB-C
- **Purpose**: Programming, debugging, serial console
- **IC**: ESP32-S3 has built-in USB (no external USB-UART needed)

### GPIO Expansion Headers
- Breakout unused ESP32-S3 pins for prototyping/future expansion
- Include power rails (+3.3V, +5V, GND)

## Schematic Overview

*(To be completed with detailed schematic sections)*

### Section 1: Power Input and Protection
- Reverse polarity protection (Schottky diode or P-FET)
- TVS diode for overvoltage protection
- Input fuse

### Section 2: Regulators
- Buck converter (12V → 5V)
- LDO regulator (5V → 3.3V)
- Decoupling capacitors

### Section 3: ESP32-S3 Core
- ESP32-S3-WROOM-1 module
- Boot/reset buttons
- USB-C connector
- Crystal/capacitors (if not using module)

### Section 4: I2C Interface
- Pull-up resistors (4.7kΩ)
- Connector to I/O expander boards

### Section 5: CAN Interface
- TJA1050 transceiver
- 120Ω termination resistor
- CAN H/L connector

### Section 6: Debug/Expansion
- GPIO headers
- Test points
- Status LED

## PCB Layout Considerations

- **Power planes**: Separate analog/digital grounds if using ADC
- **Decoupling**: Capacitors close to ESP32-S3 VDD pins
- **USB**: Follow USB-C design guidelines (differential pairs, length matching)
- **CAN**: Twisted pair routing for CAN H/L, 120Ω controlled impedance
- **I2C**: Short traces, avoid running parallel to high-speed signals

## BOM (Key Components)

*(To be completed with full BOM)*

| Qty | Reference | Part Number | Description |
|-----|-----------|-------------|-------------|
| 1 | U1 | ESP32-S3-WROOM-1 | WiFi+BLE MCU module |
| 1 | U2 | LM2596 | Buck converter 12V→5V |
| 1 | U3 | AMS1117-3.3 | LDO regulator 5V→3.3V |
| 1 | U4 | TJA1050 | CAN transceiver |
| 2 | R1, R2 | 4.7kΩ | I2C pull-ups |
| 1 | R3 | 120Ω | CAN termination |
| Various | C | Capacitors | Decoupling (100nF, 10µF, etc.) |
| 1 | J1 | USB-C connector | Programming port |
| 1 | J2 | Power input | Screw terminal or barrel jack |

## Testing & Bring-Up

1. **Visual inspection**: Check for shorts, correct component placement
2. **Power-on test**: Verify +5V and +3.3V rails
3. **USB connection**: Connect to PC, verify ESP32-S3 detected
4. **Flash firmware**: Upload `ots-fw-main` via USB
5. **I2C scan**: Verify MCP23017 boards detected at 0x20, 0x21
6. **CAN test**: Send/receive test frames with CAN analyzer
7. **WiFi test**: Connect to network, verify WebSocket server starts

## Known Issues / Errata

### Rev 2
- *(To be filled in after prototyping)*

## Future Improvements

- [ ] Add SD card slot for data logging
- [ ] Ethernet port for wired connectivity option
- [ ] Battery backup for RTC/configuration
- [ ] More status LEDs (WiFi, CAN, I2C activity)

## Related Documentation

- **Hardware Spec**: [../hardware-spec.md](../hardware-spec.md) - System architecture
- **I/O Expander Boards**: [io-expander-board.md](io-expander-board.md)
- **Firmware**: `/ots-fw-main/` - Controller firmware
- **Module Bus**: See hardware-spec.md for bus protocol details
- **ESP32-S3 Datasheet**: [Espressif Documentation](https://www.espressif.com/en/products/socs/esp32-s3)
