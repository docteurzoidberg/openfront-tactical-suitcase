# Troops Module Firmware Implementation Prompt

## Context

You are implementing the **Troops Module** for the OTS (OpenFront Tactical Station) firmware. This module provides real-time troop count visualization and deployment percentage control for the OpenFront.io browser game.

## Module Overview

- **Size**: 16U (full-height module)
- **Display**: 2×16 character I2C LCD
- **Input**: Potentiometer slider via I2C ADC (ADS1115)
- **Protocol**: WebSocket JSON messages (see `/protocol-context.md`)

## Hardware Components

### LCD Display
- **Model**: I2C 1602A APKLVSR
  - 16×2 character LCD with integrated I2C backpack
  - Controller: HD44780 via PCF8574 I2C expander
  - Yellow-green LED backlight
- **I2C Address**: 0x27 (default) or 0x3F (if configured)
- **ESP-IDF Driver**: Use built-in `i2c.h` driver
  - Control PCF8574 I2C expander directly via I2C read/write
  - Implement HD44780 4-bit mode commands
  - Consider using ESP-IDF component: `esp_lcd_hd44780` or custom implementation
- **Connection**: Shared I2C bus (SDA/SCL from main controller)
- **Voltage**: 5V (regulated on I2C backpack)

### ADC (for slider)
- **Model**: ADS1115 (16-bit I2C ADC)
- **I2C Address**: 0x48
- **ESP-IDF Driver**: Use built-in `i2c.h` driver
  - Implement ADS1115 register read/write via I2C transactions
  - Configure single-ended input on AIN0 (channel 0)
  - Set gain to ±4.096V range (GAIN_ONE)
  - Use continuous conversion or single-shot mode
- **Connection**: Shared I2C bus
- **Input**: Potentiometer slider on channel A0

## Display Format

### Line 1: Troop Counts
```
120K / 1.1M
```
Format: `{current} / {max}` with intelligent unit scaling

### Line 2: Deployment Percentage
```
50% (60K)
```
Format: `{percent}% ({calculated})` where calculated = current × (percent/100)

### Unit Scaling Logic
```cpp
String formatTroops(uint32_t troops) {
  if (troops >= 1000000000) {
    return String(troops / 1000000000.0, 1) + "B";
  } else if (troops >= 1000000) {
    return String(troops / 1000000.0, 1) + "M";
  } else if (troops >= 1000) {
    return String(troops / 1000.0, 1) + "K";
  } else {
    return String(troops);
  }
}
```

## Protocol Integration

### Incoming: Troop State Updates

Listen for game state messages containing troop information:

```json
{
  "type": "state",
  "payload": {
    "timestamp": 1234567890,
    "troops": {
      "current": 120000,
      "max": 1100000
    }
  }
}
```

**Implementation**:
1. Parse JSON in WebSocket message handler
2. Extract `troops.current` and `troops.max`
3. Update module state variables
4. Refresh LCD Line 1 immediately
5. Recalculate and refresh LCD Line 2 (using current slider %)

### Outgoing: Slider Commands

When slider position changes by ≥1%, send command:

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

**Implementation**:
1. Poll ADC every 100ms
2. Convert ADC value (0-32767) to percentage (0-100)
3. Compare with `lastSentPercent`
4. If `abs(newPercent - lastSentPercent) >= 1`:
   - Send command via WebSocket
   - Update `lastSentPercent`
   - Refresh LCD Line 2

## Module State Structure

```cpp
struct TroopsModuleState {
  // From game state
  uint32_t currentTroops;
  uint32_t maxTroops;
  
  // From slider
  uint8_t sliderPercent;      // 0-100
  uint8_t lastSentPercent;    // For change detection
  
  // Timing
  uint32_t lastSliderRead;    // Debounce timestamp (millis)
  
  // Display cache (optional, for efficiency)
  String lastLine1;
  String lastLine2;
  bool displayDirty;
};
```

## Implementation Tasks

### 1. Module Initialization

```cpp
void troops_module_init() {
  // Initialize I2C bus (if not already initialized by main)
  // i2c_config_t conf = { ... };
  // i2c_param_config(I2C_NUM_0, &conf);
  // i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
  
  // Initialize LCD (HD44780 via PCF8574)
  lcd_init(0x27);  // Custom function to init HD44780 via I2C
  lcd_backlight(true);
  lcd_clear();
  
  // Initialize ADS1115 ADC
  ads1115_init(0x48);  // Custom function to configure ADS1115
  ads1115_set_gain(ADS1115_GAIN_ONE);  // ±4.096V range
  ads1115_set_mode(ADS1115_MODE_CONTINUOUS);  // Continuous conversion
  
  // Initialize state
  troopsState.currentTroops = 0;
  troopsState.maxTroops = 0;
  troopsState.sliderPercent = 0;
  troopsState.lastSentPercent = 0;
  troopsState.lastSliderRead = 0;
  troopsState.displayDirty = true;
  
  // Show startup message
  lcd_set_cursor(0, 0);
  lcd_write_string("  TROOPS MODULE ");
  lcd_set_cursor(0, 1);
  lcd_write_string("  Initializing  ");
  vTaskDelay(pdMS_TO_TICKS(1000));
  lcd_clear();
}
```

### 2. State Update Handler

```cpp
void troops_module_handle_state(const JsonObject& payload) {
  // Check if troops data exists
  if (!payload.containsKey("troops")) return;
  
  JsonObject troops = payload["troops"];
  
  // Update state
  troopsState.currentTroops = troops["current"];
  troopsState.maxTroops = troops["max"];
  troopsState.displayDirty = true;
  
  // Update display
  troops_module_update_display();
}
```

### 3. Slider Polling

```cpp
void troops_module_poll_slider() {
  uint32_t now = millis();
  
  // Debounce (100ms interval)
  if (now - troopsState.lastSliderRead < 100) return;
  troopsState.lastSliderRead = now;
  
  // Read ADC
  int16_t adc = ads1115_read_adc(ADS1115_CHANNEL_0);  // Channel A0
  if (adc < 0) adc = 0;
  
  // Map to percentage (0-100)
  uint8_t newPercent = map(adc, 0, 32767, 0, 100);
  newPercent = constrain(newPercent, 0, 100);
  
  // Update state
  troopsState.sliderPercent = newPercent;
  
  // Check for ≥1% change
  if (abs(newPercent - troopsState.lastSentPercent) >= 1) {
    troops_module_send_command(newPercent);
    troopsState.lastSentPercent = newPercent;
    troopsState.displayDirty = true;
  }
  
  // Update display if needed
  if (troopsState.displayDirty) {
    troops_module_update_display();
  }
}
```

### 4. Display Update

```cpp
void troops_module_update_display() {
  if (!troopsState.displayDirty) return;
  
  // Line 1: Current / Max
  String line1 = formatTroops(troopsState.currentTroops) + " / " + 
                 formatTroops(troopsState.maxTroops);
  
  // Center or right-align (pad to 16 chars)
  while (line1.length() < 16) {
    line1 = " " + line1;  // Right-align
  }
  
  lcd_set_cursor(0, 0);
  lcd_write_string(line1);
  
  // Line 2: Percent% (Calculated)
  uint32_t calculated = (troopsState.currentTroops * troopsState.sliderPercent) / 100;
  char line2[17];
  snprintf(line2, sizeof(line2), "%-16s", 
           (String(troopsState.sliderPercent) + "% (" + formatTroops(calculated) + ")").c_str());
  
  lcd_set_cursor(0, 1);
  lcd.print(line2);
  
  troopsState.displayDirty = false;
}
```

### 5. Command Sender

```cpp
void troops_module_send_command(uint8_t percent) {
  // Build JSON command
  StaticJsonDocument<256> doc;
  doc["type"] = "cmd";
  JsonObject payload = doc.createNestedObject("payload");
  payload["action"] = "set-troops-percent";
  JsonObject params = payload.createNestedObject("params");
  params["percent"] = percent;
  
  // Send via WebSocket
  String output;
  serializeJson(doc, output);
  ws_send(output.c_str());
  
  // Debug log
  Serial.printf("[Troops] Sent: set-troops-percent %d%%\n", percent);
}
```

### 6. Main Loop Integration

```cpp
void loop() {
  // ... existing code ...
  
  // Poll troops module slider
  troops_module_poll_slider();
  
  // ... rest of loop ...
}
```

### 7. WebSocket Message Router (add to existing handler)

```cpp
void ws_handle_message(const char* data) {
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, data);
  
  if (error) {
    Serial.println("[WS] JSON parse error");
    return;
  }
  
  const char* type = doc["type"];
  
  if (strcmp(type, "state") == 0) {
    troops_module_handle_state(doc["payload"]);
    // ... other module handlers ...
  }
  // ... other message types ...
}
```

## Testing Checklist

### I2C Bus Test
- [ ] I2C master driver initialized successfully
- [ ] LCD (PCF8574) detected at 0x27 (or 0x3F) via i2c_master_write
- [ ] ADC (ADS1115) detected at 0x48 via i2c_master_write
- [ ] No I2C bus errors in ESP-IDF logs (ESP_ERROR_CHECK)

### Display Test
- [ ] LCD backlight turns on
- [ ] Startup message appears
- [ ] Both lines display correctly
- [ ] Characters are readable

### Slider Test
- [ ] ADC reads ~0 at 0% position
- [ ] ADC reads ~32767 at 100% position
- [ ] Slider values map correctly to 0-100%
- [ ] Commands sent only on ≥1% change

### Protocol Test
- [ ] Receives state messages from server
- [ ] Parses troops.current and troops.max correctly
- [ ] Display updates when state received
- [ ] Sends set-troops-percent commands
- [ ] Commands have correct JSON format

### Scaling Test
- [ ] Low troops (< 1K): shows raw number
- [ ] Medium troops (< 1M): shows with "K"
- [ ] High troops (≥ 1M): shows with "M"
- [ ] Very high troops (≥ 1B): shows with "B"
- [ ] Decimal precision correct (1 decimal place)

### Integration Test
- [ ] Module works alongside other modules (Alert, Nuke, etc.)
- [ ] No interference on shared I2C bus
- [ ] WebSocket messages routed correctly
- [ ] Display updates in real-time during gameplay

## Code Organization

Place implementation in:
- **Source**: `/ots-fw-main/src/module_troops.c` (or .cpp)
- **Header**: `/ots-fw-main/include/module_troops.h`
- **Integration**: Update `/ots-fw-main/src/main.c` to call troops module functions

## Dependencies

**ESP-IDF Components**:
- Built-in `i2c.h` driver (no external library needed)
- `cJSON` for JSON parsing (built into ESP-IDF)
- Custom implementation required for:
  - HD44780 LCD control via PCF8574 I2C expander
  - ADS1115 ADC communication via I2C

**Optional ESP-IDF Components** (via `idf_component.yml`):
```yaml
dependencies:
  # Optional: LCD component if available
  # espressif/esp_lcd_hd44780: "^1.0.0"
  # Or implement custom HD44780 + PCF8574 driver
```

**Implementation Notes**:
1. Use ESP-IDF I2C master driver for all I2C communication
2. Implement PCF8574 bit-banging for HD44780 control
3. Implement ADS1115 register read/write functions
4. Use ESP-IDF FreeRTOS APIs (`vTaskDelay`, `xTaskGetTickCount`)

## ESP-IDF Implementation Guide

### I2C Driver Setup
```cpp
#include "driver/i2c.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO GPIO_NUM_21  // Adjust to your pin
#define I2C_MASTER_SCL_IO GPIO_NUM_22  // Adjust to your pin
#define I2C_MASTER_FREQ_HZ 100000      // 100kHz

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}
```

### PCF8574 LCD Helper Functions
```cpp
// PCF8574 pin mapping for HD44780
#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BACKLIGHT 0x08

void pcf8574_write(uint8_t addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}
```

### ADS1115 I2C Functions
```cpp
#define ADS1115_REG_CONFIG 0x01
#define ADS1115_REG_CONVERSION 0x00

int16_t ads1115_read_adc(uint8_t channel) {
    // Write config register
    uint16_t config = 0xC383 | (channel << 12);  // Single-shot, channel select
    // Implement I2C write + read sequence
    // Return 16-bit ADC value
}
```

## Key Considerations

1. **ESP-IDF Framework**: Use ESP-IDF APIs, not Arduino libraries
2. **I2C Master Driver**: Use `driver/i2c.h` for all I2C communication
3. **FreeRTOS**: Use `vTaskDelay()` instead of `delay()`, `xTaskGetTickCount()` instead of `millis()`
4. **Debouncing**: 100ms minimum between slider reads to avoid command spam
5. **Change Threshold**: Only send commands on ≥1% difference
6. **Display Updates**: Immediate (no animations or delays)
7. **Scaling**: Use 1 decimal place for K/M/B units (e.g., "1.2M" not "1234K")
8. **Padding**: Right-align Line 1, left-align Line 2 (or adjust for readability)
9. **Error Handling**: Handle missing troops data gracefully (show "---")
10. **I2C Conflicts**: Ensure no address collisions with other modules (0x27=LCD, 0x48=ADC)

## Example Serial Debug Output

```
[Troops] Module initialized
[Troops] LCD detected at 0x27
[Troops] ADC detected at 0x48
[Troops] State received: 120000 / 1100000 troops
[Troops] Display updated: 120K / 1.1M
[Troops] Slider: 50% -> sending command
[Troops] Sent: set-troops-percent 50%
[Troops] Display updated: 50% (60K)
```

## References

- Main protocol: `/protocol-context.md`
- Hardware spec: `/ots-hardware/modules/troops-module.md`
- Other module implementations: `/ots-fw-main/prompts/ALERT_MODULE_PROMPT.md`
