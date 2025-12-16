#include "nuke_module.h"
#include "module_io.h"
#include "button_handler.h"
#include "led_controller.h"
#include "ws_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "NUKE_MOD";

static module_status_t status = {0};

// Module initialization
static esp_err_t nuke_module_init(void) {
    ESP_LOGI(TAG, "Initializing nuke module...");
    
    // Pins are already configured globally (board 0 = input, board 1 = output)
    // Just verify we can read/write
    
    // Turn off all nuke LEDs initially
    for (int i = 0; i < 3; i++) {
        module_io_set_nuke_led(i, false);
    }
    
    status.initialized = true;
    status.operational = true;
    status.error_count = 0;
    
    ESP_LOGI(TAG, "Nuke module initialized (3 buttons, 3 LEDs)");
    return ESP_OK;
}

// Module update (periodic)
static esp_err_t nuke_module_update(void) {
    // Nothing to do here - button scanning handled by button_handler
    // LED blinking handled by led_controller
    return ESP_OK;
}

// Handle events
static bool nuke_module_handle_event(const internal_event_t *event) {
    // Handle nuke launch events (for LED feedback)
    if (event->type == GAME_EVENT_NUKE_LAUNCHED || 
        event->type == GAME_EVENT_HYDRO_LAUNCHED ||
        event->type == GAME_EVENT_MIRV_LAUNCHED) {
        
        uint8_t led_index = 0;
        if (event->type == GAME_EVENT_NUKE_LAUNCHED) led_index = 0;
        else if (event->type == GAME_EVENT_HYDRO_LAUNCHED) led_index = 1;
        else if (event->type == GAME_EVENT_MIRV_LAUNCHED) led_index = 2;
        
        ESP_LOGI(TAG, "Nuke launched: %s (LED %d)", event_type_to_string(event->type), led_index);
        
        // Blink the corresponding LED for 10 seconds
        led_controller_nuke_blink(led_index, 10000);
        
        return true;
    }
    
    return false;
}

// Get module status
static void nuke_module_get_status(module_status_t *out_status) {
    if (out_status) {
        memcpy(out_status, &status, sizeof(module_status_t));
    }
}

// Shutdown module
static esp_err_t nuke_module_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down nuke module...");
    
    // Turn off all LEDs
    for (int i = 0; i < 3; i++) {
        module_io_set_nuke_led(i, false);
    }
    
    status.operational = false;
    return ESP_OK;
}

// Module definition
hardware_module_t nuke_module = {
    .name = "Nuke Module",
    .enabled = true,
    .init = nuke_module_init,
    .update = nuke_module_update,
    .handle_event = nuke_module_handle_event,
    .get_status = nuke_module_get_status,
    .shutdown = nuke_module_shutdown
};
