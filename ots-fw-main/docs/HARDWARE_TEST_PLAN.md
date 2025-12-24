# OTS Firmware Hardware Test Plan

## Overview

This document provides a structured testing approach for the OTS hardware modules. Each test can be run independently to isolate potential issues and verify hardware functionality without interference from other boards.

## Test Environment Setup

### Prerequisites
- ESP32-S3 dev board with firmware flashed
- USB serial connection for monitoring
- 12V power supply connected
- Each board to be tested connected to I2C bus (SDA=GPIO8, SCL=GPIO9)

### I2C Address Map
| Device | Address | Board |
|--------|---------|-------|
| MCP23017 Input Board | 0x20 | Addon Board #1 |
| MCP23017 Output Board | 0x21 | Addon Board #2 |
| LCD (PCF8574) | 0x27 | Troops Module |
| ADS1015 ADC | 0x48 | Troops Module |

## Test Sequence

Run tests in this order to build confidence incrementally:
1. I2C Bus Scan (no boards connected)
2. Output Board Test
3. Input Board Test
4. Troops Module Test
5. Integration Test (all boards)

---

## Test 1: I2C Bus Scan

**Purpose**: Verify I2C bus functionality and detect all connected devices.

### Hardware Required
- ESP32-S3 only (no addon boards)

### Test Firmware

Create `test_i2c_scan.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

static const char *TAG = "I2C_SCAN";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

void i2c_scan(void) {
    ESP_LOGI(TAG, "Starting I2C scan...");
    int devices_found = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address: 0x%02X", addr);
            devices_found++;
        }
    }
    
    ESP_LOGI(TAG, "Scan complete. Found %d devices.", devices_found);
}

void app_main(void) {
    i2c_master_init();
    
    while (1) {
        i2c_scan();
        vTaskDelay(pdMS_TO_TICKS(5000));  // Scan every 5 seconds
    }
}
```

### Procedure
1. Flash firmware to ESP32-S3
2. Open serial monitor
3. **Test with NO boards connected** - should find 0 devices
4. Connect Output Board only - should find 0x21
5. Connect Input Board only - should find 0x20
6. Connect Troops Module - should find 0x27 and 0x48

### Expected Results
```
I (1234) I2C_SCAN: Starting I2C scan...
I (1235) I2C_SCAN: Found device at address: 0x20
I (1236) I2C_SCAN: Found device at address: 0x21
I (1237) I2C_SCAN: Found device at address: 0x27
I (1238) I2C_SCAN: Found device at address: 0x48
I (1239) I2C_SCAN: Scan complete. Found 4 devices.
```

### Pass/Fail Criteria
- ✅ **PASS**: All expected devices detected at correct addresses
- ❌ **FAIL**: Missing devices or unexpected addresses

---

## Test 2: Output Board Test (MCP23017 @ 0x21)

**Purpose**: Verify all 16 output pins can be controlled individually.

### Hardware Required
- ESP32-S3
- Output Board (MCP23017 @ 0x21)
- Multimeter or LEDs to verify outputs

### Test Firmware

Create `test_output_board.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define MCP23017_ADDR        0x21  // Output board
#define MCP23017_IODIRA      0x00  // I/O direction A
#define MCP23017_IODIRB      0x01  // I/O direction B
#define MCP23017_GPIOA       0x12  // Port A
#define MCP23017_GPIOB       0x13  // Port B

static const char *TAG = "OUTPUT_TEST";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

esp_err_t mcp23017_write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void mcp23017_init_outputs(void) {
    // Configure all pins as outputs
    ESP_ERROR_CHECK(mcp23017_write_reg(MCP23017_IODIRA, 0x00));
    ESP_ERROR_CHECK(mcp23017_write_reg(MCP23017_IODIRB, 0x00));
    ESP_LOGI(TAG, "MCP23017 configured as outputs");
}

void test_walking_bit(void) {
    ESP_LOGI(TAG, "=== Walking Bit Test ===");
    ESP_LOGI(TAG, "Watch outputs: one LED should light at a time");
    
    // Test Port A (pins 0-7)
    for (int i = 0; i < 8; i++) {
        uint8_t pattern = (1 << i);
        mcp23017_write_reg(MCP23017_GPIOA, pattern);
        mcp23017_write_reg(MCP23017_GPIOB, 0x00);
        ESP_LOGI(TAG, "Port A Pin %d HIGH", i);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Test Port B (pins 8-15)
    for (int i = 0; i < 8; i++) {
        uint8_t pattern = (1 << i);
        mcp23017_write_reg(MCP23017_GPIOA, 0x00);
        mcp23017_write_reg(MCP23017_GPIOB, pattern);
        ESP_LOGI(TAG, "Port B Pin %d HIGH", i);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void test_all_on_off(void) {
    ESP_LOGI(TAG, "=== All Pins On/Off Test ===");
    
    // All ON
    ESP_LOGI(TAG, "All outputs HIGH");
    mcp23017_write_reg(MCP23017_GPIOA, 0xFF);
    mcp23017_write_reg(MCP23017_GPIOB, 0xFF);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // All OFF
    ESP_LOGI(TAG, "All outputs LOW");
    mcp23017_write_reg(MCP23017_GPIOA, 0x00);
    mcp23017_write_reg(MCP23017_GPIOB, 0x00);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void test_alternating_pattern(void) {
    ESP_LOGI(TAG, "=== Alternating Pattern Test ===");
    
    for (int i = 0; i < 5; i++) {
        // Even pins
        mcp23017_write_reg(MCP23017_GPIOA, 0xAA);  // 10101010
        mcp23017_write_reg(MCP23017_GPIOB, 0xAA);
        ESP_LOGI(TAG, "Even pins HIGH");
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Odd pins
        mcp23017_write_reg(MCP23017_GPIOA, 0x55);  // 01010101
        mcp23017_write_reg(MCP23017_GPIOB, 0x55);
        ESP_LOGI(TAG, "Odd pins HIGH");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Output Board Test Starting");
    i2c_master_init();
    mcp23017_init_outputs();
    
    while (1) {
        test_walking_bit();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_all_on_off();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_alternating_pattern();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGI(TAG, "Test cycle complete. Repeating...");
    }
}
```

### Procedure
1. Connect ONLY the Output Board (0x21)
2. Flash test firmware
3. Connect LEDs or multimeter to output pins
4. Observe patterns:
   - Walking bit (one LED at a time)
   - All on, all off
   - Alternating even/odd pins

### Expected Results
- Each output pin toggles individually
- No crosstalk between pins
- Clean transitions (no flickering)
- ULN2008A properly driving loads

### Pass/Fail Criteria
- ✅ **PASS**: All 16 outputs controllable, no stuck pins
- ❌ **FAIL**: 
  - Pin(s) stuck HIGH or LOW
  - Multiple pins activate together unexpectedly
  - I2C communication errors

### Troubleshooting
- **Pin stuck**: Check ULN2008A connection, solder joints
- **Multiple pins**: Check for shorts on PCB
- **No response**: Verify I2C address (0x21), check power, check I2C bus

---

## Test 3: Input Board Test (MCP23017 @ 0x20)

**Purpose**: Verify all 16 input pins can be read with pullups enabled.

### Hardware Required
- ESP32-S3
- Input Board (MCP23017 @ 0x20)
- Jumper wires to connect pins to GND for testing

### Test Firmware

Create `test_input_board.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define MCP23017_ADDR        0x20  // Input board
#define MCP23017_IODIRA      0x00
#define MCP23017_IODIRB      0x01
#define MCP23017_GPPUA       0x0C  // Pullup A
#define MCP23017_GPPUB       0x0D  // Pullup B
#define MCP23017_GPIOA       0x12
#define MCP23017_GPIOB       0x13

static const char *TAG = "INPUT_TEST";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

esp_err_t mcp23017_write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t mcp23017_read_reg(uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void mcp23017_init_inputs(void) {
    // Configure all pins as inputs
    ESP_ERROR_CHECK(mcp23017_write_reg(MCP23017_IODIRA, 0xFF));
    ESP_ERROR_CHECK(mcp23017_write_reg(MCP23017_IODIRB, 0xFF));
    
    // Enable pullups
    ESP_ERROR_CHECK(mcp23017_write_reg(MCP23017_GPPUA, 0xFF));
    ESP_ERROR_CHECK(mcp23017_write_reg(MCP23017_GPPUB, 0xFF));
    
    ESP_LOGI(TAG, "MCP23017 configured as inputs with pullups");
}

void test_input_monitoring(void) {
    uint8_t last_port_a = 0xFF;
    uint8_t last_port_b = 0xFF;
    
    ESP_LOGI(TAG, "=== Input Monitoring Test ===");
    ESP_LOGI(TAG, "Connect pins to GND to test");
    ESP_LOGI(TAG, "All pins should read HIGH (1) when floating");
    
    while (1) {
        uint8_t port_a, port_b;
        
        if (mcp23017_read_reg(MCP23017_GPIOA, &port_a) == ESP_OK &&
            mcp23017_read_reg(MCP23017_GPIOB, &port_b) == ESP_OK) {
            
            // Report changes on Port A
            if (port_a != last_port_a) {
                for (int i = 0; i < 8; i++) {
                    bool current = (port_a >> i) & 1;
                    bool previous = (last_port_a >> i) & 1;
                    if (current != previous) {
                        ESP_LOGI(TAG, "Port A Pin %d: %s", i, current ? "HIGH" : "LOW");
                    }
                }
                last_port_a = port_a;
            }
            
            // Report changes on Port B
            if (port_b != last_port_b) {
                for (int i = 0; i < 8; i++) {
                    bool current = (port_b >> i) & 1;
                    bool previous = (last_port_b >> i) & 1;
                    if (current != previous) {
                        ESP_LOGI(TAG, "Port B Pin %d: %s", i, current ? "HIGH" : "LOW");
                    }
                }
                last_port_b = port_b;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // Poll every 50ms
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Input Board Test Starting");
    i2c_master_init();
    mcp23017_init_inputs();
    test_input_monitoring();
}
```

### Procedure
1. Connect ONLY the Input Board (0x20)
2. Flash test firmware
3. Open serial monitor
4. **Initial state**: All pins should read HIGH (pullup active)
5. **Test each pin**: Connect pin to GND with jumper wire
6. **Verify**: Serial output shows pin going LOW
7. **Release**: Remove jumper, verify pin returns to HIGH

### Expected Results
```
I (1234) INPUT_TEST: Input Board Test Starting
I (1235) INPUT_TEST: MCP23017 configured as inputs with pullups
I (1236) INPUT_TEST: === Input Monitoring Test ===
I (1237) INPUT_TEST: All pins should read HIGH (1) when floating
I (5000) INPUT_TEST: Port A Pin 1: LOW
I (6000) INPUT_TEST: Port A Pin 1: HIGH
I (7000) INPUT_TEST: Port B Pin 3: LOW
```

### Pass/Fail Criteria
- ✅ **PASS**: 
  - All 16 pins read HIGH when floating
  - Each pin reads LOW when connected to GND
  - Pin returns to HIGH when released
  - No bouncing or false triggers
- ❌ **FAIL**:
  - Pin stuck LOW or HIGH
  - No response to GND connection
  - Erratic readings (bouncing without input)

### Troubleshooting
- **Pin stuck LOW**: Check for short to GND, verify pullup enabled
- **Pin stuck HIGH**: Check I2C communication, verify pin not damaged
- **Erratic readings**: Check for loose connections, EMI, bad solder joints

---

## Test 4: Troops Module Test (LCD + ADC)

**Purpose**: Verify LCD display and ADC reading functionality.

### Hardware Required
- ESP32-S3
- Troops Module (LCD @ 0x27, ADS1015 @ 0x48)
- Potentiometer connected to ADS1015 AIN0

### Test Firmware Part 1: LCD Test

Create `test_lcd.c`:

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define LCD_ADDR             0x27

// PCF8574 to HD44780 pin mapping
#define LCD_RS    0x01
#define LCD_RW    0x02
#define LCD_EN    0x04
#define LCD_BL    0x08  // Backlight

static const char *TAG = "LCD_TEST";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

void lcd_write_byte(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data | LCD_BL, true);  // Keep backlight on
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

void lcd_pulse_enable(uint8_t data) {
    lcd_write_byte(data | LCD_EN);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_byte(data & ~LCD_EN);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void lcd_send_nibble(uint8_t nibble, bool rs) {
    uint8_t data = (nibble << 4) | (rs ? LCD_RS : 0);
    lcd_pulse_enable(data);
}

void lcd_send_byte(uint8_t byte, bool rs) {
    lcd_send_nibble((byte >> 4) & 0x0F, rs);  // High nibble
    lcd_send_nibble(byte & 0x0F, rs);         // Low nibble
}

void lcd_command(uint8_t cmd) {
    lcd_send_byte(cmd, false);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_write_char(char c) {
    lcd_send_byte(c, true);
}

void lcd_write_string(const char *str) {
    while (*str) {
        lcd_write_char(*str++);
    }
}

void lcd_init(void) {
    ESP_LOGI(TAG, "Initializing LCD...");
    
    vTaskDelay(pdMS_TO_TICKS(50));  // Wait for LCD power-up
    
    // Initialize in 4-bit mode
    lcd_send_nibble(0x03, false);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x03, false);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_nibble(0x03, false);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_nibble(0x02, false);  // Set to 4-bit mode
    
    // Function set: 4-bit, 2 lines, 5x8 dots
    lcd_command(0x28);
    // Display on, cursor off, blink off
    lcd_command(0x0C);
    // Clear display
    lcd_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
    // Entry mode: increment, no shift
    lcd_command(0x06);
    
    ESP_LOGI(TAG, "LCD initialized");
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_command(0x80 | (col + row_offsets[row]));
}

void test_backlight(void) {
    ESP_LOGI(TAG, "=== Backlight Test ===");
    
    // Backlight OFF
    ESP_LOGI(TAG, "Backlight OFF (3 seconds)");
    lcd_write_byte(0x00);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Backlight ON
    ESP_LOGI(TAG, "Backlight ON");
    lcd_write_byte(LCD_BL);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void test_text_display(void) {
    ESP_LOGI(TAG, "=== Text Display Test ===");
    
    // Test 1: Simple text
    lcd_command(0x01);  // Clear
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_set_cursor(0, 0);
    lcd_write_string("  LCD TEST OK  ");
    lcd_set_cursor(0, 1);
    lcd_write_string("  Hardware v1  ");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 2: Troop format
    lcd_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_set_cursor(0, 0);
    lcd_write_string("  120K / 1.1M  ");
    lcd_set_cursor(0, 1);
    lcd_write_string("50% (60K)       ");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 3: All characters
    lcd_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_set_cursor(0, 0);
    lcd_write_string("0123456789ABCDEF");
    lcd_set_cursor(0, 1);
    lcd_write_string("!@#$%^&*()+-=<>");
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void test_scrolling_counter(void) {
    ESP_LOGI(TAG, "=== Counter Test ===");
    
    for (int i = 0; i < 20; i++) {
        char line1[17], line2[17];
        snprintf(line1, sizeof(line1), "Counter: %d     ", i);
        snprintf(line2, sizeof(line2), "Value: %d       ", i * 10);
        
        lcd_set_cursor(0, 0);
        lcd_write_string(line1);
        lcd_set_cursor(0, 1);
        lcd_write_string(line2);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "LCD Test Starting");
    i2c_master_init();
    lcd_init();
    
    while (1) {
        test_backlight();
        test_text_display();
        test_scrolling_counter();
        
        ESP_LOGI(TAG, "Test cycle complete. Repeating in 5s...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### Test Firmware Part 2: ADC Test

Create `test_adc.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define ADS1015_ADDR         0x48
#define ADS1015_REG_CONFIG   0x01
#define ADS1015_REG_CONVERT  0x00

static const char *TAG = "ADC_TEST";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

esp_err_t ads1015_write_config(uint16_t config) {
    uint8_t data[3] = {
        ADS1015_REG_CONFIG,
        (config >> 8) & 0xFF,
        config & 0xFF
    };
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADS1015_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 3, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ads1015_read_conversion(int16_t *result) {
    uint8_t data[2];
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADS1015_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, ADS1015_REG_CONVERT, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADS1015_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        *result = (data[0] << 8) | data[1];
        *result >>= 4;  // ADS1015 is 12-bit, data is left-justified
    }
    
    return ret;
}

int16_t ads1015_read_adc(uint8_t channel) {
    // Config: Single-shot, AINx vs GND, ±4.096V, 1600SPS
    uint16_t config = 0xC383 | ((channel & 0x03) << 12);
    
    if (ads1015_write_config(config) != ESP_OK) {
        return -1;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));  // Wait for conversion
    
    int16_t result;
    if (ads1015_read_conversion(&result) != ESP_OK) {
        return -1;
    }
    
    return result;
}

void test_adc_continuous(void) {
    ESP_LOGI(TAG, "=== ADC Continuous Test ===");
    ESP_LOGI(TAG, "Adjust potentiometer to test full range");
    
    int16_t last_percent = -1;
    
    while (1) {
        int16_t raw = ads1015_read_adc(0);  // Channel 0 (AIN0)
        
        if (raw >= 0) {
            // Convert to percentage (12-bit: 0-4095)
            int16_t percent = (raw * 100) / 4095;
            
            // Report changes
            if (abs(percent - last_percent) >= 1) {
                ESP_LOGI(TAG, "ADC Raw: %4d | Percent: %3d%%", raw, percent);
                last_percent = percent;
            }
        } else {
            ESP_LOGE(TAG, "ADC read failed");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ADC Test Starting");
    i2c_master_init();
    
    ESP_LOGI(TAG, "Testing ADS1015 at address 0x48");
    test_adc_continuous();
}
```

### Procedure

**LCD Test**:
1. Connect Troops Module
2. Flash LCD test firmware
3. Verify:
   - Backlight turns on/off
   - Text displays correctly on both lines
   - Characters are readable
   - Counter updates smoothly

**ADC Test**:
1. Keep Troops Module connected
2. Flash ADC test firmware
3. Slowly turn potentiometer from 0% to 100%
4. Verify serial output shows:
   - Raw value: 0 → 4095
   - Percent: 0% → 100%
   - Smooth transitions, no jumps

### Expected Results

**LCD**:
```
I (1234) LCD_TEST: LCD Test Starting
I (1235) LCD_TEST: Initializing LCD...
I (1250) LCD_TEST: LCD initialized
I (1251) LCD_TEST: === Backlight Test ===
[LCD displays text clearly]
```

**ADC**:
```
I (1234) ADC_TEST: ADC Test Starting
I (1235) ADC_TEST: === ADC Continuous Test ===
I (1240) ADC_TEST: ADC Raw:    0 | Percent:   0%
I (2000) ADC_TEST: ADC Raw:  512 | Percent:  12%
I (3000) ADC_TEST: ADC Raw: 2048 | Percent:  50%
I (4000) ADC_TEST: ADC Raw: 4095 | Percent: 100%
```

### Pass/Fail Criteria

**LCD**:
- ✅ **PASS**: 
  - Backlight controllable
  - All characters display correctly
  - Both lines work
  - No flickering or artifacts
- ❌ **FAIL**:
  - Garbled text
  - Missing characters
  - Backlight stuck
  - I2C errors

**ADC**:
- ✅ **PASS**:
  - Reads 0-4095 range (12-bit)
  - Smooth value changes
  - Accurate percentage conversion
  - No erratic readings
- ❌ **FAIL**:
  - Stuck at 0 or max value
  - Jumpy readings (noise)
  - I2C communication errors

### Troubleshooting

**LCD Issues**:
- **No display**: Check I2C address (try 0x3F if 0x27 fails), verify 5V power
- **Garbled text**: Reinitialize, check I2C speed (try 50kHz)
- **Dim display**: Adjust contrast pot on backpack
- **No backlight**: Check backpack jumper, verify PCF8574

**ADC Issues**:
- **No reading**: Verify 0x48 address, check power, check I2C
- **Stuck at 0**: Check potentiometer connection, verify GND
- **Stuck at max**: Check potentiometer wiring, verify Vcc
- **Noisy**: Add capacitor to pot, check grounding, slow down reads

---

## Test 5: Integration Test (All Boards)

**Purpose**: Verify all boards work together without interference.

### Hardware Required
- ESP32-S3
- All three boards connected simultaneously

### Test Firmware

Create `test_integration.c` combining elements from previous tests:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define MCP23017_INPUT_ADDR  0x20
#define MCP23017_OUTPUT_ADDR 0x21
#define LCD_ADDR             0x27
#define ADS1015_ADDR         0x48

static const char *TAG = "INTEGRATION";

// [Include all helper functions from previous tests]

void test_all_devices(void) {
    ESP_LOGI(TAG, "=== Integration Test ===");
    ESP_LOGI(TAG, "Testing all devices simultaneously");
    
    while (1) {
        // 1. Read inputs
        uint8_t port_a, port_b;
        if (mcp23017_read_reg_addr(MCP23017_INPUT_ADDR, 0x12, &port_a) == ESP_OK &&
            mcp23017_read_reg_addr(MCP23017_INPUT_ADDR, 0x13, &port_b) == ESP_OK) {
            ESP_LOGI(TAG, "Inputs: PortA=0x%02X PortB=0x%02X", port_a, port_b);
        }
        
        // 2. Set outputs (mirror inputs)
        mcp23017_write_reg_addr(MCP23017_OUTPUT_ADDR, 0x12, port_a);
        mcp23017_write_reg_addr(MCP23017_OUTPUT_ADDR, 0x13, port_b);
        
        // 3. Read ADC
        int16_t adc_raw = ads1015_read_adc(0);
        int16_t percent = (adc_raw * 100) / 4095;
        
        // 4. Update LCD
        char line1[17], line2[17];
        snprintf(line1, sizeof(line1), "Inputs: %02X %02X  ", port_a, port_b);
        snprintf(line2, sizeof(line2), "Slider: %3d%%    ", percent);
        
        lcd_set_cursor(0, 0);
        lcd_write_string(line1);
        lcd_set_cursor(0, 1);
        lcd_write_string(line2);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Integration Test Starting");
    
    i2c_master_init();
    
    // Initialize all devices
    mcp23017_init_inputs(MCP23017_INPUT_ADDR);
    mcp23017_init_outputs(MCP23017_OUTPUT_ADDR);
    lcd_init();
    
    ESP_LOGI(TAG, "All devices initialized");
    
    test_all_devices();
}
```

### Procedure
1. Connect all three boards to I2C bus
2. Flash integration firmware
3. Test simultaneous operation:
   - Press input buttons → outputs mirror inputs
   - Adjust slider → LCD shows percentage
   - Verify no I2C conflicts or lockups

### Pass/Fail Criteria
- ✅ **PASS**: All devices respond without interference
- ❌ **FAIL**: I2C errors, device conflicts, system lockup

---

## Test Documentation Template

For each test, record results:

```
Test: _______________
Date: _______________
Firmware Version: _______________

Board Serial Number: _______________
I2C Address: _______________

Results:
[ ] PASS  [ ] FAIL

Notes:
_________________________________
_________________________________

Issues Found:
_________________________________
_________________________________

Signature: _______________
```

---

## Common Issues & Solutions

### I2C Bus Issues
- **All devices fail**: Check SDA/SCL wiring, verify pullups (4.7kΩ typical)
- **Intermittent failures**: Reduce I2C speed to 50kHz, check cable length
- **Address conflicts**: Use I2C scanner to verify no duplicates

### Power Issues
- **Brownouts**: Check 5V rail stability, use separate power for boards
- **Noise**: Add decoupling capacitors near each IC (100nF)

### Firmware Issues
- **I2C timeout**: Increase timeout in `i2c_master_cmd_begin()`
- **Stack overflow**: Increase task stack size in `xTaskCreate()`

---

## Recommended Test Order

1. **Day 1**: I2C scan + Output board
2. **Day 2**: Input board
3. **Day 3**: LCD test
4. **Day 4**: ADC test
5. **Day 5**: Integration test

This phased approach builds confidence incrementally and isolates issues quickly.

---

## Safety Checklist

Before testing:
- [ ] 12V power supply current-limited
- [ ] No shorts on PCBs (visual inspection)
- [ ] I2C pullup resistors present
- [ ] ESP32 not connected to computer while high voltage testing
- [ ] Test environment ESD-safe

---

**Document Version**: 1.0  
**Last Updated**: December 21, 2025  
**Author**: OTS Firmware Team
