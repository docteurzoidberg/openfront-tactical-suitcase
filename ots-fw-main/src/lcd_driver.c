#include "lcd_driver.h"
#include "config.h"
#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>

static const char* TAG = "OTS_LCD";

// LCD commands (HD44780)
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x04
#define LCD_CMD_DISPLAY_CONTROL 0x08
#define LCD_CMD_FUNCTION_SET    0x20
#define LCD_CMD_SET_DDRAM_ADDR  0x80

// LCD flags
#define LCD_ENTRY_LEFT          0x02
#define LCD_DISPLAY_ON          0x04
#define LCD_CURSOR_OFF          0x00
#define LCD_BLINK_OFF           0x00
#define LCD_2LINE               0x08
#define LCD_5x8DOTS             0x00
#define LCD_BACKLIGHT           0x08

// PCF8574 pin mapping for HD44780
#define LCD_RS                  0x01
#define LCD_RW                  0x02
#define LCD_EN                  0x04

static uint8_t lcd_addr = LCD_I2C_ADDR;
static bool lcd_initialized = false;

// I2C helper
static esp_err_t i2c_write_byte(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t lcd_write_nibble(uint8_t data, bool rs) {
    uint8_t value = (data & 0xF0) | LCD_BACKLIGHT;
    if (rs) value |= LCD_RS;
    
    // Pulse enable
    i2c_write_byte(value | LCD_EN);
    vTaskDelay(pdMS_TO_TICKS(1));
    i2c_write_byte(value & ~LCD_EN);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    return ESP_OK;
}

static esp_err_t lcd_write_byte_internal(uint8_t data, bool rs) {
    lcd_write_nibble(data & 0xF0, rs);
    lcd_write_nibble((data << 4) & 0xF0, rs);
    return ESP_OK;
}

esp_err_t lcd_command(uint8_t cmd) {
    return lcd_write_byte_internal(cmd, false);
}

esp_err_t lcd_write_char(char c) {
    return lcd_write_byte_internal(c, true);
}

esp_err_t lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t addr = (row == 0) ? 0x00 : 0x40;
    addr += col;
    return lcd_command(LCD_CMD_SET_DDRAM_ADDR | addr);
}

esp_err_t lcd_write_string(const char* str) {
    if (!lcd_initialized || !str) {
        return ESP_FAIL;
    }
    
    // Optimized: Batch I2C writes for entire string
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd_addr << 1) | I2C_MASTER_WRITE, true);
    
    while (*str) {
        uint8_t c = *str++;
        
        // Write high nibble
        uint8_t high = (c & 0xF0) | LCD_BACKLIGHT | LCD_RS;
        i2c_master_write_byte(cmd, high | LCD_EN, true);
        i2c_master_write_byte(cmd, high, true);
        
        // Write low nibble
        uint8_t low = ((c << 4) & 0xF0) | LCD_BACKLIGHT | LCD_RS;
        i2c_master_write_byte(cmd, low | LCD_EN, true);
        i2c_master_write_byte(cmd, low, true);
    }
    
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

esp_err_t lcd_write_line(uint8_t row, const char* str) {
    if (!lcd_initialized || row >= LCD_ROWS || !str) {
        return ESP_FAIL;
    }
    
    // Set cursor to start of line
    lcd_set_cursor(0, row);
    
    // Write string (will be padded/truncated to LCD_COLS by caller if needed)
    return lcd_write_string(str);
}

esp_err_t lcd_clear(void) {
    lcd_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));
    return ESP_OK;
}

esp_err_t lcd_init(uint8_t i2c_addr) {
    lcd_addr = i2c_addr;
    
    // Wait for LCD to power up
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Initialize in 4-bit mode
    lcd_write_nibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_nibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_nibble(0x20, false);  // 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(1));
    
    // Function set: 4-bit, 2 lines, 5x8 font
    lcd_command(LCD_CMD_FUNCTION_SET | LCD_2LINE | LCD_5x8DOTS);
    
    // Display control: display on, cursor off, blink off
    lcd_command(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF);
    
    // Clear display
    lcd_clear();
    
    // Entry mode: left to right
    lcd_command(LCD_CMD_ENTRY_MODE | LCD_ENTRY_LEFT);
    
    lcd_initialized = true;
    ESP_LOGI(TAG, "LCD initialized at 0x%02x", lcd_addr);
    return ESP_OK;
}

esp_err_t lcd_show_splash(uint32_t delay_ms) {
    if (!lcd_initialized) {
        ESP_LOGE(TAG, "LCD not initialized");
        return ESP_FAIL;
    }
    
    // Line 1: Project name
    lcd_set_cursor(0, 0);
    lcd_write_string("  OpenFront.io  ");
    
    // Line 2: "Tactical Case" + version
    char line2[LCD_COLS + 1];
    snprintf(line2, sizeof(line2), "Tactical %-7s", OTS_FIRMWARE_VERSION);
    lcd_set_cursor(0, 1);
    lcd_write_string(line2);
    
    // Wait for specified delay
    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        lcd_clear();
    }
    
    return ESP_OK;
}
