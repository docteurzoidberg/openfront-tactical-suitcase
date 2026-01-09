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
#include "i2c_handler.h"
#include "game_state_manager.h"
#include "protocol.h"
#include "network_manager.h"
#include "http_server.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *TAG = "OTS_SYS_STATUS";

// Module state
// Animation state (shared for all animated screens)
typedef struct {
    uint8_t frame;
    uint64_t last_update_ms;
    char last_line2[LCD_COLS + 1];
} animation_state_t;

typedef struct {
    bool initialized;
    bool lcd_available;   // Is LCD hardware present?
    bool display_active;  // Are we controlling the display?
    bool display_dirty;
    bool player_won;      // true = victory, false = defeat
    bool show_game_end;   // Show game end screen
    bool ws_connected;    // WebSocket connection status
    animation_state_t animation;
} system_status_state_t;

static system_status_state_t module_state = {0};

// ============================================================================
// Display Functions
// ============================================================================

// Helper to display simple two-line screens
static void display_two_lines(const char *line1, const char *line2) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string(line1);
    lcd_set_cursor(0, 1);
    lcd_write_string(line2);
}

static void display_splash(void) {
    display_two_lines("  OTS Firmware  ", "  Booting...    ");
}

static void display_captive_portal(void) {
    display_two_lines("   Setup WiFi   ", "  Read Manual   ");
}

static void display_waiting_connection(void) {
    // Line 2 animated by system_status_update(); starts at frame 0
    display_two_lines(" Waiting for    ", " Connection   .  ");
}

static void display_lobby(void) {
    // Line 2 animated by system_status_update(); starts at frame 0
    display_two_lines(" Connected!     ", " Waiting Game.  ");
}

// Unified animation helper for all scan-dot animations
static void animate_scan_line2(const char *prefix, uint8_t frame) {
    static const char *suffixes[] = {".  ", " . ", "  .", " . "};
    
    char line2[LCD_COLS + 1];
    memset(line2, ' ', LCD_COLS);
    line2[LCD_COLS] = '\0';

    memcpy(line2, prefix, strlen(prefix));
    const char *suffix = suffixes[frame % 4];
    memcpy(line2 + (LCD_COLS - 3), suffix, 3);

    if (strncmp(module_state.animation.last_line2, line2, LCD_COLS) != 0) {
        lcd_write_line(1, line2);
        memcpy(module_state.animation.last_line2, line2, LCD_COLS + 1);
    }
}

// Update animation if enough time has passed
static bool update_animation_if_needed(const char *prefix) {
    const uint64_t now_ms = esp_timer_get_time() / 1000;
    if (module_state.animation.last_update_ms == 0 || 
        (now_ms - module_state.animation.last_update_ms) >= 250) {
        module_state.animation.last_update_ms = now_ms;
        module_state.animation.frame = (module_state.animation.frame + 1) % 4;
        animate_scan_line2(prefix, module_state.animation.frame);
        return true;
    }
    return false;
}

static void reset_animation_state(void) {
    module_state.animation.frame = 0;
    module_state.animation.last_update_ms = 0;
    memset(module_state.animation.last_line2, 0, sizeof(module_state.animation.last_line2));
}

static void display_choose_spawn(void) {
    display_two_lines("   Spawning...  ", " Get Ready!     ");
}

static void display_game_end(bool victory) {
    const char *line1 = victory ? "   VICTORY!     " : "    DEFEAT      ";
    display_two_lines(line1, " Good Game!     ");
}

// ============================================================================
// Hardware Module Interface Implementation
// ============================================================================

static esp_err_t system_status_init(void) {
    ESP_LOGI(TAG, "Initializing system status module...");

    // Try to initialize LCD, but continue if it's not present
    esp_err_t ret = lcd_init(ots_i2c_bus_get(), LCD_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD not detected at 0x%02X: %s - continuing without display", 
                 LCD_I2C_ADDR, esp_err_to_name(ret));
        module_state.lcd_available = false;
    } else {
        ESP_LOGI(TAG, "LCD initialized successfully at 0x%02X", LCD_I2C_ADDR);
        module_state.lcd_available = true;
        
        // Show splash screen on boot
        display_splash();

        // After a short splash, move to the normal "waiting for connection" screen.
        // This prevents the LCD from sitting on "Booting..." forever if there are
        // no events yet.
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    
    module_state.initialized = true;
    module_state.display_active = true;  // We start with control
    module_state.display_dirty = true;
    
    ESP_LOGI(TAG, "System status module initialized (LCD: %s)", 
             module_state.lcd_available ? "present" : "not present");
    return ESP_OK;
}

static esp_err_t system_status_update(void) {
    if (!module_state.initialized) return ESP_OK;
    
    // Skip all LCD operations if hardware not present
    if (!module_state.lcd_available) return ESP_OK;

    // Auto-yield LCD control when game starts
    if (module_state.display_active && module_state.ws_connected && 
        !module_state.show_game_end && game_state_get_phase() == GAME_PHASE_IN_GAME) {
        module_state.display_active = false;
    }

    // Handle display updates when we're in control and display is dirty
    if (module_state.display_dirty && module_state.display_active) {
        // Priority 1: Captive portal mode
        if (network_manager_is_portal_mode()) {
            display_captive_portal();
            module_state.show_game_end = false;
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // Priority 2: Wait for WSS server to start (keep splash)
        if (!http_server_is_started()) {
            return ESP_OK;  // Keep splash, don't clear display_dirty
        }
        
        // Priority 3: Not connected - show waiting screen
        if (!module_state.ws_connected) {
            display_waiting_connection();
            module_state.show_game_end = false;
            module_state.display_dirty = false;
            return ESP_OK;
        }

        // Priority 4: Game end screen takes precedence
        if (module_state.show_game_end) {
            display_game_end(module_state.player_won);
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // Priority 5: Connected - show screen based on game phase
        game_phase_t phase = game_state_get_phase();
        switch (phase) {
            case GAME_PHASE_LOBBY:
                display_lobby();
                break;
            case GAME_PHASE_SPAWNING:
                display_choose_spawn();
                break;
            case GAME_PHASE_IN_GAME:
                module_state.display_active = false;  // Yield to troops module
                break;
            case GAME_PHASE_WON:
            case GAME_PHASE_LOST:
            case GAME_PHASE_ENDED:
                module_state.show_game_end = true;
                module_state.player_won = (phase == GAME_PHASE_WON);
                display_game_end(module_state.player_won);
                break;
            default:
                display_waiting_connection();
                break;
        }
        
        module_state.display_dirty = false;
    }

    // Handle animations (only when display is active)
    if (!module_state.display_active) return ESP_OK;
    
    // Animate waiting screen (not in portal mode)
    if (!module_state.ws_connected && !network_manager_is_portal_mode()) {
        update_animation_if_needed(" Connection   ");
        return ESP_OK;
    }

    // Animate lobby screen
    if (module_state.ws_connected && !module_state.show_game_end && 
        game_state_get_phase() == GAME_PHASE_LOBBY) {
        update_animation_if_needed(" Waiting Game");
    }
    
    return ESP_OK;
}

static bool system_status_handle_event(const internal_event_t *event) {
    if (!module_state.initialized || !event) return false;
    
    switch (event->type) {
        case INTERNAL_EVENT_WS_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected - showing lobby screen");
            game_state_reset();
            module_state.ws_connected = true;
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;
            reset_animation_state();
            return true;
            
        case INTERNAL_EVENT_WS_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected - showing waiting screen");
            game_state_reset();
            module_state.ws_connected = false;
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;
            reset_animation_state();
            return true;
            
        case GAME_EVENT_GAME_START:
            ESP_LOGI(TAG, "Game started - yielding LCD control to troops module");
            module_state.display_active = false;
            module_state.show_game_end = false;
            return true;

        case GAME_EVENT_GAME_SPAWNING:
            ESP_LOGI(TAG, "Game spawning - showing choose spawn screen");
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;
            return true;
            
        case GAME_EVENT_GAME_END: {
            // Parse victory status: {"victory": true/false/null}
            bool victory = event->data && strstr(event->data, "\"victory\":true");
            bool defeat = event->data && strstr(event->data, "\"victory\":false");
            
            module_state.display_active = true;
            module_state.display_dirty = true;
            
            if (victory || defeat) {
                ESP_LOGI(TAG, "Game ended - %s", victory ? "VICTORY!" : "Defeat");
                module_state.show_game_end = true;
                module_state.player_won = victory;
            } else {
                ESP_LOGI(TAG, "Game ended (unknown outcome) - returning to lobby");
                module_state.show_game_end = false;
            }
            return true;
        }
            
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
    .enabled = true,
    .init = system_status_init,
    .update = system_status_update,
    .handle_event = system_status_handle_event,
    .get_status = system_status_get_status,
    .shutdown = system_status_shutdown
};

const hardware_module_t* system_status_module_get(void) {
    return &system_status_module;
}

void system_status_refresh_display(void) {
    ESP_LOGI(TAG, "*** system_status_refresh_display() called, initialized=%d ***", module_state.initialized);
    if (module_state.initialized) {
        module_state.display_dirty = true;
        module_state.display_active = true;
        ESP_LOGI(TAG, "Display marked as dirty and active");
    }
}
