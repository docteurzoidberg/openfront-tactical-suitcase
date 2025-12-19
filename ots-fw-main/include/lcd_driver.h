#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

// LCD dimensions
#define LCD_COLS 16
#define LCD_ROWS 2

// Default I2C address for LCD (PCF8574 backpack)
#define LCD_I2C_ADDR 0x27

/**
 * @brief Initialize LCD display
 * 
 * @param i2c_addr I2C address of LCD backpack (default: 0x27)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_init(uint8_t i2c_addr);

/**
 * @brief Clear LCD display
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_clear(void);

/**
 * @brief Set cursor position
 * 
 * @param col Column (0-15)
 * @param row Row (0-1)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_set_cursor(uint8_t col, uint8_t row);

/**
 * @brief Write string to LCD at current cursor position
 * 
 * @param str Null-terminated string
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_write_string(const char* str);

/**
 * @brief Write string to entire line (optimized)
 * 
 * @param row Row number (0-1)
 * @param str Null-terminated string (should be padded to LCD_COLS if needed)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_write_line(uint8_t row, const char* str);

/**
 * @brief Show splash screen on LCD
 * 
 * Displays project name and version, then clears after delay
 * 
 * @param delay_ms Duration to show splash screen (milliseconds)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_show_splash(uint32_t delay_ms);

/**
 * @brief Write single character to LCD
 * 
 * @param c Character to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_write_char(char c);

/**
 * @brief Send command to LCD
 * 
 * @param cmd Command byte
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_command(uint8_t cmd);

#endif // LCD_DRIVER_H
