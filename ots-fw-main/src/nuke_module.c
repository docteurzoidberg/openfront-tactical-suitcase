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

// Button to nuke type mapping
typedef struct {
    game_event_type_t event_type;
    const char *nuke_type;
} button_mapping_t;

static const button_mapping_t button_map[] = {
    {GAME_EVENT_NUKE_LAUNCHED, "atom"},
    {GAME_EVENT_HYDRO_LAUNCHED, "hydro"},
    {GAME_EVENT_MIRV_LAUNCHED, "mirv"}
};

// Handle events
static bool nuke_module_handle_event(const internal_event_t *event) {
    // Handle button press events
    if (event->type == INTERNAL_EVENT_BUTTON_PRESSED) {
        uint8_t button_index = event->data[0];
        
        if (button_index >= sizeof(button_map) / sizeof(button_map[0])) {
            ESP_LOGW(TAG, "Invalid button index: %d", button_index);
            return false;
        }
        
        ESP_LOGI(TAG, "Button %d pressed (%s)", button_index, button_map[button_index].nuke_type);
        
        // Create and send game event
        game_event_t game_event = {0};
        game_event.timestamp = esp_timer_get_time() / 1000;
        game_event.type = button_map[button_index].event_type;
        strncpy(game_event.message, "Nuke sent", sizeof(game_event.message) - 1);
        snprintf(game_event.data, sizeof(game_event.data), "{\"nukeType\":\"%s\"}", 
                 button_map[button_index].nuke_type);
        
        // Post to event dispatcher (for local handling - LED feedback)
        event_dispatcher_post_game_event(&game_event, EVENT_SOURCE_BUTTON);
        
        // Send to WebSocket server
        ws_client_send_event(&game_event);
        
        return true;
    }
    
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
