#include "alert_module.h"
#include "module_io.h"
#include "led_controller.h"
#include "nuke_tracker.h"
#include "ots_common.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "OTS_ALERT";

static module_status_t status = {0};

// Module initialization
static esp_err_t alert_module_init(void) {
    ESP_LOGI(TAG, "Initializing alert module...");
    
    // Pins are already configured globally (board 1 = output)
    // Just verify we can write
    
    // Turn off all alert LEDs initially
    for (int i = 0; i < 6; i++) {
        module_io_set_alert_led(i, false);
    }
    
    // Initialize nuke tracker
    nuke_tracker_init();
    
    status.initialized = true;
    status.operational = true;
    status.error_count = 0;
    
    ESP_LOGI(TAG, "Alert module initialized (6 LEDs)");
    return ESP_OK;
}

// Module update (periodic)
static esp_err_t alert_module_update(void) {
    // Nothing to do here - LED blinking handled by led_controller
    return ESP_OK;
}

// Helper to update LED state based on active nuke count
static void update_nuke_led_state(uint8_t led_index, nuke_type_t nuke_type) {
    uint8_t count = nuke_tracker_get_active_count(nuke_type, NUKE_DIR_INCOMING);
    
    if (count > 0) {
        // At least one nuke in flight - turn LED on
        module_io_set_alert_led(led_index, true);
        ESP_LOGD(TAG, "LED %d ON (%d nukes active)", led_index, count);
    } else {
        // No nukes in flight - turn LED off
        module_io_set_alert_led(led_index, false);
        ESP_LOGD(TAG, "LED %d OFF (all resolved)", led_index);
    }
    
    // Update warning LED (on if any alert LED is active)
    uint8_t atom_count = nuke_tracker_get_active_count(NUKE_TYPE_ATOM, NUKE_DIR_INCOMING);
    uint8_t hydro_count = nuke_tracker_get_active_count(NUKE_TYPE_HYDRO, NUKE_DIR_INCOMING);
    uint8_t mirv_count = nuke_tracker_get_active_count(NUKE_TYPE_MIRV, NUKE_DIR_INCOMING);
    
    if (atom_count > 0 || hydro_count > 0 || mirv_count > 0) {
        module_io_set_alert_led(0, true);  // Warning LED
    } else {
        module_io_set_alert_led(0, false);
    }
}

// Handle events
static bool alert_module_handle_event(const internal_event_t *event) {
    bool handled = false;
    
    switch (event->type) {
        // Incoming attack alerts - track nuke and turn on LED
        case GAME_EVENT_ALERT_NUKE: {
            uint32_t unit_id = ots_parse_unit_id(event->data);
            ESP_LOGI(TAG, "Atom alert! (unit=%lu)", (unsigned long)unit_id);
            
            if (unit_id > 0) {
                nuke_tracker_register_launch(unit_id, NUKE_TYPE_ATOM, NUKE_DIR_INCOMING);
                update_nuke_led_state(1, NUKE_TYPE_ATOM);
            }
            handled = true;
            break;
        }
            
        case GAME_EVENT_ALERT_HYDRO: {
            uint32_t unit_id = ots_parse_unit_id(event->data);
            ESP_LOGI(TAG, "Hydro alert! (unit=%lu)", (unsigned long)unit_id);
            
            if (unit_id > 0) {
                nuke_tracker_register_launch(unit_id, NUKE_TYPE_HYDRO, NUKE_DIR_INCOMING);
                update_nuke_led_state(2, NUKE_TYPE_HYDRO);
            }
            handled = true;
            break;
        }
            
        case GAME_EVENT_ALERT_MIRV: {
            uint32_t unit_id = ots_parse_unit_id(event->data);
            ESP_LOGI(TAG, "MIRV alert! (unit=%lu)", (unsigned long)unit_id);
            
            if (unit_id > 0) {
                nuke_tracker_register_launch(unit_id, NUKE_TYPE_MIRV, NUKE_DIR_INCOMING);
                update_nuke_led_state(3, NUKE_TYPE_MIRV);
            }
            handled = true;
            break;
        }
            
        case GAME_EVENT_ALERT_LAND:
            ESP_LOGI(TAG, "Land attack alert!");
            led_controller_alert_on(4, 15000);  // Alert LED 4 = Land, 15s
            led_controller_alert_on(0, 15000);  // Warning LED (longer)
            handled = true;
            break;
            
        case GAME_EVENT_ALERT_NAVAL:
            ESP_LOGI(TAG, "Naval attack alert!");
            led_controller_alert_on(5, 15000);  // Alert LED 5 = Naval, 15s
            led_controller_alert_on(0, 15000);  // Warning LED (longer)
            handled = true;
            break;
            
        // Explosion/interception events - resolve nuke and update LED
        case GAME_EVENT_NUKE_EXPLODED:
        case GAME_EVENT_NUKE_INTERCEPTED: {
            bool exploded = (event->type == GAME_EVENT_NUKE_EXPLODED);
            uint32_t unit_id = ots_parse_unit_id(event->data);
            
            ESP_LOGI(TAG, "Nuke %s (unit=%lu)", 
                    exploded ? "exploded" : "intercepted",
                    (unsigned long)unit_id);
            
            if (unit_id > 0) {
                // Try to resolve the nuke (might be incoming or outgoing)
                nuke_tracker_resolve_nuke(unit_id, exploded);
                
                // Update all nuke alert LED states
                update_nuke_led_state(1, NUKE_TYPE_ATOM);
                update_nuke_led_state(2, NUKE_TYPE_HYDRO);
                update_nuke_led_state(3, NUKE_TYPE_MIRV);
            }
            
            handled = true;
            break;
        }
            
        // General warning LED based on game phase
        case GAME_EVENT_GAME_START:
            ESP_LOGI(TAG, "Game started - enabling warning LED");
            module_io_set_alert_led(0, true);  // Turn on warning LED
            handled = true;
            break;
        
        // WebSocket disconnect - visual feedback
        case INTERNAL_EVENT_WS_DISCONNECTED: {
            ESP_LOGW(TAG, "WebSocket disconnected - showing warning");
            
            // Check if any alerts are active
            uint8_t atom_count = nuke_tracker_get_active_count(NUKE_TYPE_ATOM, NUKE_DIR_INCOMING);
            uint8_t hydro_count = nuke_tracker_get_active_count(NUKE_TYPE_HYDRO, NUKE_DIR_INCOMING);
            uint8_t mirv_count = nuke_tracker_get_active_count(NUKE_TYPE_MIRV, NUKE_DIR_INCOMING);
            
            // Create blink command for warning LED
            led_command_t cmd = {
                .type = LED_TYPE_ALERT,
                .index = 0,  // Warning LED
                .effect = LED_EFFECT_BLINK,
                .duration_ms = 0,  // Infinite until reconnect
                .blink_rate_ms = (atom_count > 0 || hydro_count > 0 || mirv_count > 0) ? 100 : 500
            };
            
            if (led_controller_send_command(&cmd)) {
                ESP_LOGI(TAG, "Warning LED blinking at %lums (connection lost, %s)",
                         (unsigned long)cmd.blink_rate_ms,
                         cmd.blink_rate_ms == 100 ? "threats active" : "no threats");
            }
            
            handled = true;
            break;
        }
        
        // WebSocket reconnect - restore normal LED state
        case INTERNAL_EVENT_WS_CONNECTED: {
            ESP_LOGI(TAG, "WebSocket reconnected - restoring alert state");
            
            // Restore alert LED states based on active incoming nukes
            update_nuke_led_state(1, NUKE_TYPE_ATOM);
            update_nuke_led_state(2, NUKE_TYPE_HYDRO);
            update_nuke_led_state(3, NUKE_TYPE_MIRV);
            
            handled = true;
            break;
        }
            
        case GAME_EVENT_GAME_END:
            ESP_LOGI(TAG, "Game ended - disabling all alerts");
            // Clear all tracked nukes
            nuke_tracker_clear_all();
            // Turn off all alerts
            for (int i = 0; i < 6; i++) {
                module_io_set_alert_led(i, false);
            }
            handled = true;
            break;
            
        default:
            break;
    }
    
    return handled;
}

// Get module status
static void alert_module_get_status(module_status_t *out_status) {
    if (out_status) {
        memcpy(out_status, &status, sizeof(module_status_t));
    }
}

// Shutdown module
static esp_err_t alert_module_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down alert module...");
    
    // Turn off all LEDs
    for (int i = 0; i < 6; i++) {
        module_io_set_alert_led(i, false);
    }
    
    status.operational = false;
    return ESP_OK;
}

// Module definition
hardware_module_t alert_module = {
    .name = "Alert Module",
    .enabled = true,
    .init = alert_module_init,
    .update = alert_module_update,
    .handle_event = alert_module_handle_event,
    .get_status = alert_module_get_status,
    .shutdown = alert_module_shutdown
};
