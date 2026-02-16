# Keypad PCB

Electrical design for the 15-key RGB mechanical keyboard module.

## Overview

Custom PCB for the Keypad Module featuring:
- 15× Cherry MX mechanical switch footprints
- 15× SK6812-MINI-E RGB LED footprints (reverse-mount)
- M5Stack Stamp S3 controller module
- Dual-mode operation: CAN bus OR USB HID

**KiCAD Project**: `kicad/keyboard_rev1.zip`

**Note**: Detailed mechanical integration, key matrix, and firmware behavior are documented in [Keypad Module](../modules/keypad-module.md). This document focuses on PCB electrical design.

## Specifications

| Property | Value |
|----------|-------|
| **MCU** | M5Stack Stamp S3 (ESP32-S3) |
| **Switches** | 15× Cherry MX compatible |
| **LEDs** | 15× SK6812-MINI-E (WS2812-style RGB) |
| **Matrix** | 3 rows × 7 columns (3×7 = 21 positions, 15 used) |
| **CAN Transceiver** | TJA1050 or MCP2551 |
| **USB** | USB-C connector (HID mode + power) |
| **PCB Size** | 4U module size (~200mm × 50mm) |
| **Layers** | 2-layer FR4, 1.6mm |

## Key Matrix Layout

```
        Col0  Col1  Col2  Col3  Col4  Col5  Col6
Row0:    K1    K2    K3    K4    K5    --    --
Row1:    K6    K7    K8    K9    K10   --    --
Row2:    K11   K12   K13   K14   K15   --    --
```

- **Row pins**: 3 GPIO (scanned outputs)
- **Column pins**: 7 GPIO (polled inputs with internal pull-ups)
- **Unused positions**: Col5,Col6 on all rows (for future expansion)

## Schematic Sections

### 1. M5Stack Stamp S3 Core
- Surface-mount header for Stamp S3 module
- Power supply (5V → 3.3V via Stamp's onboard LDO)
- Boot/reset control

### 2. Key Matrix
- 3× row GPIO → diodes → switch matrix
- 7× column GPIO ← switch matrix (with internal pull-ups)
- Anti-ghosting diodes (1N4148 or BAV70 dual diode)

### 3. RGB LEDs (SK6812-MINI-E)
- Reverse-mounted on PCB back side
- Daisy-chained data line (one GPIO from Stamp S3)
- 5V power rail, 100µF bulk capacitor

### 4. CAN Bus Interface
- TJA1050 transceiver (SOIC-8)
- 120Ω termination resistor (optional jumper)
- Bus connector (to main controller)

### 5. USB Interface
- USB-C connector
- Direct connection to Stamp S3 USB pins
- ESD protection (optional TVS diodes)

### 6. Power
- Dual input: USB-C OR bus connector
- Schottky diode OR circuit to prevent conflict
- 5V → SK6812 LEDs directly
- 5V → Stamp S3 (internal LDO to 3.3V)

## PCB Layout Highlights

- **Switch footprints**: 19.05mm spacing (standard MX layout)
- **Front side**: Switches, diodes, Stamp S3 module
- **Back side**: SK6812 LEDs (reverse-mount), light shines through PCB/switches to keycaps
- **RGB routing**: Single trace daisy-chaining all 15 SK6812 LEDs
- **USB-C**: Centered or right-edge placement for easy access

## Power Budget

| Component | Current | Notes |
|-----------|---------|-------|
| Stamp S3 (idle) | ~50mA | 3.3V via onboard LDO |
| Stamp S3 (WiFi) | ~200mA | CAN-only mode uses less |
| SK6812 LEDs (all white, max) | ~900mA | 15× 60mA per LED |
| TJA1050 transceiver | ~10mA | CAN mode only |
| **Total (worst case)** | ~1.2A | From 5V supply |

**Design**: Use USB-C for power when standalone (USB HID mode), or bus connector when integrated (CAN mode).

## BOM (Key Components)

| Qty | Reference | Part Number | Description |
|-----|-----------|-------------|-------------|
| 1 | U1 | M5Stack Stamp S3 | ESP32-S3 module |
| 15 | SW1-SW15 | Cherry MX compatible | Mechanical switches (user provided) |
| 15 | D1-D15 | 1N4148 or BAV70 | Anti-ghosting diodes |
| 15 | LED1-LED15 | SK6812-MINI-E | RGB LEDs (reverse mount) |
| 1 | U2 | TJA1050 | CAN transceiver (SOIC-8) |
| 1 | R1 | 120Ω | CAN termination resistor |
| 1 | J1 | USB-C connector | Power + HID mode |
| 1 | J2 | Bus connector | CAN + power (OTS integrated mode) |
| 2 | C1, C2 | 100nF | Decoupling caps |
| 1 | C3 | 100µF | LED power bulk cap |

## Assembly Notes

1. **LED orientation critical**: SK6812-MINI-E has specific pin 1 marking, check datasheet
2. **Reverse mount**: LEDs solder on PCB back, light shines through to front
3. **Switch installation**: Front panel plate mounts switches, then switches solder to PCB
4. **Diode polarity**: Ensure correct anode/cathode orientation for matrix

## Testing

1. **Visual inspection**: Check LED polarity, diode orientation
2. **Power test**: Verify 5V and 3.3V (from Stamp S3)
3. **USB connection**: Connect to PC, verify Stamp S3 enumeration
4. **Matrix test**: Flash test firmware, short each row/col combination, verify key detect
5. **RGB test**: Flash LED test pattern, verify all 15 LEDs work
6. **CAN test**: Connect to CAN bus, verify message TX/RX

## Related Documentation

- **Module Integration**: [../modules/keypad-module.md](../modules/keypad-module.md) - Full module specs
- **Firmware**: Key scanning algorithm, RGB control, CAN protocol
- **M5Stack Stamp S3**: [Product page](https://docs.m5stack.com/en/core/StampS3)
- **SK6812-MINI-E**: LED datasheet (WS2812-compatible protocol)
- **TJA1050**: CAN transceiver datasheet
