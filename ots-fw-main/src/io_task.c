#include "io_task.h"
#include "button_handler.h"
#include "adc_handler.h"
#include "led_controller.h"
#include "io_expander.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTS_IO_TASK";

#define IO_TASK_STACK_SIZE 4096
#define IO_SCAN_INTERVAL_MS 50
#define ADC_SCAN_INTERVAL_MS 100

static TaskHandle_t io_task_handle = NULL;
static bool task_running = false;

// Forward declaration
static void io_task_main(void *pvParameters);

esp_err_t io_task_start(void) {
    if (io_task_handle != NULL) {
        ESP_LOGW(TAG, "I/O task already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting I/O task...");
    
    // Create the I/O task
    BaseType_t result = xTaskCreate(
        io_task_main,
        "io_task",
        IO_TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY_BUTTON_MONITOR,
        &io_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create I/O task");
        return ESP_FAIL;
    }
    
    task_running = true;
    ESP_LOGI(TAG, "I/O task started");
    return ESP_OK;
}

static void io_task_main(void *pvParameters) {
    ESP_LOGI(TAG, "I/O task main loop started");
    
    const TickType_t scan_interval = pdMS_TO_TICKS(IO_SCAN_INTERVAL_MS);
    TickType_t last_wake_time = xTaskGetTickCount();
    
    uint32_t adc_scan_counter = 0;
    const uint32_t adc_scan_divisor = ADC_SCAN_INTERVAL_MS / IO_SCAN_INTERVAL_MS;  // Scan ADC every N loops
    
    uint32_t health_check_counter = 0;
    const uint32_t health_check_divisor = 10000 / IO_SCAN_INTERVAL_MS;  // Health check every 10s
    
    while (task_running) {
        // Scan buttons every loop (50ms)
        button_handler_scan();
        
        // Scan ADC channels less frequently (100ms)
        if (adc_scan_counter++ >= adc_scan_divisor) {
            adc_handler_scan();
            adc_scan_counter = 0;
        }
        
        // Perform I/O expander health check (every 10 seconds)
        if (health_check_counter++ >= health_check_divisor) {
            if (!io_expander_health_check()) {
                ESP_LOGW(TAG, "I/O health check failed - attempting recovery...");
                uint8_t recovered = io_expander_attempt_recovery();
                if (recovered > 0) {
                    ESP_LOGI(TAG, "Recovered %d board(s)", recovered);
                }
            }
            health_check_counter = 0;
        }
        
        // LED updates are handled by led_controller task
        
        // Wait for next scan interval (maintains precise timing)
        vTaskDelayUntil(&last_wake_time, scan_interval);
    }
    
    ESP_LOGI(TAG, "I/O task main loop ended");
}
