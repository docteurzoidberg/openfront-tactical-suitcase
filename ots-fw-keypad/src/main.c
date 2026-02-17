/**
 * @file main.c
 * @brief OTS Keypad Module Firmware
 * 
 * 15-key RGB mechanical keyboard with dual-mode operation:
 * - CAN bus mode: Integrated OTS module
 * - USB HID mode: Standalone keyboard (future)
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
#include "esp_timer.h"
#include "nvs_flash.h"
#include "config.h"
#include "matrix_scanner.h"
#include "led_controller.h"
#include "can_handler.h"

static const char *TAG = "KEYPAD";

/**
 * @brief Key event callback (from matrix scanner)
 * 
 * Called when a key press/release is detected.
 * Forwards key events to CAN bus.
 */
static void on_key_event(uint8_t key_id, key_state_t state) {
    ESP_LOGI(TAG, "Key K%d %s", key_id, 
             state == KEY_STATE_PRESSED ? "PRESSED" : "RELEASED");
    
    // Send key event to CAN bus
    esp_err_t ret = can_handler_send_key_event(key_id, state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send key event: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "OTS Keypad Module v%s", OTS_KEYPAD_VERSION);
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "Hardware: M5Stack Stamp S3 (ESP32-S3)");
    
    // Initialize NVS
    ret = nvs_flash_init();
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
    ESP_LOGI(TAG, "  Scan rate: %dms (%d Hz)", KEYPAD_SCAN_RATE_MS, 1000 / KEYPAD_SCAN_RATE_MS);
    ESP_LOGI(TAG, "  Debounce: %dms", KEYPAD_DEBOUNCE_MS);
    
    // ========================================================================
    // Module Initialization (bottom-up)
    // ========================================================================
    
    ESP_LOGI(TAG, "Initializing modules...");
    
    // 1. LED Controller
    ret = led_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED controller init failed: %s", esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }
    
    // 2. CAN Handler
    ret = can_handler_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CAN handler init failed: %s", esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }
    
    // 3. Matrix Scanner
    ret = matrix_scanner_init(on_key_event);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Matrix scanner init failed: %s", esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }
    
    ESP_LOGI(TAG, "All modules initialized successfully");
    
    // ========================================================================
    // Module Startup
    // ========================================================================
    
    ESP_LOGI(TAG, "Starting modules...");
    
    // 1. Push initial LED state (all OFF)
    ret = led_controller_update();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED update failed: %s", esp_err_to_name(ret));
    }
    
    // 2. Start CAN handler (sends MODULE_ANNOUNCE)
    ret = can_handler_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CAN handler start failed: %s", esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }
    
    // 3. Start matrix scanner (creates scan task)
    ret = matrix_scanner_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Matrix scanner start failed: %s", esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }
    
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "Keypad module ready");
    ESP_LOGI(TAG, "===================================");
    
    // Main loop - monitor system health
    uint32_t loop_count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        loop_count++;
        if (loop_count % 12 == 0) {  // Every 60 seconds
            ESP_LOGI(TAG, "System running... (uptime: %lld s)", 
                     esp_timer_get_time() / 1000000ULL);
        }
    }
}
