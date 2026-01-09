/**
 * OTS CAN Test Firmware
 * 
 * Interactive CAN bus testing tool for OTS project.
 * Supports monitoring, simulation, and protocol validation.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "can_driver.h"
#include "can_discovery.h"
#include "can_test.h"

static const char *TAG = "can_test";

// Global state
test_state_t g_test_state = {
    .mode = MODE_IDLE,
    .running = true,
    .can_hardware_present = false,  // Detected during init
    .rx_count = 0,
    .tx_count = 0,
    .error_count = 0,
    .show_raw_hex = true,
    .show_parsed = true,
    .can_filter = 0  // Show all messages
};

// CAN RX task
static void can_rx_task(void *arg) {
    can_frame_t frame;
    int consecutive_errors = 0;
    
    while (g_test_state.running) {
        // Skip RX loop if hardware not present
        if (!g_test_state.can_hardware_present) {
            vTaskDelay(pdMS_TO_TICKS(1000));  // Sleep 1s when no hardware
            continue;
        }
        
        esp_err_t ret = can_driver_receive(&frame, pdMS_TO_TICKS(100));  // 100ms timeout with yield inside
        
        if (ret == ESP_OK) {
            g_test_state.rx_count++;
            consecutive_errors = 0;  // Reset error counter
            
            // Apply filter if set
            if (g_test_state.can_filter == 0 || frame.id == g_test_state.can_filter) {
                // Process based on mode
                switch (g_test_state.mode) {
                    case MODE_MONITOR:
                        can_monitor_process_frame(&frame);
                        break;
                    
                    case MODE_AUDIO_MODULE:
                    case MODE_CONTROLLER:
                        can_simulator_process_frame(&frame);
                        break;
                    
                    default:
                        // Show in idle mode too
                        can_decoder_print_frame(&frame, g_test_state.show_raw_hex, 
                                               g_test_state.show_parsed);
                        break;
                }
            }            
            // Yield after processing to prevent watchdog
            vTaskDelay(pdMS_TO_TICKS(1));
                    } else if (ret != ESP_ERR_TIMEOUT) {
            g_test_state.error_count++;
            consecutive_errors++;
            
            // Only log first 3 errors, then disable RX to stop spam
            if (consecutive_errors <= 3) {
                ESP_LOGE(TAG, "CAN receive error: %s", esp_err_to_name(ret));
            } else if (consecutive_errors == 4) {
                ESP_LOGE(TAG, "Too many CAN errors - hardware may be disconnected");
                ESP_LOGW(TAG, "Disabling CAN receive to prevent log spam");
                g_test_state.can_hardware_present = false;
            }
        }
    }
    
    vTaskDelete(NULL);
}

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Print banner
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     OTS CAN Test Firmware v%s            ║\n", CAN_TEST_VERSION);
    printf("║     Interactive CAN Bus Testing Tool            ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Initialize CAN driver with board-specific pins
    ESP_LOGI(TAG, "Initializing CAN driver...");
    can_config_t can_config = CAN_CONFIG_DEFAULT();
    
    // Lower bitrate for testing (more tolerant of signal quality issues)
    can_config.bitrate = 125000;  // 125 kbps (was 500 kbps)
    ESP_LOGI(TAG, "Using 125 kbps bitrate");
    
    // Disable loopback - using two physical devices with built-in 120Ω termination
    can_config.loopback = false;
    ESP_LOGI(TAG, "Normal mode (not loopback) - WWZMDiB modules have built-in termination");
    
    // Override default pins based on board type
#ifdef CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3 DevKit: GPIO 5 (TX), GPIO 4 (RX)
    // TJA1050 wiring: ESP32 TX → TJA1050 TXD, ESP32 RX → TJA1050 RXD (NOT CROSSED)
    can_config.tx_gpio = 5;
    can_config.rx_gpio = 4;
    ESP_LOGI(TAG, "Board: ESP32-S3 DevKit, using GPIO5(TX)/GPIO4(RX) for CAN");
#else
    // ESP32-A1S AudioKit: GPIO 5 (TX), GPIO 18 (RX) - testing GPIO5 (same as S3 uses)
    // GPIO5 = shared with headphone detect, GPIO18 = generally available
    // TJA1050 wiring: ESP32 TX → TJA1050 TXD, ESP32 RX → TJA1050 RXD (NOT CROSSED)
    can_config.tx_gpio = 5;
    can_config.rx_gpio = 18;
    ESP_LOGI(TAG, "Board: ESP32-A1S AudioKit, using GPIO5(TX)/GPIO18(RX) for CAN");
#endif
    
    ret = can_driver_init(&can_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CAN driver: %s", esp_err_to_name(ret));
        return;
    }
    
    // Test if physical CAN hardware is working by attempting a receive
    printf("  Testing CAN hardware...\n");
    
    can_frame_t test_frame;
    esp_err_t test_ret = can_driver_receive(&test_frame, 50);  // Quick 50ms test
    
    if (test_ret == ESP_ERR_TIMEOUT) {
        // Timeout is good - means hardware works, just no messages yet
        g_test_state.can_hardware_present = true;
        printf("✓ CAN hardware detected (physical mode)\n");
    } else if (test_ret == ESP_ERR_NOT_SUPPORTED || test_ret == ESP_ERR_INVALID_STATE) {
        // Hardware error - mock mode or transceiver not connected
        g_test_state.can_hardware_present = false;
        printf("⚠ No CAN transceiver detected (mock mode)\n");
        printf("  Commands will work, but no bus traffic will be received\n");
    } else {
        // Unexpected error or success
        g_test_state.can_hardware_present = (test_ret == ESP_OK);
        if (test_ret == ESP_OK) {
            printf("✓ CAN hardware detected (received frame immediately)\n");
        } else {
            ESP_LOGW(TAG, "Unexpected CAN test result: %s", esp_err_to_name(test_ret));
            printf("⚠ CAN hardware status unknown\n");
        }
    }
    
    // Log detailed TWAI status for diagnostics
    printf("\n");
    can_driver_log_twai_status();
    printf("\n");
    
    // Initialize components
    can_decoder_init();
    cli_handler_init();
    
    // Start CAN RX task
    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
    
    // Print quick help
    printf("\n");
    printf("Quick Start:\n");
    printf("  m       - Monitor mode (passive sniffer)\n");
    printf("  a       - Audio module simulator\n");
    printf("  c       - Controller simulator\n");
    printf("  h or ?  - Show all commands\n");
    printf("\n");
    printf("Ready. Type command: ");
    fflush(stdout);
    
    // Run CLI
    cli_handler_run();
    
    // Cleanup (never reached in normal operation)
    g_test_state.running = false;
    vTaskDelay(pdMS_TO_TICKS(100));
    can_driver_deinit();
}
