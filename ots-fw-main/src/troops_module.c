#include "troops_module.h"
#include "protocol.h"
#include "ws_handlers.h"
#include "event_dispatcher.h"
#include "led_handler.h"
#include "lcd_driver.h"
#include "adc_handler.h"
#include "game_state_manager.h"
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>
#include <cJSON.h>

static const char* TAG = "OTS_TROOPS";

// Module state
static troops_module_state_t module_state = {
    .current_troops = 0,
    .max_troops = 0,
    .slider_percent = 0,
    .last_sent_percent = 0,
    .last_slider_read = 0,
    .display_dirty = true,
    .initialized = false
};

// Latest percent as reported by the game/userscript (attackRatio * 100)
static bool s_have_game_percent = false;
static uint8_t s_game_percent = 0;

// Forward declarations
static esp_err_t troops_init(void);
static esp_err_t troops_update(void);
static bool troops_handle_event(const internal_event_t *event);
static void troops_get_status(module_status_t *status);
static esp_err_t troops_shutdown(void);

// Helper functions
static void update_troop_display(void);
static void send_percent_command(uint8_t percent);

// Module interface
static const hardware_module_t troops_module = {
    .name = "Troops",
    .enabled = true,
    .init = troops_init,
    .update = troops_update,
    .handle_event = troops_handle_event,
    .get_status = troops_get_status,
    .shutdown = troops_shutdown
};

const hardware_module_t* troops_module_get(void) {
    return &troops_module;
}

// ============================================================================
// Troop Count Formatting (Integer-only, no floating point)
// ============================================================================

void troops_format_count(uint32_t troops, char* buffer, size_t buffer_size) {
    if (troops >= 1000000000) {
        uint32_t billions = troops / 1000000000;
        uint32_t hundreds_mil = (troops % 1000000000) / 100000000;
        snprintf(buffer, buffer_size, "%lu.%luB", 
                (unsigned long)billions, (unsigned long)hundreds_mil);
    } else if (troops >= 1000000) {
        uint32_t millions = troops / 1000000;
        uint32_t hundreds_k = (troops % 1000000) / 100000;
        snprintf(buffer, buffer_size, "%lu.%luM", 
                (unsigned long)millions, (unsigned long)hundreds_k);
    } else if (troops >= 1000) {
        uint32_t thousands = troops / 1000;
        uint32_t hundreds = (troops % 1000) / 100;
        snprintf(buffer, buffer_size, "%lu.%luK", 
                (unsigned long)thousands, (unsigned long)hundreds);
    } else {
        snprintf(buffer, buffer_size, "%lu", (unsigned long)troops);
    }
}

// ============================================================================
// Troop Display Update (In-Game Only)
// ============================================================================

static void update_troop_display(void) {
    char line1[LCD_COLS + 1];
    char line2[LCD_COLS + 1];
    char current_str[8], max_str[8], calc_str[8];
    
    // Format troop counts
    troops_format_count(module_state.current_troops, current_str, sizeof(current_str));
    troops_format_count(module_state.max_troops, max_str, sizeof(max_str));
    
    // Line 1: "120K / 1.1M" (right-aligned)
    int len = snprintf(line1, sizeof(line1), "%s / %s", current_str, max_str);
    if (len > 0 && len < LCD_COLS) {
        int padding = LCD_COLS - len;
        memmove(line1 + padding, line1, len + 1);
        memset(line1, ' ', padding);
    }
    
    // Line 2: "50% (60K)" (left-aligned)
    const uint8_t display_percent = s_have_game_percent ? s_game_percent : module_state.slider_percent;
    uint32_t calculated = ((uint64_t)module_state.current_troops * display_percent) / 100;
    troops_format_count(calculated, calc_str, sizeof(calc_str));
    int len2 = snprintf(line2, sizeof(line2), "%u%% (%s)", (unsigned)display_percent, calc_str);
    if (len2 < 0) {
        line2[0] = '\0';
        len2 = 0;
    }
    if (len2 >= LCD_COLS) {
        line2[LCD_COLS] = '\0';
        len2 = LCD_COLS;
    }
    if (len2 < LCD_COLS) {
        memset(line2 + len2, ' ', LCD_COLS - len2);
        line2[LCD_COLS] = '\0';
    }
    
    // Write to LCD using optimized batched I2C
    lcd_write_line(0, line1);
    lcd_write_line(1, line2);
}



// ============================================================================
// Protocol Communication
// ============================================================================

static void send_percent_command(uint8_t percent) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "cmd");
    
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "action", "set-troops-percent");
    
    cJSON* params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "percent", percent);
    
    cJSON_AddItemToObject(payload, "params", params);
    cJSON_AddItemToObject(root, "payload", payload);
    
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ws_handlers_send_text(json_str, strlen(json_str));
        ESP_LOGI(TAG, "Sent troops percent: %d%%", percent);
        free(json_str);
    }
    
    cJSON_Delete(root);
}

// ============================================================================
// Module Interface Implementation
// ============================================================================

static esp_err_t troops_init(void) {
    ESP_LOGI(TAG, "Initializing troops module...");
    
    // Note: LCD is owned/initialized by SystemStatus at boot and this module
    // only writes during GAME_PHASE_IN_GAME.
    // ADC is initialized and scanned by adc_handler/io_task.
    
    module_state.initialized = true;
    module_state.display_dirty = true;
    
    ESP_LOGI(TAG, "Troops module initialized");
    return ESP_OK;
}

static esp_err_t troops_update(void) {
    if (!module_state.initialized) return ESP_OK;

    // Only send slider commands during active game.
    if (game_state_get_phase() == GAME_PHASE_IN_GAME) {
        // Query ADC value from handler (scanned by io_task)
        adc_event_t adc_value;
        if (adc_handler_get_value(ADC_CHANNEL_TROOPS_SLIDER, &adc_value) == ESP_OK) {
            uint8_t new_percent = adc_value.percent;
            module_state.slider_percent = new_percent;
            
            // Check for ≥1% change before sending command
            int diff = abs((int)new_percent - (int)module_state.last_sent_percent);
            if (diff >= TROOPS_CHANGE_THRESHOLD) {
                send_percent_command(new_percent);
                module_state.last_sent_percent = new_percent;
                module_state.display_dirty = true;
            }
        }
    }

    // Update display if dirty (when allowed)
    if (game_state_get_phase() == GAME_PHASE_IN_GAME && module_state.display_dirty) {
        update_troop_display();
        module_state.display_dirty = false;
    }
    
    return ESP_OK;
}

static bool troops_handle_event(const internal_event_t *event) {
    if (!module_state.initialized || !event) return false;
    
    // Parse event data for troop updates (from JSON payload)
    if (event->type == GAME_EVENT_TROOP_UPDATE && event->data && strlen(event->data) > 0) {
        cJSON* root = cJSON_Parse(event->data);
        if (root) {
            // TROOP_UPDATE event format: {"currentTroops":2500,"maxTroops":12141,...}
            cJSON* current = cJSON_GetObjectItem(root, "currentTroops");
            cJSON* max = cJSON_GetObjectItem(root, "maxTroops");
            cJSON* attack_ratio = cJSON_GetObjectItem(root, "attackRatio");
            
            if (current && cJSON_IsNumber(current)) {
                module_state.current_troops = (uint32_t)current->valuedouble;
            }
            if (max && cJSON_IsNumber(max)) {
                module_state.max_troops = (uint32_t)max->valuedouble;
            }

            // Game slider value (0.0..1.0) -> percent
            if (attack_ratio && cJSON_IsNumber(attack_ratio)) {
                double ratio = attack_ratio->valuedouble;
                if (ratio < 0.0) ratio = 0.0;
                if (ratio > 1.0) ratio = 1.0;
                s_game_percent = (uint8_t)lround(ratio * 100.0);
                s_have_game_percent = true;
            }

            // If we are receiving TROOP_UPDATE with non-zero max troops, the game
            // is effectively running. Use this as a fallback ONLY when we're still
            // pre-game, in case GAME_START was dropped due to event queue pressure.
            //
            // IMPORTANT: Never force a return to IN_GAME after a match ends, or the
            // end screen can get suppressed.
            const game_phase_t phase = game_state_get_phase();
            if (module_state.max_troops > 0 && (phase == GAME_PHASE_LOBBY || phase == GAME_PHASE_SPAWNING)) {
                ESP_LOGW(TAG, "TROOP_UPDATE received while phase=%d; forcing GAME_START transition", (int)phase);
                game_state_update(GAME_EVENT_GAME_START);
            }
            
            module_state.display_dirty = true;
            ESP_LOGD(TAG, "Troop update: %lu / %lu", 
                     (unsigned long)module_state.current_troops,
                     (unsigned long)module_state.max_troops);
            
            cJSON_Delete(root);
            return true;
        }
    }
    
    return false;
}

static void troops_get_status(module_status_t *status) {
    if (!status) return;
    
    status->initialized = module_state.initialized;
    status->operational = module_state.initialized;
    status->error_count = 0;
    
    snprintf(status->last_error, sizeof(status->last_error), 
             "Troops: %lu/%lu (%d%%)",
             (unsigned long)module_state.current_troops,
             (unsigned long)module_state.max_troops,
             module_state.slider_percent);
}

static esp_err_t troops_shutdown(void) {
    if (module_state.initialized) {
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string("  SHUTDOWN...   ");
        ESP_LOGI(TAG, "Troops module shutdown");
        module_state.initialized = false;
    }
    return ESP_OK;
}
