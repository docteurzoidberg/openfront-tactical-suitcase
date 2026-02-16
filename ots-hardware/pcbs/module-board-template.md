# Module Board Template

Generic reusable PCB frame for OTS hardware modules.

## Overview

The Module Board Template is a standardized PCB design that provides:
- Standard mounting holes for rack integration
- Bus connector footprint (I2C + CAN + Power)
- Prototyping area for custom circuitry
- Optional header footprints for common ICs (MCP23017, etc.)

This template accelerates module development by providing a consistent mechanical and electrical interface.

**KiCAD Project**: `kicad/board_module_template.zip`

## Specifications

| Property | Value |
|----------|-------|
| **Size Options** | 2U (100mm × 50mm), 4U (200mm × 50mm) |
| **Layers** | 2-layer FR4 |
| **Thickness** | 1.6mm |
| **Mounting Holes** | 4× M3 or M4 (matching rack frame) |
| **Power Input** | +5V, +3.3V, +12V (via bus connector) |

## Features

### Standardized Elements

1. **Bus Connector Footprint**
   - JST XH or Molex KK254 (4-6 pin)
   - Pinout: +V, GND, SDA, SCL, CAN_H, CAN_L
   - Polarity protection footprint (optional diode)

2. **Mounting Holes**
   - Corner positions match rack specifications
   - Clearance for standoffs and screws

3. **Power Section**
   - Footprint for optional local regulator (e.g., AMS1117)
   - Input/output capacitor footprints
   - Test points for power rails

4. **Prototyping Area**
   - Grid of plated through-holes (0.1" spacing)
   - Silkscreen grid for easy component placement
   - Power/ground rails along edges

5. **Optional IC Footprints**
   - MCP23017 (DIP-28 or SOIC-28)
   - ADS1115 (ADC, SOIC-10)
   - TJA1050 (CAN transceiver, SOIC-8)

6. **Headers/Connectors**
   - GPIO breakout headers
   - Screw terminals for external wiring
   - Button/LED connection points

## Physical Layout

```
+-------------------------------------------+
|  [ ]                               [ ]   | Mounting holes (M3/M4)
|                                           |
|  [Bus Connector]  [Power Section]        |
|                                           |
|  +-------------------------------------+  |
|  |                                     |  | Prototyping area
|  |      [Prototyping Grid]             |  | (plated through-holes)
|  |                                     |  |
|  +-------------------------------------+  |
|                                           |
|  [Optional IC Footprints]                 |
|                                           |
|  [ ]                               [ ]   |
+-------------------------------------------+
```

## Usage

### For Simple Modules (LEDs, Buttons)
1. Populate bus connector
2. Add current-limiting resistors in proto area
3. Wire LEDs/buttons via screw terminals or headers

### For Complex Modules (Displays, Sensors)
1. Populate bus connector
2. Mount IC (MCP23017, LCD driver, etc.) in optional footprint
3. Use proto area for supporting circuitry
4. Connect external components via headers

### For CAN-Based Modules
1. Populate bus connector
2. Add TJA1050 transceiver in optional footprint
3. Route CAN H/L to external connector or daughter board

## BOM (Template Populated)

| Qty | Reference | Part Number | Description | Notes |
|-----|-----------|-------------|-------------|-------|
| 1 | J1 | JST XH 6-pin | Bus connector | Or Molex KK254 |
| 2 | C1, C2 | Ceramic 100nF | Decoupling caps | Near power input |
| 2 | C3, C4 | Electrolytic 10µF | Bulk caps | 16V rating |
| 1 | D1 | 1N5817 | Reverse polarity protection | Optional |
| 2 | TP1, TP2 | Test point | Power rails | +V and GND |

**Additional components depend on module function**

## Customization Guide

### Clone Template for New Module

1. Copy `board_module_template.zip` → `<module_name>_rev1.zip`
2. Open in KiCAD
3. Modify prototyping area:
   - Add specific IC footprints
   - Route traces for module-specific signals
   - Add connectors for external components
4. Update silkscreen with module name and version
5. Run DRC (Design Rule Check)
6. Generate Gerber files for fabrication

### Layout Tips

- Keep power traces wide (10+ mil for >500mA loads)
- Place decoupling caps close to IC VDD pins
- Use ground plane on bottom layer (if 2-layer)
- Add test points for critical signals
- Include polarity markings for connectors

## Related Documentation

- **Module Specs**: [../modules/](../modules/) - Integration examples
- **Hardware Spec**: [../hardware-spec.md](../hardware-spec.md) - Bus pinout
- **Controller Board**: [controller.md](controller.md) - Bus master
- **I/O Expander**: [io-expander-board.md](io-expander-board.md) - Common IC used on modules
