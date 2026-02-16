# I/O Expander Board (MCP23017)

GPIO expansion boards for connecting hardware modules to main controller.

## Overview

The I/O Expander Boards provide 16 additional GPIO pins per board via I2C-controlled MCP23017 chips. The OTS system uses two boards to interface with modules that need button inputs and LED outputs.

**Board 0** (I2C address 0x20):
- Main Power Module: LINK LED (Pin 0)
- Nuke Module: 3 buttons (Pins 1-3), 3 LEDs (Pins 8-10)

**Board 1** (I2C address 0x21):
- Alert Module: 6 LEDs (Pins 0-5 or configurable)

## Specifications

| Property | Value |
|----------|-------|
| **IC** | Microchip MCP23017-E/SP (DIP-28) or MCP23017-E/SS (SOIC-28) |
| **GPIO Pins** | 16 (Port A: 8 pins, Port B: 8 pins) |
| **I2C Address** | Configurable via A0/A1/A2 pins (0x20-0x27) |
| **Supply Voltage** | 1.8V to 5.5V (typically 3.3V or 5V) |
| **I/O Current** | 25mA per pin (sourcing/sinking) |
| **PCB Size** | TBD (fits standard module frame) |
| **Layers** | 2-layer FR4 |

## Pin Configuration

### MCP23017 I2C Address Selection

| A2 | A1 | A0 | I2C Address | Usage |
|---|---|---|-------------|-------|
| 0 | 0 | 0 | 0x20 | Board 0 (Main Power + Nuke) |
| 0 | 0 | 1 | 0x21 | Board 1 (Alert) |
| 0 | 1 | 0 | 0x22 | Reserved |
| 0 | 1 | 1 | 0x23 | Reserved |

### GPIO Port Allocation

**Board 0 (0x20):**
```
Port A (GPA0-GPA7):
  GPA0: Main Power - LINK LED
  GPA1: Nuke - ATOM button
  GPA2: Nuke - HYDRO button
  GPA3: Nuke - MIRV button
  GPA4-GPA7: Available

Port B (GPB0-GPB7):
  GPB0: Nuke - ATOM LED
  GPB1: Nuke - HYDRO LED
  GPB2: Nuke - MIRV LED
  GPB3-GPB7: Available
```

**Board 1 (0x21):**
```
Port A (GPA0-GPA7):
  GPA0: Alert - WARNING LED
  GPA1: Alert - ATTACKED LED
  GPA2: Alert - ATOM LED
  GPA3: Alert - HYDRO LED
  GPA4: Alert - MIRV LED
  GPA5: Alert - LAND LED
  GPA6-GPA7: Available

Port B (GPB0-GPB7):
  GPB0-GPB7: Available
```

## Schematic

### Core Circuit

```
                   +3.3V/5V
                      |
                     [R1] 10kΩ (RESET pull-up)
                      |
    [ESP32-S3]------[I2C Bus]------[MCP23017]
      SDA -----------------------> SDA (Pin 13)
      SCL -----------------------> SCL (Pin 12)
                                    RESET (Pin 18) --+-- [C1] 100nF -- GND
                                    A0 (Pin 15) -----+-- [Address Config]
                                    A1 (Pin 16) -----+
                                    A2 (Pin 17) -----+
                                    INT A/B ------------ (Optional interrupt)
                                    VDD (Pin 9) --------- +3.3V/5V
                                    VSS (Pin 10) -------- GND
                                    
    GPIO Ports:
      GPA0-GPA7 (Pins 21-28, 1-7)
      GPB0-GPB7 (Pins 8, 10-17)
```

### Pull-up/Pull-down Configuration

- **Inputs (buttons)**: Internal 100kΩ pull-ups enabled via firmware (GPPU register)
- **Outputs (LEDs)**: No pull resistors, driven high/low
- **I2C Bus**: 4.7kΩ pull-ups on main controller board (SDA/SCL)

### LED Connection Pattern

```
MCP23017 Pin --> [Current Limiting Resistor] --> [LED Anode] --> [LED Cathode] --> GND

Typical: 
- 3.3V logic: 220Ω-330Ω resistor for 10-15mA LED current
- 5V logic: 470Ω-1kΩ resistor
```

### Button Connection Pattern

```
MCP23017 Pin --> [Button] --> GND
               (100kΩ internal pull-up enabled)

Button pressed: Pin reads LOW (0)
Button released: Pin reads HIGH (1) via pull-up
```

## Power Design

### Power Tree

```
+12V Input (from main power)
   |
   +--[Buck Converter]---> +5V (if using 5V logic)
         |
         +--[LDO Regulator]---> +3.3V (for MCP23017 + ESP32-S3)
```

**Or simplified:**

```
+5V Input (from main power/USB)
   |
   +--[LDO Regulator]---> +3.3V
```

### Current Budget

| Component | Current | Notes |
|-----------|---------|-------|
| MCP23017 (idle) | ~1mA | Typical operating current |
| LEDs (6-10 total) | 10-15mA each | 60-150mA total when all on |
| **Total per board** | ~160mA max | Conservative estimate |

### Decoupling

- **C1**: 100nF ceramic capacitor near MCP23017 VDD pin
- **C2**: 10µF electrolytic capacitor at board power input

## Interfaces

### I2C Bus Connector

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | +3.3V/5V | Power supply |
| 2 | GND | Ground reference |
| 3 | SDA | I2C Data |
| 4 | SCL | I2C Clock |

Connector: 4-pin JST XH or Molex KK254 (TBD)

### GPIO Headers

- **Option A**: Dual-row pin headers (2×8) for each port
- **Option B**: Screw terminals for module connections
- **Option C**: Direct wire solder pads

## BOM (Bill of Materials)

### Key Components

| Qty | Reference | Part Number | Description | Notes |
|-----|-----------|-------------|-------------|-------|
| 1 | U1 | MCP23017-E/SP | I/O Expander IC | DIP-28 or SOIC-28 |
| 1 | C1 | Ceramic 100nF | Decoupling capacitor | X7R, 0805 or through-hole |
| 1 | C2 | Electrolytic 10µF | Bulk capacitor | 16V rating |
| 1 | R1 | 10kΩ | RESET pull-up | 1/4W through-hole or 0805 |
| 3 | R2-R4 | 10kΩ | Address config (if jumpers) | Optional |
| 1-16 | R5-R20 | 220Ω-1kΩ | LED current limiters | Depends on LED count |
| 1 | J1 | 4-pin connector | I2C bus connection | JST XH or Molex |
| 1-2 | Headers | Pin headers | GPIO connections | Dual-row 2×8 |

**Total cost per board:** ~$5-10 USD (components only, excluding PCB)

## Fabrication Notes

### PCB Specifications
- **Layers**: 2-layer
- **Material**: FR4
- **Thickness**: 1.6mm
- **Copper weight**: 1oz (35µm)
- **Surface finish**: HASL or ENIG
- **Silkscreen**: Both sides recommended

### Manufacturing
- Compatible with JLCPCB/PCBWay standard specs
- No special requirements (no impedance control, no blind vias)
- Standard 6/6 mil trace/space

### Assembly
- Through-hole or SMD versions available
- Through-hole easier for prototyping/rework
- SMD more compact for production

## Testing & Bring-Up

### Test Points

Provide test points for:
- **TP1**: VDD (+3.3V/5V)
- **TP2**: GND
- **TP3**: SDA
- **TP4**: SCL
- **TP5-TP20**: GPIO pins (optional, depends on layout)

### Bring-Up Procedure

1. **Power Test**: Verify VDD voltage (3.3V or 5V)
2. **I2C Detection**: Run `i2cdetect` from ESP32 to find device at 0x20/0x21
3. **Register Test**: Write/read IODIRA and IODIRB registers to verify communication
4. **GPIO Test**: 
   - Set all pins as outputs
   - Toggle each pin high/low, verify with multimeter
   - Test LED outputs with actual LEDs
   - Test button inputs with jumper wires to GND

### Firmware Testing

```cpp
// Pseudo-code for testing Board 0
#include "module_io.h"

// Test LINK LED (Board 0, Pin 0)
module_io_set_pin_mode(0, 0, PIN_OUTPUT);
module_io_digital_write(0, 0, HIGH);  // LED ON
delay(500);
module_io_digital_write(0, 0, LOW);   // LED OFF

// Test ATOM button (Board 0, Pin 1)
module_io_set_pin_mode(0, 1, PIN_INPUT_PULLUP);
int button_state = module_io_digital_read(0, 1);
// button_state = 0 when pressed, 1 when released
```

## Known Issues / Errata

### Rev 1
- None reported yet

## Future Improvements

- [ ] Add interrupt pins (INT A/B) connection to ESP32 for faster response
- [ ] Consider level shifters if mixing 3.3V/5V logic
- [ ] Add ESD protection on GPIO pins exposed to panel connectors
- [ ] Optimize PCB layout for shorter traces to modules

## Related Documentation

- **Main Controller**: [controller.md](controller.md) - I2C bus master
- **Module Specs**: 
  - [Main Power Module](../modules/main-power-module.md) - Uses Board 0
  - [Nuke Module](../modules/nuke-module.md) - Uses Board 0
  - [Alert Module](../modules/alert-module.md) - Uses Board 1
- **Firmware**: `/ots-fw-main/include/module_io.h` - I/O abstraction layer
- **Datasheet**: [MCP23017 Datasheet](https://www.microchip.com/en-us/product/MCP23017) (Microchip)
