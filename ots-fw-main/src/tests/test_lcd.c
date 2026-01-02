/*
 * Minimal LCD Display Test (16x2 HD44780 via PCF8574 @ 0x27)
 *
 * Intentionally minimal: no bruteforce, no sweep, no interactive console.
 * Goal is to validate that lcd_driver.c initializes and writes stable text.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "i2c_bus.h"
#include "lcd_driver.h"

#include <stdio.h>

static const char *TAG = "TEST_LCD";

void app_main(void) {
    ESP_LOGI(TAG, "Starting minimal LCD test...");
    ESP_ERROR_CHECK(ots_i2c_bus_init());

    esp_err_t ret = lcd_init(ots_i2c_bus_get(), LCD_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lcd_init(0x%02X) failed: %s", LCD_I2C_ADDR, esp_err_to_name(ret));
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    lcd_backlight_on();
    (void)lcd_clear();
    (void)lcd_set_cursor(0, 0);
    (void)lcd_write_string("OTS LCD OK");
    (void)lcd_set_cursor(0, 1);
    (void)lcd_write_string("Counter: 0000");

    uint32_t counter = 0;
    while (1) {
        char line2[17];
        snprintf(line2, sizeof(line2), "Counter: %04lu", (unsigned long)counter++);
        (void)lcd_set_cursor(0, 1);
        (void)lcd_write_string(line2);
        ESP_LOGI(TAG, "%s", line2);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
