#include "io_task.h"
#include "button_handler.h"
#include "led_controller.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IO_TASK";

#define IO_TASK_STACK_SIZE 4096
#define IO_SCAN_INTERVAL_MS 50

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

esp_err_t io_task_stop(void) {
    if (io_task_handle == NULL) {
        ESP_LOGW(TAG, "I/O task not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping I/O task...");
    task_running = false;
    
    // Delete the task
    vTaskDelete(io_task_handle);
    io_task_handle = NULL;
    
    ESP_LOGI(TAG, "I/O task stopped");
    return ESP_OK;
}

bool io_task_is_running(void) {
    return task_running && io_task_handle != NULL;
}

static void io_task_main(void *pvParameters) {
    ESP_LOGI(TAG, "I/O task main loop started");
    
    const TickType_t scan_interval = pdMS_TO_TICKS(IO_SCAN_INTERVAL_MS);
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (task_running) {
        // Scan buttons and update state
        button_handler_scan();
        
        // LED updates are handled by led_controller task
        // No need to do anything here for LEDs
        
        // Wait for next scan interval (maintains precise timing)
        vTaskDelayUntil(&last_wake_time, scan_interval);
    }
    
    ESP_LOGI(TAG, "I/O task main loop ended");
}
