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
    
    while (g_test_state.running) {
        esp_err_t ret = can_driver_receive(&frame, pdMS_TO_TICKS(100));
        
        if (ret == ESP_OK) {
            g_test_state.rx_count++;
            
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
        } else if (ret != ESP_ERR_TIMEOUT) {
            g_test_state.error_count++;
            ESP_LOGE(TAG, "CAN receive error: %s", esp_err_to_name(ret));
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
    
    // Initialize CAN driver
    ESP_LOGI(TAG, "Initializing CAN driver...");
    can_config_t can_config = CAN_CONFIG_DEFAULT();
    ret = can_driver_init(&can_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CAN driver: %s", esp_err_to_name(ret));
        return;
    }
    
    // CAN driver automatically detects mode (mock or physical TWAI)
    printf("✓ CAN Driver initialized\n");
    
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
