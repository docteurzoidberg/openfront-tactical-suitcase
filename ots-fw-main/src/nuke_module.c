#include "nuke_module.h"
#include "module_io.h"
#include "button_handler.h"
#include "led_handler.h"
#include "nuke_state_manager.h"
#include "ws_handlers.h"
#include "ots_common.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "OTS_NUKE";

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

// Button to nuke type mapping - all buttons use unified NUKE_LAUNCHED event
typedef struct {
    game_event_type_t event_type;
    const char *nuke_type;
    nuke_type_t tracker_type;
} button_mapping_t;

static const button_mapping_t button_map[] = {
    {GAME_EVENT_NUKE_LAUNCHED, "atom", NUKE_TYPE_ATOM},
    {GAME_EVENT_NUKE_LAUNCHED, "hydro", NUKE_TYPE_HYDRO},
    {GAME_EVENT_NUKE_LAUNCHED, "mirv", NUKE_TYPE_MIRV}
};

// Helper to update LED state based on active outgoing nuke count
static void update_nuke_button_led_state(uint8_t led_index, nuke_type_t nuke_type) {
    uint8_t count = nuke_tracker_get_active_count(nuke_type, NUKE_DIR_OUTGOING);
    
    if (count > 0) {
        // At least one nuke in flight - turn LED on (solid)
        module_io_set_nuke_led(led_index, true);
        ESP_LOGD(TAG, "LED %d ON (%d nukes in flight)", led_index, count);
    } else {
        // No nukes in flight - turn LED off
        module_io_set_nuke_led(led_index, false);
        ESP_LOGD(TAG, "LED %d OFF (all resolved)", led_index);
    }
}

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
        ws_handlers_send_event(&game_event);
        
        return true;
    }
    
    // Handle nuke launch events (for LED feedback) - track and turn on LED
    // Now all nukes use unified NUKE_LAUNCHED event with nukeType in data
    if (event->type == GAME_EVENT_NUKE_LAUNCHED) {
        
        // Parse nuke type from event data to determine LED and tracker type
        // Expected JSON: {"nukeType": "atom"|"hydro"|"mirv", ...}
        uint8_t led_index = 0;
        nuke_type_t nuke_type = NUKE_TYPE_ATOM;
        
        // Simple string search for nuke type in data
        if (event->data && strstr(event->data, "\"nukeType\":\"hydro\"") != NULL) {
            led_index = 1;
            nuke_type = NUKE_TYPE_HYDRO;
        } else if (event->data && strstr(event->data, "\"nukeType\":\"mirv\"") != NULL) {
            led_index = 2;
            nuke_type = NUKE_TYPE_MIRV;
        } else {
            // Default to atom or if nukeType is "atom"
            led_index = 0;
            nuke_type = NUKE_TYPE_ATOM;
        }
        
        uint32_t unit_id = ots_parse_unit_id(event->data);
        ESP_LOGI(TAG, "Nuke launched: %s (LED %d, unit=%lu)", 
                event_type_to_string(event->type), led_index, (unsigned long)unit_id);
        
        if (unit_id > 0) {
            // Register outgoing nuke in tracker
            nuke_tracker_register_launch(unit_id, nuke_type, NUKE_DIR_OUTGOING);
            // Update LED state
            update_nuke_button_led_state(led_index, nuke_type);
        }
        
        return true;
    }
    
    // Handle explosion/interception events - resolve nuke and update LED
    if (event->type == GAME_EVENT_NUKE_EXPLODED || 
        event->type == GAME_EVENT_NUKE_INTERCEPTED) {
        
        bool exploded = (event->type == GAME_EVENT_NUKE_EXPLODED);
        uint32_t unit_id = ots_parse_unit_id(event->data);
        
        ESP_LOGI(TAG, "Nuke %s (unit=%lu)", 
                exploded ? "exploded" : "intercepted",
                (unsigned long)unit_id);
        
        if (unit_id > 0) {
            // Try to resolve the nuke (might be incoming or outgoing)
            nuke_tracker_resolve_nuke(unit_id, exploded);
            
            // Update all button LED states
            update_nuke_button_led_state(0, NUKE_TYPE_ATOM);
            update_nuke_button_led_state(1, NUKE_TYPE_HYDRO);
            update_nuke_button_led_state(2, NUKE_TYPE_MIRV);
        }
        
        return true;
    }
    
    // Handle WebSocket disconnect - visual feedback
    if (event->type == INTERNAL_EVENT_WS_DISCONNECTED) {
        ESP_LOGW(TAG, "WebSocket disconnected - showing visual feedback");
        
        // If any nukes are active, blink them rapidly to show connection loss
        for (int i = 0; i < 3; i++) {
            nuke_type_t nuke_type = (nuke_type_t)i;
            uint8_t count = nuke_tracker_get_active_count(nuke_type, NUKE_DIR_OUTGOING);
            if (count > 0) {
                // Fast blink (200ms) to indicate disconnect state
                led_command_t cmd = {
                    .type = LED_TYPE_NUKE,
                    .index = i,
                    .effect = LED_EFFECT_BLINK,
                    .duration_ms = 0,  // Infinite until reconnect
                    .blink_rate_ms = 200  // Fast blink
                };
                
                if (led_controller_send_command(&cmd)) {
                    ESP_LOGI(TAG, "Nuke LED %d fast blinking (connection lost, %d active)", i, count);
                }
            }
        }
        
        return true;
    }
    
    // Handle WebSocket reconnect - restore normal LED state
    if (event->type == INTERNAL_EVENT_WS_CONNECTED) {
        ESP_LOGI(TAG, "WebSocket reconnected - restoring LED state");
        
        // Restore normal LED state based on active nukes
        for (int i = 0; i < 3; i++) {
            update_nuke_button_led_state(i, (nuke_type_t)i);
        }
        
        return true;
    }
    
    // Handle game end - clear all tracking
    if (event->type == GAME_EVENT_GAME_END) {
        
        ESP_LOGI(TAG, "Game ended - clearing nuke tracking");
        nuke_tracker_clear_all();
        
        // Turn off all LEDs
        for (int i = 0; i < 3; i++) {
            module_io_set_nuke_led(i, false);
        }
        
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
