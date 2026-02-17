/**
 * @file main.c
 * @brief OTS Keypad Module Firmware
 * 
 * 15-key RGB mechanical keyboard with dual-mode operation:
 * - CAN bus mode: Integrated OTS module
 * - USB HID mode: Standalone keyboard
 * 
 * Hardware:
 * - M5Stack Stamp S3 (ESP32-S3)
 * - 15x Cherry MX switches (3x7 matrix)
 * - 15x SK6812-MINI-E RGB LEDs
 * - TJA1050 CAN transceiver
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"

static const char *TAG = "KEYPAD";

void app_main(void)
{
    ESP_LOGI(TAG, "OTS Keypad Module Firmware v%s", OTS_KEYPAD_VERSION);
    ESP_LOGI(TAG, "Hardware: M5Stack Stamp S3 (ESP32-S3)");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Log configuration
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Matrix: %dx%d (%d keys)", MATRIX_ROWS, MATRIX_COLS, NUM_KEYS);
    ESP_LOGI(TAG, "  Row GPIOs: %d, %d, %d", GPIO_ROW0, GPIO_ROW1, GPIO_ROW2);
    ESP_LOGI(TAG, "  RGB Data GPIO: %d (%d LEDs)", GPIO_RGB_DATA, NUM_LEDS);
    ESP_LOGI(TAG, "  CAN TX/RX: GPIO %d/%d", GPIO_CAN_TX, GPIO_CAN_RX);
    
    ESP_LOGI(TAG, "Firmware initialization complete");
    ESP_LOGI(TAG, "Ready for feature implementation");
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
