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
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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

    // Waiting-for-connection scan animation (line 2)
    uint8_t conn_anim_frame;
    uint64_t conn_anim_last_ms;
    char conn_last_line2[LCD_COLS + 1];

    // Lobby scan animation (line 2)
    uint8_t lobby_anim_frame;
    uint64_t lobby_anim_last_ms;
    char lobby_last_line2[LCD_COLS + 1];
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
    // Line 2 is animated by system_status_update(); start with frame 0.
    lcd_write_string(" Connection   .  ");
}

static void display_lobby(void) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string(" Connected!     ");
    // Lobby line 2 is animated by system_status_update(); start with frame 0.
    lcd_set_cursor(0, 1);
    lcd_write_string(" Waiting Game.  ");
}

static void conn_anim_write_line2_if_changed(uint8_t frame) {
    // Keep the message stable and move a single dot across the last 3 columns.
    // 16 chars total: 13-char prefix + 3-char scan suffix.
    // Single moving dot (no fixed dots).
    // Frame 0 matches display_waiting_connection(): " Connection   .  "
    static const char *prefix = " Connection   ";
    static const char *suffixes[] = {
        ".  ",
        " . ",
        "  .",
        " . "
    };

    char line2[LCD_COLS + 1];
    memset(line2, ' ', LCD_COLS);
    line2[LCD_COLS] = '\0';

    memcpy(line2, prefix, strlen(prefix));
    const char *suffix = suffixes[frame % (sizeof(suffixes) / sizeof(suffixes[0]))];
    memcpy(line2 + (LCD_COLS - 3), suffix, 3);

    if (strncmp(module_state.conn_last_line2, line2, LCD_COLS) != 0) {
        (void)lcd_write_line(1, line2);
        memcpy(module_state.conn_last_line2, line2, LCD_COLS + 1);
    }
}

static void lobby_anim_write_line2_if_changed(uint8_t frame) {
    // Keep the message stable and move a single dot across the last 3 columns.
    // 16 chars total: 13-char prefix + 3-char scan suffix.
    static const char *prefix = " Waiting Game";
    static const char *suffixes[] = {
        ".  ",
        " . ",
        "  .",
        " . "
    };

    char line2[LCD_COLS + 1];
    memset(line2, ' ', LCD_COLS);
    line2[LCD_COLS] = '\0';

    memcpy(line2, prefix, strlen(prefix));
    const char *suffix = suffixes[frame % (sizeof(suffixes) / sizeof(suffixes[0]))];
    memcpy(line2 + (LCD_COLS - 3), suffix, 3);

    if (strncmp(module_state.lobby_last_line2, line2, LCD_COLS) != 0) {
        (void)lcd_write_line(1, line2);
        memcpy(module_state.lobby_last_line2, line2, LCD_COLS + 1);
    }
}

static void display_choose_spawn(void) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("   Spawning...  ");
    lcd_set_cursor(0, 1);
    lcd_write_string(" Get Ready!     ");
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

    // Ensure the LCD is initialized before we attempt to draw anything.
    esp_err_t ret = lcd_init(LCD_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD at 0x%02X: %s", LCD_I2C_ADDR, esp_err_to_name(ret));
        return ret;
    }
    
    // Show splash screen on boot
    display_splash();

    // After a short splash, move to the normal "waiting for connection" screen.
    // This prevents the LCD from sitting on "Booting..." forever if there are
    // no events yet.
    vTaskDelay(pdMS_TO_TICKS(1200));
    
    module_state.initialized = true;
    module_state.display_active = true;  // We start with control
    module_state.display_dirty = true;
    
    ESP_LOGI(TAG, "System status module initialized");
    return ESP_OK;
}

static esp_err_t system_status_update(void) {
    if (!module_state.initialized) return ESP_OK;

    // If the game has transitioned to IN_GAME, ensure we yield LCD control even
    // if SystemStatus isn't about to redraw.
    if (module_state.display_active && module_state.ws_connected && !module_state.show_game_end) {
        if (game_state_get_phase() == GAME_PHASE_IN_GAME) {
            module_state.display_active = false;
        }
    }

    // Update display if dirty and we're in control
    if (module_state.display_dirty && module_state.display_active) {
        game_phase_t phase = game_state_get_phase();

        // Check connection status first - if not connected, always show waiting screen
        if (!module_state.ws_connected) {
            display_waiting_connection();
            module_state.show_game_end = false;
            module_state.display_dirty = false;
            return ESP_OK;
        }

        // Show game end screen if flag is set
        if (module_state.show_game_end) {
            display_game_end(module_state.player_won);
            module_state.display_dirty = false;
            return ESP_OK;
        }
        
        // Connected - show screen based on game phase
        switch (phase) {
            case GAME_PHASE_LOBBY:
                display_lobby();
                break;
            case GAME_PHASE_SPAWNING:
                display_choose_spawn();
                break;
            case GAME_PHASE_IN_GAME:
                // Yield control - don't touch LCD during game
                module_state.display_active = false;
                break;
            case GAME_PHASE_WON:
            case GAME_PHASE_LOST:
            case GAME_PHASE_ENDED:
                // Show game end screen until userscript disconnect/reconnect resets state.
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

    // Waiting-for-connection scan animation: when NOT connected, update line 2 periodically.
    if (module_state.display_active && !module_state.ws_connected) {
        const uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000);
        if (module_state.conn_anim_last_ms == 0 || (now_ms - module_state.conn_anim_last_ms) >= 250) {
            module_state.conn_anim_last_ms = now_ms;
            module_state.conn_anim_frame = (module_state.conn_anim_frame + 1) % 4;
            conn_anim_write_line2_if_changed(module_state.conn_anim_frame);
        }
        return ESP_OK;
    }

    // Lobby scan animation: when connected and in LOBBY, update line 2 periodically.
    if (module_state.display_active && module_state.ws_connected && !module_state.show_game_end) {
        if (game_state_get_phase() == GAME_PHASE_LOBBY) {
            const uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000);
            if (module_state.lobby_anim_last_ms == 0 || (now_ms - module_state.lobby_anim_last_ms) >= 250) {
                module_state.lobby_anim_last_ms = now_ms;
                module_state.lobby_anim_frame = (module_state.lobby_anim_frame + 1) % 4;
                lobby_anim_write_line2_if_changed(module_state.lobby_anim_frame);
            }
        }
    }
    
    return ESP_OK;
}

static bool system_status_handle_event(const internal_event_t *event) {
    if (!module_state.initialized || !event) return false;
    
    switch (event->type) {
        case INTERNAL_EVENT_WS_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected - showing lobby screen");
            // Userscript reconnect usually means the browser page reloaded and any
            // previous game session is no longer relevant. Reset the game phase so
            // we don't keep showing stale end-game screens.
            game_state_reset();
            module_state.ws_connected = true;
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;  // Clear game end flag
            module_state.lobby_anim_frame = 0;
            module_state.lobby_anim_last_ms = 0;
            memset(module_state.lobby_last_line2, 0, sizeof(module_state.lobby_last_line2));
            module_state.conn_anim_frame = 0;
            module_state.conn_anim_last_ms = 0;
            memset(module_state.conn_last_line2, 0, sizeof(module_state.conn_last_line2));
            return true;
            
        case INTERNAL_EVENT_WS_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected - showing waiting screen");
            // Clear stale game session state on disconnect.
            game_state_reset();
            module_state.ws_connected = false;
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;
            module_state.lobby_anim_frame = 0;
            module_state.lobby_anim_last_ms = 0;
            memset(module_state.lobby_last_line2, 0, sizeof(module_state.lobby_last_line2));
            module_state.conn_anim_frame = 0;
            module_state.conn_anim_last_ms = 0;
            memset(module_state.conn_last_line2, 0, sizeof(module_state.conn_last_line2));
            return true;
            
        case GAME_EVENT_GAME_START:
            ESP_LOGI(TAG, "Game started - yielding LCD control to troops module");
            module_state.display_active = false;
            module_state.show_game_end = false;  // Clear any previous game end
            module_state.lobby_anim_last_ms = 0;
            return true;

        case GAME_EVENT_GAME_SPAWNING:
            ESP_LOGI(TAG, "Game spawning - showing choose spawn screen");
            module_state.display_active = true;
            module_state.display_dirty = true;
            module_state.show_game_end = false;
            module_state.lobby_anim_last_ms = 0;
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
