# Troops Module Firmware Implementation Prompt

## Context

You are implementing the **Troops Module** for the OTS (OpenFront Tactical Station) firmware. This module provides real-time troop count visualization and deployment percentage control for the OpenFront.io browser game.

## Module Overview

- **Size**: 16U (full-height module)
- **Display**: 2×16 character I2C LCD (HD44780 via PCF8574 backpack)
- **Input**: Potentiometer slider via I2C ADC (**ADS1015** 12-bit)
- **I2C Bus**: Shared with MCP23017 expanders (GPIO8 SDA, GPIO9 SCL)
- **Protocol**: WebSocket JSON messages (see `/prompts/protocol-context.md`)

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
- **Model**: ADS1015 (12-bit I2C ADC)
- **I2C Address**: 0x48 (default)
- **ESP-IDF Driver**: Native I2C master driver (`driver/i2c.h`)
  - Implement ADS1015 register read/write via I2C transactions
  - Configure single-ended input on AIN0 (channel 0)
  - Set gain to ±4.096V range (PGA setting)
  - Single-shot conversion mode (1600 SPS)
  - 12-bit resolution: 0-4095 range
- **Connection**: Shared I2C bus with MCP23017 expanders
- **Input**: Potentiometer slider on channel AIN0

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
```c
void troops_format_count(uint32_t troops, char* buffer, size_t buffer_size) {
    if (troops >= 1000000000) {
        snprintf(buffer, buffer_size, "%.1fB", troops / 1000000000.0);
    } else if (troops >= 1000000) {
        snprintf(buffer, buffer_size, "%.1fM", troops / 1000000.0);
    } else if (troops >= 1000) {
        snprintf(buffer, buffer_size, "%.1fK", troops / 1000.0);
    } else {
        snprintf(buffer, buffer_size, "%lu", (unsigned long)troops);
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
2. Convert ADC value (0-4095 for 12-bit ADS1015) to percentage (0-100)
3. Compare with `last_sent_percent`
4. If `abs(new_percent - last_sent_percent) >= 1`:
   - Send command via WebSocket using cJSON
   - Update `last_sent_percent`
   - Set `display_dirty = true`

## Module State Structure

```c
typedef struct {
    uint32_t current_troops;        // Current troop count from server
    uint32_t max_troops;            // Maximum troop count from server
    uint8_t slider_percent;         // Current slider position (0-100)
    uint8_t last_sent_percent;      // Last percent value sent to server
    uint64_t last_slider_read;      // Timestamp of last slider read (ms)
    bool display_dirty;             // LCD needs update
    bool initialized;               // Module initialization complete
} troops_module_state_t;
```

## Implementation Tasks

### 1. Module Initialization

Implements `hardware_module_t` interface:

```c
static esp_err_t troops_init(void) {
    ESP_LOGI(TAG, "Initializing troops module...");
    
    // I2C bus already initialized by main (shared with MCP23017)
    
    // Initialize ADS1015 ADC at 0x48
    esp_err_t ret = ads1015_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADS1015");
        return ret;
    }
    
    // Initialize LCD at 0x27 (HD44780 via PCF8574)
    ret = lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD");
        return ret;
    }
    
    // Show startup message
    lcd_set_cursor(0, 0);
    lcd_write_string(" TROOPS MODULE  ");
    lcd_set_cursor(0, 1);
    lcd_write_string(" Initializing.. ");
    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd_clear();
    
    module_state.initialized = true;
    module_state.display_dirty = true;
    
    return ESP_OK;
}
```

### 2. Event Handler (hardware_module_t interface)

```c
static void troops_handle_event(game_event_type_t event_type, const char* event_data_json) {
    if (!module_state.initialized) return;
    
    // Parse event data for troop updates using cJSON
    if (event_data_json && strlen(event_data_json) > 0) {
        cJSON* root = cJSON_Parse(event_data_json);
        if (root) {
            // Check for troops data in state message
            cJSON* troops = cJSON_GetObjectItem(root, "troops");
            if (troops) {
                cJSON* current = cJSON_GetObjectItem(troops, "current");
                cJSON* max = cJSON_GetObjectItem(troops, "max");
                
                if (current && cJSON_IsNumber(current)) {
                    module_state.current_troops = (uint32_t)current->valueint;
                }
                if (max && cJSON_IsNumber(max)) {
                    module_state.max_troops = (uint32_t)max->valueint;
                }
                
                module_state.display_dirty = true;
            }
            cJSON_Delete(root);
        }
    }
}
```

### 3. Slider Polling

```c
static void poll_slider(void) {
    uint64_t now = esp_timer_get_time() / 1000;  // Convert to ms
    
    // Debounce: only read every 100ms
    if (now - module_state.last_slider_read < TROOPS_SLIDER_POLL_MS) {
        return;
    }
    module_state.last_slider_read = now;
    
    // Read ADC
    int16_t adc_value = ads1015_read_adc(ADS1015_CHANNEL_AIN0);
    if (adc_value < 0) {
        ESP_LOGW(TAG, "Failed to read ADC");
        return;
    }
    
    // Map to percentage (ADS1015 is 12-bit: 0-4095)
    uint8_t new_percent = (adc_value * 100) / 4095;
    if (new_percent > 100) new_percent = 100;
    
    // Update state
    module_state.slider_percent = new_percent;
    
    // Check for ≥1% change
    int diff = abs((int)new_percent - (int)module_state.last_sent_percent);
    if (diff >= TROOPS_CHANGE_THRESHOLD) {
        send_percent_command(new_percent);
        module_state.last_sent_percent = new_percent;
        module_state.display_dirty = true;
    }
}
```

### 4. Display Update

```c
static void update_display(void) {
    if (!module_state.display_dirty) return;
    
    char line1[LCD_COLS + 1];
    char line2[LCD_COLS + 1];
    char current_str[8], max_str[8], calc_str[8];
    
    // Format troop counts
    troops_format_count(module_state.current_troops, current_str, sizeof(current_str));
    troops_format_count(module_state.max_troops, max_str, sizeof(max_str));
    
    // Line 1: "120K / 1.1M" (right-aligned)
    snprintf(line1, sizeof(line1), "%s / %s", current_str, max_str);
    int padding = LCD_COLS - strlen(line1);
    if (padding > 0) {
        memmove(line1 + padding, line1, strlen(line1) + 1);
        memset(line1, ' ', padding);
    }
    
    // Line 2: "50% (60K)" (left-aligned)
    uint32_t calculated = ((uint64_t)module_state.current_troops * module_state.slider_percent) / 100;
    troops_format_count(calculated, calc_str, sizeof(calc_str));
    snprintf(line2, sizeof(line2), "%d%% (%s)", module_state.slider_percent, calc_str);
    
    // Write to LCD
    lcd_set_cursor(0, 0);
    lcd_write_string(line1);
    lcd_set_cursor(0, 1);
    lcd_write_string(line2);
    
    module_state.display_dirty = false;
}
```

### 5. Command Sender

```c
static void send_percent_command(uint8_t percent) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "cmd");
    
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "action", "set-troops-percent");
    
    cJSON* params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "percent", percent);
    
    cJSON_AddItemToObject(payload, "params", params);
    cJSON_AddItemToObject(root, "payload", payload);
    
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
      ws_server_send_text(json_str, strlen(json_str));
      ESP_LOGI(TAG, "Sent troops percent: %d%%", percent);
      free(json_str);
    }
    
    cJSON_Delete(root);
}
```

### 6. Module Update (hardware_module_t interface)

```c
static void troops_update(void) {
    if (!module_state.initialized) return;
    
    // Poll slider for changes
    poll_slider();
    
    // Update display if needed
    if (module_state.display_dirty) {
        update_display();
    }
}
```

### 7. Module Registration

In `main.c`:
```c
#include "troops_module.h"

void app_main(void) {
    // ... existing initialization ...
    
    // Register hardware modules
    module_manager_register(troops_module_get());
  
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
- [ ] ADC (ADS1015) detected at 0x48 via i2c_master_write
- [ ] No I2C bus errors in ESP-IDF logs (ESP_ERROR_CHECK)

### Display Test
- [ ] LCD backlight turns on
- [ ] Startup message appears
- [ ] Both lines display correctly
- [ ] Characters are readable

### Slider Test
- [ ] ADC reads ~0 at 0% position
- [ ] ADC reads ~4095 at 100% position (12-bit ADS1015)
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
3. Implement ADS1015 register read/write functions
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

### ADS1015 I2C Functions
```c
#define ADS1015_REG_CONFIG 0x01
#define ADS1015_REG_CONVERSION 0x00

int16_t ads1015_read_adc(uint8_t channel) {
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

- Main protocol: `/prompts/protocol-context.md`
- Hardware spec: `/ots-hardware/modules/troops-module.md`
- Other module implementations: `/ots-fw-main/prompts/ALERT_MODULE_PROMPT.md`
