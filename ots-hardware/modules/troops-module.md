# Troops Module (16U)

## Overview

The Troops Module provides real-time visualization of available troops and control over deployment percentages. It features a 2x16 character LCD display showing current troop counts and a potentiometer slider for setting the deployment percentage, which directly controls the in-game troop slider.

## Module Specifications

- **Size**: 16U (full-height, 160mm × 25.4mm front panel)
- **Module Type**: Input/Output (Display + Control)
- **I2C Bus**: Shared main bus via 2×05 IDC header
- **Power**: 5V from main bus

## Hardware Components

### Display
- **LCD**: I2C 1602A APKLVSR (2×16 character I2C LCD module)
  - Model: 1602A with APKLVSR I2C backpack
  - I2C Address: 0x27 (default) or 0x3F (configurable via backpack jumpers)
  - Controller: HD44780 via PCF8574 I2C expander
  - Display: 16 characters × 2 lines
  - Backlight: Yellow-green LED backlight
  - Character size: 5×8 dots
  - Operating voltage: 5V (via I2C backpack regulator)
  - Interface: I2C (SDA/SCL only, no parallel pins needed)

### Control Input
- **Potentiometer Slider**: Linear 10kΩ potentiometer
  - Connected via I2C ADC chip
  - ADC: ADS1015 or ADS1115 (12-bit or 16-bit)
  - I2C Address: 0x48 (default)
  - Range: 0-100% (mapped from ADC full scale)
  - Resolution: 1% minimum change threshold

### Connectors
- **Main Bus**: 2×05 pin IDC header (I2C + Power)
  - Pin layout as per main power module specification
  - I2C SDA/SCL shared bus
  - 12V and GND

## Display Format

### Line 1: Troop Availability
```
120K / 1.1M
```
- Format: `{current} / {max}`
- Uses intelligent unit scaling:
  - < 1,000: show raw number (e.g., `500`)
  - < 1,000,000: show in K (e.g., `120K`)
  - ≥ 1,000,000: show in M (e.g., `1.1M`)
  - ≥ 1,000,000,000: show in B (e.g., `2.3B`)
- Right-aligned or centered for better readability

### Line 2: Deployment Percentage
```
50% (60K)
```
- Format: `{percentage}% ({calculated})`
- Percentage: 0-100%, derived from slider position
- Calculated amount: percentage of current troops, same scaling as Line 1
- Updates in real-time as slider moves

### Display Update Behavior
- Updates immediately when game state received (troops data)
- Updates immediately when slider position changes (≥1% difference)
- No animations or transitions (instant update)
- Display persists even when not in game (shows last known values or "---")

### Example States

**Normal operation**:
```
120K / 1.1M
50% (60K)
```

**Low troops**:
```
500 / 1.1M
25% (125)
```

**High troops**:
```
2.3M / 5.8M
100% (2.3M)
```

**No game connection** (optional):
```
--- / ---
--% (---)
```

## I2C Bus Configuration

### Shared Bus (Main 2×05 IDC Header)
- **SDA**: Pin 3
- **SCL**: Pin 5
- **12V**: Pins 2, 4
- **GND**: Pins 1, 6, 9

### I2C Addresses
- **LCD**: 0x27 (or 0x3F if conflict)
- **ADC**: 0x48 (ADS1115 default)

Both devices share the same I2C bus from the main controller.

## Protocol Behavior

### Incoming: Game State Updates

The module receives frequent game state updates containing troop information:

```json
{
  "type": "state",
  "payload": {
    "timestamp": 1234567890,
    "troops": {
      "current": 120000,
      "max": 1100000
    },
    // ... other state fields
  }
}
```

**Display Update**:
1. Parse `troops.current` and `troops.max`
2. Apply intelligent scaling (K/M/B)
3. Update Line 1 immediately
4. Recalculate Line 2 based on current slider percentage

### Outgoing: Slider Position Commands

When the slider position changes by ≥1%, send command:

```json
{
  "type": "cmd",
  "payload": {
    "action": "set-troops-percent",
    "params": {
      "percent": 50
    }
  }
}
```

**Slider Behavior**:
1. Read ADC value (0-4095 for ADS1015, 0-32767 for ADS1115)
2. Map to percentage (0-100)
3. Compare with last sent percentage
4. If difference ≥1%, send command
5. Update Line 2 display with new percentage and calculated amount

**Debouncing**: 100ms debounce on slider readings to avoid flooding commands during rapid movement.

## Firmware Requirements

### Libraries Needed
- **LiquidCrystal_I2C**: For LCD control
- **Adafruit_ADS1X15**: For ADC reading (or Wire library for custom implementation)

### State Variables
```cpp
struct TroopsModuleState {
  uint32_t currentTroops;
  uint32_t maxTroops;
  uint8_t sliderPercent;      // 0-100
  uint8_t lastSentPercent;    // For 1% change detection
  uint32_t lastSliderRead;    // Timestamp for debouncing
};
```

### Update Frequency
- **Display update**: Immediate on state change
- **Slider polling**: Every 100ms (debounced)
- **Command throttling**: Only on ≥1% change

### Display Helper Functions
```cpp
// Convert number to scaled string (e.g., "1.2M", "500K", "123")
String formatTroops(uint32_t troops);

// Update LCD line 1 with troop counts
void updateTroopsDisplay(uint32_t current, uint32_t max);

// Update LCD line 2 with percentage and calculated amount
void updatePercentDisplay(uint8_t percent, uint32_t current);

// Read slider and return percentage (0-100)
uint8_t readSliderPercent();
```

## Physical Layout

```
┌────────────────────┐
│   TROOPS MODULE    │
│                    │
│  ┌──────────────┐  │
│  │  120K / 1.1M │  │ ← LCD Line 1
│  │  50% (60K)   │  │ ← LCD Line 2
│  └──────────────┘  │
│                    │
│  ────────█────────  │ ← Slider (50%)
│  0%            100% │
│                    │
└────────────────────┘
```

## Power Budget

- LCD with backlight: ~20mA @ 5V (via I2C backpack regulator)
- ADC (ADS1115): ~150µA @ 5V
- **Total**: ~25mA @ 5V (negligible from 12V bus)

## Mounting

- Standard 16U front panel mounting
- LCD centered in upper half
- Slider centered in lower half
- Recommended vertical spacing: 40mm between LCD and slider

## PCB Requirements

### Main Components
1. LCD I2C module (pre-assembled backpack)
2. ADS1115 breakout board or DIP chip
3. 10kΩ linear potentiometer (slide type, 60mm travel recommended)
4. 2×05 IDC header (main bus connection)
5. Optional: 100nF ceramic capacitors for I2C noise filtering

### PCB Layout Notes
- Keep I2C traces short and parallel
- Add pull-up resistors if not present on LCD/ADC modules (4.7kΩ typical)
- Route 12V and GND with adequate trace width (20+ mil)
- Consider adding a 5V regulator if LCD backlight needs more current

## Testing Procedure

1. **I2C Detection**: Verify both devices (0x27, 0x48) appear on bus
2. **LCD Test**: Write test pattern to both lines
3. **ADC Test**: Read slider at 0%, 50%, 100% positions
4. **Command Test**: Move slider, verify commands sent over WebSocket
5. **State Test**: Send mock troop data, verify display updates correctly
6. **Scaling Test**: Test with various troop counts (hundreds, K, M, B ranges)

## Notes

- This module is passive (output-only for display, input-only for slider control)
- No buttons or additional LEDs required
- Display updates should be instantaneous (no animations)
- Slider sends commands to userscript, which controls the in-game troop slider
- Module remains functional even when game is not active (shows last known state)
