/**
 * CAN Monitor Mode - Passive Bus Sniffer
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "can_test.h"

static const char *TAG = "monitor";
static bool monitoring = false;
static uint32_t start_time_us = 0;

void can_monitor_start(void) {
    if (monitoring) {
        ESP_LOGW(TAG, "Monitor already running");
        return;
    }
    
    monitoring = true;
    start_time_us = esp_timer_get_time();
    g_test_state.rx_count = 0;
    g_test_state.tx_count = 0;
    g_test_state.error_count = 0;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    MONITOR MODE - PASSIVE                      ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Listening to all CAN bus traffic...                          ║\n");
    printf("║  Press 'q' to stop monitoring                                 ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    ESP_LOGI(TAG, "Monitor started");
}

void can_monitor_stop(void) {
    if (!monitoring) {
        return;
    }
    
    monitoring = false;
    uint32_t duration_us = esp_timer_get_time() - start_time_us;
    float duration_s = duration_us / 1000000.0f;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    MONITOR STATISTICS                          ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Duration:        %.2f seconds                              ║\n", duration_s);
    printf("║  Messages RX:     %lu                                          ║\n", g_test_state.rx_count);
    printf("║  Messages TX:     %lu                                          ║\n", g_test_state.tx_count);
    printf("║  Errors:          %lu                                          ║\n", g_test_state.error_count);
    if (duration_s > 0) {
        printf("║  Avg Rate:        %.1f msg/s                               ║\n", 
               g_test_state.rx_count / duration_s);
    }
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    ESP_LOGI(TAG, "Monitor stopped");
}

void can_monitor_process_frame(const can_frame_t *frame) {
    if (!monitoring) {
        return;
    }
    
    // Calculate timestamp relative to start
    uint32_t timestamp_us = esp_timer_get_time() - start_time_us;
    float timestamp_s = timestamp_us / 1000000.0f;
    
    // Print timestamp
    printf("[%8.3f] ", timestamp_s);
    
    // Print frame with decoder
    can_decoder_print_frame(frame, g_test_state.show_raw_hex, g_test_state.show_parsed);
}
