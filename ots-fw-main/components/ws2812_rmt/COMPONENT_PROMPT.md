# WS2812 RGB LED Driver Component (via RMT)

**Component**: `ws2812_rmt`  
**Location**: `ots-fw-main/components/ws2812_rmt/`  
**Status**: ✅ Active (Used by main firmware for RGB LED status indicators)

## Overview

ESP-IDF component for controlling WS2812/WS2812B/SK6812 RGB LED strips using the ESP32 RMT (Remote Control) peripheral. Provides precise timing for WS2812 protocol without CPU overhead.

## Hardware

**LED Type**: WS2812B / WS2812 / SK6812 (individually addressable RGB LEDs)  
**Interface**: Single-wire serial (timing-critical)  
**Voltage**: 5V data logic (3.3V compatible with level shifter)  
**Protocol**: 800kHz data rate, 24-bit color (8-bit RGB)  
**Driver**: ESP32 RMT peripheral (hardware-based timing)

## Features

- ✅ Hardware-based timing via RMT peripheral (no CPU blocking)
- ✅ Supports any number of LEDs (limited by memory)
- ✅ 24-bit RGB color (8 bits per channel)
- ✅ Simple API: `ws2812_set_pixel()`, `ws2812_refresh()`
- ✅ Efficient batch updates (update buffer, then refresh once)
- ✅ Automatic timing calculation (no manual bit-banging)

## API Reference

### Initialization

```c
#include "ws2812_rmt.h"

// Configuration
ws2812_config_t config = {
    .gpio_num = GPIO_NUM_13,         // GPIO pin for data line
    .led_count = 8,                  // Number of LEDs in strip
    .resolution_hz = 10000000,       // RMT resolution (10MHz default)
};

// Initialize driver
esp_err_t ret = ws2812_init(&config);

// Cleanup
void ws2812_deinit(void);
```

### LED Control

```c
// Set single pixel color (0-indexed)
void ws2812_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

// Set pixel using color struct
ws2812_color_t color = {.r = 255, .g = 0, .b = 0};  // Red
ws2812_set_pixel_color(uint32_t index, ws2812_color_t color);

// Clear all LEDs (set to black)
void ws2812_clear(void);

// Update LED strip (send buffer to hardware)
esp_err_t ws2812_refresh(void);
```

### Color Helpers

```c
// Create color from RGB values
ws2812_color_t ws2812_rgb(uint8_t r, uint8_t g, uint8_t b);

// Create color from HSV (hue, saturation, value)
ws2812_color_t ws2812_hsv(uint16_t h, uint8_t s, uint8_t v);

// Dim color by factor (0-255)
ws2812_color_t ws2812_dim(ws2812_color_t color, uint8_t brightness);
```

## Usage Example

```c
#include "ws2812_rmt.h"

static ws2812_config_t led_config = {
    .gpio_num = GPIO_NUM_13,
    .led_count = 8,
    .resolution_hz = 10000000,
};

void rgb_led_init(void) {
    esp_err_t ret = ws2812_init(&led_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Set all LEDs to blue on startup
    for (int i = 0; i < 8; i++) {
        ws2812_set_pixel(i, 0, 0, 255);
    }
    ws2812_refresh();
}

void show_connection_status(bool connected) {
    if (connected) {
        // Green: connected
        ws2812_set_pixel(0, 0, 255, 0);
    } else {
        // Red: disconnected
        ws2812_set_pixel(0, 255, 0, 0);
    }
    ws2812_refresh();
}

void rainbow_effect(void) {
    for (int i = 0; i < 8; i++) {
        uint16_t hue = (i * 360) / 8;  // Spread across color wheel
        ws2812_color_t color = ws2812_hsv(hue, 255, 128);
        ws2812_set_pixel_color(i, color);
    }
    ws2812_refresh();
}
```

## Configuration

**Component Configuration** (`idf_component.yml`):
```yaml
dependencies:
  idf:
    version: ">=5.0.0"
```

**RMT Channel**:
- Automatically allocated by driver
- Uses RMT peripheral (not GPIO bit-banging)
- No timer conflicts with other RMT users

**LED Count**:
```c
// Maximum LEDs limited by available RAM
// Each LED uses 3 bytes in buffer + RMT items
#define MAX_LEDS 100  // Safe limit for most applications
```

## WS2812 Protocol Timing

**Bit Encoding** (800kHz):
- **0-bit**: 0.4μs HIGH, 0.85μs LOW (total: 1.25μs)
- **1-bit**: 0.8μs HIGH, 0.45μs LOW (total: 1.25μs)
- **Reset**: >50μs LOW (latch data)

**Color Order**: GRB (Green, Red, Blue) - 24 bits per LED  
**Refresh Rate**: ~400Hz for 8 LEDs, ~30Hz for 100 LEDs

## Hardware Setup

**Wiring:**
```
ESP32 GPIO13 ──[330Ω]──> WS2812 DIN
ESP32 GND ─────────────> WS2812 GND
5V Supply ─────────────> WS2812 VCC
```

**Level Shifter (Recommended):**
- ESP32 outputs 3.3V, WS2812 expects 5V logic
- Use 74AHCT125 or similar 3.3V→5V level shifter
- Or use WS2812 with 3.3V-tolerant DIN (works but not guaranteed)

**Power Considerations:**
- Each LED draws ~60mA at full white
- 8 LEDs = 480mA peak (use separate 5V supply)
- Add 1000μF capacitor near LED strip power input

## Future Use Cases

**Planned Integration:**
- Status indicators (connection, alerts, game state)
- Alert effects (nuke incoming = red pulse)
- Victory/defeat animations
- Boot sequence (rainbow chase)
- Error states (rapid red flash)

**Module Ideas:**
- RGB status bar module (8 LEDs)
- Under-lighting for suitcase (24 LEDs)
- Per-button LED feedback (NeoPixel rings)

## RMT Peripheral Details

**Why RMT?**
- Hardware-timed output (microsecond precision)
- No CPU blocking or timing loops
- Works while CPU is busy with other tasks
- Multiple channels available (up to 8 on ESP32-S3)

**RMT Configuration:**
```c
// Automatically configured by driver
rmt_config_t config = {
    .rmt_mode = RMT_MODE_TX,
    .channel = RMT_CHANNEL_0,  // Auto-allocated
    .gpio_num = gpio_num,
    .mem_block_num = 1,
    .tx_config = {
        .carrier_en = false,
        .loop_en = false,
        .idle_output_en = true,
        .idle_level = RMT_IDLE_LEVEL_LOW,
    },
    .clk_div = 8,  // 80MHz / 8 = 10MHz (0.1μs resolution)
};
```

## Performance

**Memory Usage:**
- 3 bytes per LED (RGB buffer)
- ~100 bytes RMT items (hardware buffer)
- 8 LEDs = ~124 bytes total

**Timing:**
- Full refresh: ~(num_leds × 30μs) + 50μs
- 8 LEDs: ~290μs per refresh
- Can update at ~3000 Hz (far exceeding human perception)

## Troubleshooting

**LEDs not lighting:**
- Check 5V power supply (adequate current)
- Verify GPIO pin number
- Check data line connection (DIN to GPIO)
- Try level shifter if using 3.3V logic

**Wrong colors:**
- Some LED strips use RGB order instead of GRB
- Check datasheet for your specific LED model
- Modify driver if needed (swap R/G in encoding)

**Flickering:**
- Insufficient power supply
- Poor ground connection
- Electromagnetic interference (keep wires short)

**First LED works, rest don't:**
- First LED damaged (acts as buffer)
- Replace first LED or skip it in software

## Testing

**Hardware Test:**
```bash
cd ots-fw-main
# Create test environment if not exists
pio run -e test-rgb-led -t upload && pio device monitor
```

**Expected Output:**
- Rainbow animation on LED strip
- Serial log showing initialization
- Color cycling every 100ms

## Related Documentation

- **ESP32 RMT**: [ESP-IDF RMT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html)
- **WS2812 Datasheet**: [Adafruit WS2812 Guide](https://cdn-shop.adafruit.com/datasheets/WS2812.pdf)
- **Hardware Spec**: Future lighting module in `/ots-hardware/modules/`

## Implementation Notes

**Architecture:**
- Component follows ESP-IDF component structure
- Uses ESP-IDF v5+ RMT driver API
- Pure C implementation
- Stateful design (maintains LED buffer internally)

**Timing Precision:**
- RMT peripheral handles all timing
- No `vTaskDelay()` or busy-waiting
- Works reliably even with WiFi/Bluetooth active

**Thread Safety:**
- Not thread-safe by design (single writer expected)
- Use mutex if calling from multiple tasks

---

**Last Updated**: January 5, 2026  
**Status**: Component ready, awaiting hardware integration  
**Maintained By**: OTS Firmware Team
