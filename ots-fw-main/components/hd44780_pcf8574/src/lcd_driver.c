#include "lcd_driver.h"

// Default version if not defined by application
#ifndef OTS_FIRMWARE_VERSION
#define OTS_FIRMWARE_VERSION "unknown"
#endif

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "OTS_LCD";

// ---- PCF8574 -> HD44780 mapping (known-good for common 0x27 backpacks)
// P0=RS, P1=RW, P2=EN, P3=BL, P4..P7 = D4..D7
#ifndef LCD_PCF_RS_MASK
#define LCD_PCF_RS_MASK 0x01
#endif
#ifndef LCD_PCF_RW_MASK
#define LCD_PCF_RW_MASK 0x02
#endif
#ifndef LCD_PCF_EN_MASK
#define LCD_PCF_EN_MASK 0x04
#endif

#ifndef LCD_BACKLIGHT_MASK
#define LCD_BACKLIGHT_MASK 0x08
#endif

// 0 = backlight bit high means ON (most common)
// 1 = backlight bit low means ON (some backpacks)
#ifndef LCD_BACKLIGHT_ACTIVE_LOW
#define LCD_BACKLIGHT_ACTIVE_LOW 0
#endif

// ---- HD44780 commands
#define LCD_CMD_CLEAR 0x01
#define LCD_CMD_HOME  0x02
#define LCD_CMD_ENTRY_MODE_SET 0x04
#define LCD_CMD_DISPLAY_CONTROL 0x08
#define LCD_CMD_FUNCTION_SET 0x20
#define LCD_CMD_SET_DDRAM_ADDR 0x80

// ---- HD44780 flags
#define LCD_ENTRY_INCREMENT 0x02
#define LCD_DISPLAY_ON      0x04
#define LCD_CURSOR_OFF      0x00
#define LCD_BLINK_OFF       0x00
#define LCD_2LINE           0x08
#define LCD_5x8DOTS         0x00

// ---- Timing (very conservative)
// Many HD44780-compatible clones and backpack combos need more time than the
// datasheet minimums, especially when powered from a separate 5V rail.
#define LCD_DELAY_POWERUP_MS 200
#define LCD_DELAY_INIT1_MS    10
#define LCD_DELAY_INIT2_MS    10
#define LCD_DELAY_INIT3_US  2000
#define LCD_DELAY_CMD_US     120
#define LCD_DELAY_CLEAR_MS     5
#define LCD_DELAY_EN_US       50

// Default I2C speed for the LCD expander. You can override at build time.
#ifndef LCD_I2C_SPEED_HZ
#define LCD_I2C_SPEED_HZ 50000
#endif

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_initialized = false;
static uint8_t s_addr = LCD_I2C_ADDR;
static bool s_backlight_on = true;

static inline uint8_t backlight_bits(void) {
    if (s_backlight_on) {
        return LCD_BACKLIGHT_ACTIVE_LOW ? 0x00 : LCD_BACKLIGHT_MASK;
    }
    return LCD_BACKLIGHT_ACTIVE_LOW ? LCD_BACKLIGHT_MASK : 0x00;
}

static inline esp_err_t pcf_write(uint8_t byte) {
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit(s_dev, &byte, 1, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t pulse_en(uint8_t base) {
    // EN low -> EN high -> EN low
    esp_err_t ret = pcf_write((uint8_t)(base & (uint8_t)~LCD_PCF_EN_MASK));
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_EN_US);

    ret = pcf_write((uint8_t)(base | LCD_PCF_EN_MASK));
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_EN_US);

    ret = pcf_write((uint8_t)(base & (uint8_t)~LCD_PCF_EN_MASK));
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_EN_US);

    return ESP_OK;
}

static esp_err_t write4(uint8_t nibble /* 0..15 */, bool rs) {
    // Data on P4..P7
    uint8_t data_bits = (uint8_t)((nibble & 0x0F) << 4);
    uint8_t base = (uint8_t)(data_bits | backlight_bits());

    if (rs) {
        base |= LCD_PCF_RS_MASK;
    }
    // Always write mode: RW low.
    base &= (uint8_t)~LCD_PCF_RW_MASK;

    // Data setup with EN low, then pulse.
    esp_err_t ret = pcf_write((uint8_t)(base & (uint8_t)~LCD_PCF_EN_MASK));
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_EN_US);
    return pulse_en(base);
}

static esp_err_t write8(uint8_t value, bool rs) {
    esp_err_t ret = write4((uint8_t)((value >> 4) & 0x0F), rs);
    if (ret != ESP_OK) return ret;
    ret = write4((uint8_t)(value & 0x0F), rs);
    if (ret != ESP_OK) return ret;
    return ESP_OK;
}

static esp_err_t cmd(uint8_t c) {
    esp_err_t ret = write8(c, false);
    if (ret != ESP_OK) return ret;
    if (c == LCD_CMD_CLEAR || c == LCD_CMD_HOME) {
        vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_CLEAR_MS));
    } else {
        esp_rom_delay_us(LCD_DELAY_CMD_US);
    }
    return ESP_OK;
}

static esp_err_t data(uint8_t d) {
    esp_err_t ret = write8(d, true);
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_CMD_US);
    return ESP_OK;
}

void lcd_backlight_on(void) {
    s_backlight_on = true;
    if (s_dev) {
        (void)pcf_write(backlight_bits());
    }
}

void lcd_backlight_off(void) {
    s_backlight_on = false;
    if (s_dev) {
        (void)pcf_write(backlight_bits());
    }
}

esp_err_t lcd_command(uint8_t c) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return cmd(c);
}

esp_err_t lcd_write_char(char c) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return data((uint8_t)c);
}

esp_err_t lcd_clear(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return cmd(LCD_CMD_CLEAR);
}

esp_err_t lcd_set_cursor(uint8_t col, uint8_t row) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (row >= LCD_ROWS) return ESP_ERR_INVALID_ARG;

    uint8_t addr = (row == 0) ? 0x00 : 0x40;
    addr = (uint8_t)(addr + col);
    return cmd((uint8_t)(LCD_CMD_SET_DDRAM_ADDR | addr));
}

esp_err_t lcd_write_string(const char *str) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!str) return ESP_ERR_INVALID_ARG;

    for (size_t i = 0; str[i] != '\0'; i++) {
        esp_err_t ret = data((uint8_t)str[i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t lcd_write_line(uint8_t row, const char *str) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!str) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = lcd_set_cursor(0, row);
    if (ret != ESP_OK) return ret;
    return lcd_write_string(str);
}

esp_err_t lcd_init(i2c_master_bus_handle_t bus, uint8_t i2c_addr) {
    s_initialized = false;
    s_addr = i2c_addr;

    if (!bus) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = s_addr,
        .scl_speed_hz = LCD_I2C_SPEED_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add LCD device: %s", esp_err_to_name(ret));
        s_dev = NULL;
        return ret;
    }

    // Probe
    ret = i2c_master_probe(bus, s_addr, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD not detected at 0x%02X (%s)", s_addr, esp_err_to_name(ret));
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    // Power-up delay
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_POWERUP_MS));

    // Expander known state: backlight + all control low + data low
    ret = pcf_write(backlight_bits());
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(1));

    // 4-bit initialization (datasheet)
    // 0x03, 0x03, 0x03, then 0x02
    ret = write4(0x03, false);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_INIT1_MS));

    ret = write4(0x03, false);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_INIT2_MS));

    ret = write4(0x03, false);
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_INIT3_US);

    ret = write4(0x02, false);
    if (ret != ESP_OK) return ret;
    esp_rom_delay_us(LCD_DELAY_CMD_US);

    // Function set: 4-bit, 2-line, 5x8
    ret = cmd((uint8_t)(LCD_CMD_FUNCTION_SET | LCD_2LINE | LCD_5x8DOTS));
    if (ret != ESP_OK) return ret;

    // Display off
    ret = cmd((uint8_t)(LCD_CMD_DISPLAY_CONTROL | LCD_CURSOR_OFF | LCD_BLINK_OFF));
    if (ret != ESP_OK) return ret;

    // Clear
    ret = cmd(LCD_CMD_CLEAR);
    if (ret != ESP_OK) return ret;

    // Entry mode
    ret = cmd((uint8_t)(LCD_CMD_ENTRY_MODE_SET | LCD_ENTRY_INCREMENT));
    if (ret != ESP_OK) return ret;

    // Display on
    ret = cmd((uint8_t)(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF));
    if (ret != ESP_OK) return ret;

    s_initialized = true;
    ESP_LOGI(TAG, "LCD initialized at 0x%02X", s_addr);
    return ESP_OK;
}

bool lcd_is_initialized(void) {
    return s_initialized;
}

esp_err_t lcd_show_splash(uint32_t delay_ms) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    (void)lcd_set_cursor(0, 0);
    (void)lcd_write_string("  OpenFront.io  ");

    char line2[LCD_COLS + 1];
    snprintf(line2, sizeof(line2), "Tactical %-7s", OTS_FIRMWARE_VERSION);
    (void)lcd_set_cursor(0, 1);
    (void)lcd_write_string(line2);

    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        (void)lcd_clear();
    }

    return ESP_OK;
}
