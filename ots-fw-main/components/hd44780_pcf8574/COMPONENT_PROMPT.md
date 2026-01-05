# HD44780 LCD Driver Component (via PCF8574 I2C Backpack)

**Component**: `hd44780_pcf8574`  
**Location**: `ots-fw-main/components/hd44780_pcf8574/`  
**Status**: ✅ Active (Used by Troops and System Status Modules)

## Overview

ESP-IDF component for controlling HD44780-compatible 16×2 character LCD displays via PCF8574 I2C backpack. Provides a simple API for text display, cursor control, and backlight management.

## Hardware

**LCD Model**: 16×2 character LCD (HD44780 controller)  
**I2C Backpack**: PCF8574 I2C expander  
**Default Address**: 0x27 (or 0x3F depending on hardware)  
**Interface**: I2C (shared bus with other devices)  
**Voltage**: 5V (regulated on backpack)

## Features

- ✅ 16×2 character display (configurable via defines)
- ✅ Direct text output with `lcd_write_string()`
- ✅ Cursor positioning with `lcd_set_cursor()`
- ✅ Clear display and home commands
- ✅ Backlight control (on/off)
- ✅ 4-bit mode operation (hardware-efficient)
- ✅ I2C master bus integration (ESP-IDF v5+)

## API Reference

### Initialization

```c
#include "lcd_driver.h"

// Initialize LCD on I2C bus
esp_err_t lcd_init(i2c_master_bus_handle_t bus, uint8_t i2c_addr);

// Cleanup
void lcd_deinit(void);
```

### Display Control

```c
// Clear display and home cursor
void lcd_clear(void);

// Set cursor position (0-indexed: col 0-15, row 0-1)
void lcd_set_cursor(uint8_t col, uint8_t row);

// Write string at current cursor position
void lcd_write_string(const char *str);

// Write single character
void lcd_write_char(char c);
```

### Backlight Control

```c
// Turn backlight on/off
void lcd_backlight_on(void);
void lcd_backlight_off(void);
```

## Usage Example

```c
#include "lcd_driver.h"
#include "driver/i2c_master.h"

// I2C bus already initialized by main
extern i2c_master_bus_handle_t i2c_bus_handle;

void display_init(void) {
    // Initialize LCD at default address 0x27
    esp_err_t ret = lcd_init(i2c_bus_handle, LCD_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Turn on backlight
    lcd_backlight_on();
    
    // Display startup message
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("  OTS Firmware  ");
    lcd_set_cursor(0, 1);
    lcd_write_string("  Initializing  ");
}

void display_troops(uint32_t current, uint32_t max, uint8_t percent) {
    char line1[17], line2[17];
    
    // Format line 1: "120K / 1.1M" (right-aligned)
    snprintf(line1, sizeof(line1), "%luK / %luM", 
             current / 1000, max / 1000000);
    
    // Format line 2: "50% (60K)" (left-aligned)
    uint32_t calculated = (current * percent) / 100;
    snprintf(line2, sizeof(line2), "%d%% (%luK)", 
             percent, calculated / 1000);
    
    // Update display
    lcd_set_cursor(0, 0);
    lcd_write_string(line1);
    lcd_set_cursor(0, 1);
    lcd_write_string(line2);
}
```

## Configuration

**Component Configuration** (`idf_component.yml`):
```yaml
dependencies:
  idf:
    version: ">=5.0.0"
```

**I2C Address**:
```c
// Default address (defined in lcd_driver.h)
#define LCD_I2C_ADDR 0x27

// Alternative address (some backpacks use 0x3F)
lcd_init(i2c_bus_handle, 0x3F);
```

**Display Dimensions**:
```c
// Defined in lcd_driver.h
#define LCD_COLS 16
#define LCD_ROWS 2
```

## PCF8574 Pin Mapping

The PCF8574 I2C backpack uses 4-bit mode with the following pin mapping:

| PCF8574 Pin | HD44780 Pin | Function |
|-------------|-------------|----------|
| P0 | RS | Register Select |
| P1 | RW | Read/Write (tied to GND in write-only mode) |
| P2 | EN | Enable |
| P3 | Backlight | LED backlight control |
| P4-P7 | D4-D7 | Data lines (4-bit mode) |

## Integration Points

**Used By:**
- `troops_module.c` - Troop count and deployment percentage display
- `system_status_module.c` - System status and game state screens

**I2C Bus:**
- Shared with MCP23017 expanders (0x20, 0x21)
- Shared with ADS1015 ADC (0x48)
- Bus frequency: 100kHz (standard mode)

## Timing Considerations

**Initialization Time**: ~50ms (HD44780 startup + configuration)  
**Character Write**: ~1ms per character (PCF8574 I2C + HD44780 processing)  
**Full Screen Update**: ~32ms (16 chars × 2 rows)

**Best Practices:**
- Minimize full-screen updates (use targeted `lcd_set_cursor()` + `lcd_write_string()`)
- Buffer text before updating display
- Avoid updates faster than 100ms (LCD processing time)

## Troubleshooting

**No display / blank screen:**
- Check I2C address (try 0x27 and 0x3F)
- Verify I2C bus initialized before `lcd_init()`
- Check contrast potentiometer on LCD backpack
- Verify 5V power supply

**Garbled characters:**
- I2C timing issue (check bus speed)
- Insufficient delays in init sequence
- Check PCF8574 pin mapping

**Backlight not working:**
- Some backpacks have backlight on P3, others on different pin
- Try `lcd_backlight_on()` and check for hardware jumper

**I2C errors:**
```bash
# Scan I2C bus to find LCD address
i2cdetect -y 1  # Linux
# Or use firmware test: pio run -e test-i2c
```

## Testing

**Hardware Test:**
```bash
cd ots-fw-main
pio run -e test-lcd -t upload && pio device monitor
```

**Test Output:**
- Displays test pattern on LCD
- Shows character set and positioning
- Tests backlight control

**Python Diagnostics:**
```bash
cd ots-fw-main/tools/tests
python lcd_diag.py --port /dev/ttyUSB0
```

## Related Documentation

- **Usage Example**: `/ots-fw-main/prompts/TROOPS_MODULE_PROMPT.md` (lines 130-180)
- **Hardware Spec**: `/ots-hardware/DISPLAY_SCREENS_SPEC.md`
- **Display Modes**: System Status Module implements 7 screen types
- **Test Guide**: `/ots-fw-main/docs/TESTING.md` (LCD section)

## Implementation Notes

**Architecture:**
- Component follows ESP-IDF component structure
- Uses ESP-IDF v5+ I2C master driver API
- Pure C implementation (no external libraries)
- Stateless design (no internal state tracking)

**4-Bit Mode:**
- HD44780 operates in 4-bit mode to save PCF8574 pins
- Each byte sent as two 4-bit nibbles (high then low)
- EN pulse required between nibbles

**I2C Communication:**
- All commands sent via PCF8574 I2C writes
- No read operations (write-only mode)
- Backlight bit always set in data byte

---

**Last Updated**: January 5, 2026  
**Maintained By**: OTS Firmware Team
