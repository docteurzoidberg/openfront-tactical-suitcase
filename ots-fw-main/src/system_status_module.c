/**
 * @file system_status_module.c
 * @brief System Status Display Module Implementation
 * 
 * Manages LCD display for system status screens:
 * - Splash screen on boot
 * - Waiting for connection (WS disconnected)
 * - Lobby screen (WS connected, no game)
 * 
 * Yields LCD control to troops_module during active gameplay.
 */

#include "system_status_module.h"
#include "lcd_driver.h"
#include "game_state.h"
#include "protocol.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "SysStatus";

// Module state
typedef struct {
    bool initialized;
    bool display_active;  // Are we controlling the display?
    bool display_dirty;
    bool player_won;      // true = victory, false = defeat
    bool show_game_end;   // Show game end screen
    bool ws_connected;    // WebSocket connection status
} system_status_state_t;

static system_status_state_t module_state = {0};

// ============================================================================
// Display Functions
// ============================================================================

static void display_splash(void) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("  OTS Firmware  ");
    lcd_set_cursor(0, 1);
    lcd_write_string("  Booting...    ");
}

static void display_waiting_connection(void) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string(" Waiting for    ");
    lcd_set_cursor(0, 1);
    lcd_write_string(" Connection...  ");
}

static void display_lobby(void) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string(" Connected!     ");
    lcd_set_cursor(0, 1);
    lcd_write_string(" Waiting Game...");
}

static void display_game_end(bool victory) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    if (victory) {
        lcd_write_string("   VICTORY!     ");
    } else {
        lcd_write_string("    DEFEAT      ");
    }
    lcd_set_cursor(0, 1);
    lcd_write_string(" Good Game!     ");
}

// ============================================================================
// Hardware Module Interface Implementation
// ============================================================================

static esp_err_t system_status_init(void) {
    ESP_LOGI(TAG, "Initializing system status module...");
    
    // Show splash screen on boot
    display_splash();
    
    module_state.initialized = true;
    module_state.display_active = true;  // We start with control
    module_state.display_dirty = false;
    
    ESP_LOGI(TAG, "System status module initialized");
    return ESP_OK;
}

static esp_err_t system_status_update(void) {
    if (!module_state.initialized) return ESP_OK;
    
    // Update display if dirty and we're in control
    if (module_state.display_dirty && module_state.display_active) {
        game_phase_t phase = game_state_get_phase();
        
        // Show game end screen if flag is set
        if (module_state.show_game_end) {
            display_game_end(module_state.player_won);
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // Check connection status first - if not connected, always show waiting screen
        if (!module_state.ws_connected) {
            display_waiting_connection();
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // Connected - show screen based on game phase
        switch (phase) {
            case GAME_PHASE_LOBBY:
                display_lobby();
                break;
            case GAME_PHASE_SPAWNING:
                display_lobby();  // Still in lobby UI during spawn
                break;
            case GAME_PHASE_IN_GAME:
                // Yield control - don't touch LCD during game
                module_state.display_active = false;
                break;
            case GAME_PHASE_WON:
            case GAME_PHASE_LOST:
            case GAME_PHASE_ENDED:
                // Show game end screen for 5 seconds, then return to lobby
                module_state.show_game_end = true;
                module_state.player_won = (phase == GAME_PHASE_WON);
                display_game_end(module_state.player_won);
                module_state.display_active = true;
                break;
            default:
                display_waiting_connection();
                break;
        }
        
        module_state.display_dirty = false;
    }
    
    return ESP_OK;
}

static bool system_status_handle_event(const internal_event_t *event) {
    if (!module_state.initialized || !event) return false;
    
    switch (event->type) {
        case INTERNAL_EVENT_WS_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected - showing lobby screen");
            module_state.ws_connected = true;
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;  // Clear game end flag
            return true;
            
        case INTERNAL_EVENT_WS_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected - showing waiting screen");
            module_state.ws_connected = false;
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;
            return true;
            
        case GAME_EVENT_GAME_START:
            ESP_LOGI(TAG, "Game started - yielding LCD control to troops module");
            module_state.display_active = false;
            module_state.show_game_end = false;  // Clear any previous game end
            return true;
            
        case GAME_EVENT_GAME_END:
            // Parse victory status from event data
            // Expected JSON: {"victory": true/false/null}
            // For now, we'll check the data field for "victory" boolean
            // TODO: Implement proper JSON parsing when cJSON is available
            
            // Simple string search for victory indication
            bool victory_found = (event->data && strstr(event->data, "\"victory\":true") != NULL);
            bool defeat_found = (event->data && strstr(event->data, "\"victory\":false") != NULL);
            
            if (victory_found) {
                ESP_LOGI(TAG, "Game ended - VICTORY!");
                module_state.display_active = true;
                module_state.show_game_end = true;
                module_state.player_won = true;
                module_state.display_dirty = true;
            } else if (defeat_found) {
                ESP_LOGI(TAG, "Game ended - Defeat");
                module_state.display_active = true;
                module_state.show_game_end = true;
                module_state.player_won = false;
                module_state.display_dirty = true;
            } else {
                ESP_LOGI(TAG, "Game ended (unknown outcome) - reclaiming LCD control");
                module_state.display_active = true;
                module_state.display_dirty = true;
                module_state.show_game_end = false;
            }
            return true;
            
        default:
            return false;
    }
}

static void system_status_get_status(module_status_t *status) {
    if (!status) return;
    
    status->initialized = module_state.initialized;
    status->operational = module_state.display_active;
    status->error_count = 0;
    snprintf(status->last_error, sizeof(status->last_error), 
             "display_%s", module_state.display_active ? "active" : "yielded");
}

static esp_err_t system_status_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down system status module");
    
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("  Shutting down ");
    
    module_state.initialized = false;
    return ESP_OK;
}

// ============================================================================
// Module Registration
// ============================================================================

static const hardware_module_t system_status_module = {
    .name = "SystemStatus",
    .init = system_status_init,
    .update = system_status_update,
    .handle_event = system_status_handle_event,
    .get_status = system_status_get_status,
    .shutdown = system_status_shutdown
};

const hardware_module_t* system_status_module_get(void) {
    return &system_status_module;
}
