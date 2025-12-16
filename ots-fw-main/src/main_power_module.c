#include "main_power_module.h"
#include "module_io.h"
#include "led_controller.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "POWER_MOD";

static module_status_t status = {0};

// Module initialization
static esp_err_t main_power_module_init(void) {
    ESP_LOGI(TAG, "Initializing main power module...");
    
    // Pins are already configured globally (board 1 = output)
    // Just verify we can write
    
    // Turn off link LED initially
    module_io_set_link_led(false);
    
    status.initialized = true;
    status.operational = true;
    status.error_count = 0;
    
    ESP_LOGI(TAG, "Main power module initialized (link LED)");
    return ESP_OK;
}

// Module update (periodic)
static esp_err_t main_power_module_update(void) {
    // Nothing to do here - LED state controlled by events
    return ESP_OK;
}

// Handle events
static bool main_power_module_handle_event(const internal_event_t *event) {
    bool handled = false;
    
    switch (event->type) {
        // Network connection events
        case INTERNAL_EVENT_NETWORK_CONNECTED:
            ESP_LOGI(TAG, "Network connected - link LED ON");
            led_controller_link_set(true);
            handled = true;
            break;
            
        case INTERNAL_EVENT_NETWORK_DISCONNECTED:
            ESP_LOGI(TAG, "Network disconnected - turning off link LED");
            led_controller_link_set(false);
            handled = true;
            break;
            
        // WebSocket connection events
        case INTERNAL_EVENT_WS_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected - link LED solid ON");
            led_controller_link_set(true);  // Solid ON
            handled = true;
            break;
            
        case INTERNAL_EVENT_WS_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected - blinking link LED");
            led_controller_link_blink(500);  // Blink = network OK but no WS
            handled = true;
            break;
            
        // Error handling
        case INTERNAL_EVENT_WS_ERROR:
            ESP_LOGI(TAG, "WebSocket error - fast blinking link LED");
            led_controller_link_blink(200);  // Fast blink = error
            handled = true;
            break;
            
        default:
            break;
    }
    
    return handled;
}

// Get module status
static void main_power_module_get_status(module_status_t *out_status) {
    if (out_status) {
        memcpy(out_status, &status, sizeof(module_status_t));
    }
}

// Shutdown module
static esp_err_t main_power_module_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down main power module...");
    
    // Turn off link LED
    module_io_set_link_led(false);
    
    status.operational = false;
    return ESP_OK;
}

// Module definition
hardware_module_t main_power_module = {
    .name = "Main Power Module",
    .enabled = true,
    .init = main_power_module_init,
    .update = main_power_module_update,
    .handle_event = main_power_module_handle_event,
    .get_status = main_power_module_get_status,
    .shutdown = main_power_module_shutdown
};
