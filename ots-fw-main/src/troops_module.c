#include "troops_module.h"
#include "protocol.h"
#include "ws_server.h"
#include "event_dispatcher.h"
#include "led_controller.h"
#include "lcd_driver.h"
#include "adc_driver.h"
#include "adc_handler.h"
#include "game_state.h"
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
    uint32_t calculated = ((uint64_t)module_state.current_troops * module_state.slider_percent) / 100;
    troops_format_count(calculated, calc_str, sizeof(calc_str));
    snprintf(line2, sizeof(line2), "%d%% (%s)", module_state.slider_percent, calc_str);
    
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
        ws_server_send_text(json_str, strlen(json_str));
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
    
    // Initialize ADC
    esp_err_t ret = ads1015_init(TROOPS_ADS1015_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADS1015");
        return ret;
    }
    
    // Initialize LCD
    ret = lcd_init(TROOPS_LCD_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD");
        return ret;
    }
    
    // Module initialization complete
    lcd_clear();
    
    module_state.initialized = true;
    module_state.display_dirty = true;
    
    ESP_LOGI(TAG, "Troops module initialized");
    return ESP_OK;
}

static esp_err_t troops_update(void) {
    if (!module_state.initialized) return ESP_OK;
    
    // Only monitor slider during active game
    game_phase_t phase = game_state_get_phase();
    if (phase != GAME_PHASE_IN_GAME) {
        return ESP_OK;
    }
    
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
    
    // Update display if dirty (only during game)
    if (module_state.display_dirty) {
        update_troop_display();
        module_state.display_dirty = false;
    }
    
    return ESP_OK;
}

static bool troops_handle_event(const internal_event_t *event) {
    if (!module_state.initialized || !event) return false;
    
    // Only handle events during active game
    game_phase_t phase = game_state_get_phase();
    if (phase != GAME_PHASE_IN_GAME) {
        return false;
    }
    
    // Parse event data for troop updates (from JSON payload)
    if (event->data && strlen(event->data) > 0) {
        cJSON* root = cJSON_Parse(event->data);
        if (root) {
            // Check for troops data in state message
            cJSON* troops = cJSON_GetObjectItem(root, "troops");
            if (troops) {
                cJSON* current = cJSON_GetObjectItem(troops, "current");
                cJSON* max = cJSON_GetObjectItem(troops, "max");
                
                if (current && cJSON_IsNumber(current)) {
                    module_state.current_troops = (uint32_t)current->valueint;
                }
                if (max && cJSON_IsNumber(max)) {
                    module_state.max_troops = (uint32_t)max->valueint;
                }
                
                module_state.display_dirty = true;
                ESP_LOGI(TAG, "Troop update: %lu / %lu", 
                         (unsigned long)module_state.current_troops,
                         (unsigned long)module_state.max_troops);
                
                cJSON_Delete(root);
                return true;
            }
            cJSON_Delete(root);
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
