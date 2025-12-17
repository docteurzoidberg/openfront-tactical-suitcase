#include "alert_module.h"
#include "module_io.h"
#include "led_controller.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ALERT_MOD";

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

// Handle events
static bool alert_module_handle_event(const internal_event_t *event) {
    bool handled = false;
    
    switch (event->type) {
        // Incoming attack alerts
        case GAME_EVENT_ALERT_ATOM:
            ESP_LOGI(TAG, "Atom alert!");
            led_controller_alert_on(1, 10000);  // Alert LED 1 = Atom, 10s
            led_controller_alert_on(0, 10000);  // Warning LED
            handled = true;
            break;
            
        case GAME_EVENT_ALERT_HYDRO:
            ESP_LOGI(TAG, "Hydro alert!");
            led_controller_alert_on(2, 10000);  // Alert LED 2 = Hydro, 10s
            led_controller_alert_on(0, 10000);  // Warning LED
            handled = true;
            break;
            
        case GAME_EVENT_ALERT_MIRV:
            ESP_LOGI(TAG, "MIRV alert!");
            led_controller_alert_on(3, 10000);  // Alert LED 3 = MIRV, 10s
            led_controller_alert_on(0, 10000);  // Warning LED
            handled = true;
            break;
            
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
            
        // Explosion/interception events (turn off alerts)
        case GAME_EVENT_NUKE_EXPLODED:
        case GAME_EVENT_NUKE_INTERCEPTED:
            ESP_LOGI(TAG, "Nuke resolved - clearing all nuke alerts");
            module_io_set_alert_led(1, false);  // Atom
            module_io_set_alert_led(2, false);  // Hydro
            module_io_set_alert_led(3, false);  // MIRV
            handled = true;
            break;
            
        // General warning LED based on game phase
        case GAME_EVENT_GAME_START:
            ESP_LOGI(TAG, "Game started - enabling warning LED");
            module_io_set_alert_led(0, true);  // Turn on warning LED
            handled = true;
            break;
            
        case GAME_EVENT_GAME_END:
        case GAME_EVENT_WIN:
        case GAME_EVENT_LOOSE:
            ESP_LOGI(TAG, "Game stopped - disabling all alerts");
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
