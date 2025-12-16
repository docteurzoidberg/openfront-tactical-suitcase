#include "game_state.h"
#include "led_controller.h"
#include "esp_log.h"

static const char *TAG = "GAME_STATE";

static game_phase_t current_phase = GAME_PHASE_LOBBY;
static game_state_change_callback_t state_change_callback = NULL;

static const char* phase_to_string(game_phase_t phase) {
    switch (phase) {
        case GAME_PHASE_LOBBY: return "LOBBY";
        case GAME_PHASE_SPAWNING: return "SPAWNING";
        case GAME_PHASE_IN_GAME: return "IN_GAME";
        case GAME_PHASE_WON: return "WON";
        case GAME_PHASE_LOST: return "LOST";
        case GAME_PHASE_ENDED: return "ENDED";
        default: return "UNKNOWN";
    }
}

esp_err_t game_state_init(void) {
    ESP_LOGI(TAG, "Initializing game state manager...");
    current_phase = GAME_PHASE_LOBBY;
    ESP_LOGI(TAG, "Game state initialized: %s", phase_to_string(current_phase));
    return ESP_OK;
}

void game_state_update(game_event_type_t event_type) {
    game_phase_t old_phase = current_phase;
    game_phase_t new_phase = current_phase;
    
    switch (event_type) {
        case GAME_EVENT_GAME_START:
            new_phase = GAME_PHASE_IN_GAME;
            
            // Turn off all LEDs at game start
            for (int i = 0; i < 3; i++) {
                led_command_t cmd = {.type = LED_TYPE_NUKE, .index = i, .effect = LED_EFFECT_OFF};
                led_controller_send_command(&cmd);
            }
            for (int i = 0; i < 6; i++) {
                led_command_t cmd = {.type = LED_TYPE_ALERT, .index = i, .effect = LED_EFFECT_OFF};
                led_controller_send_command(&cmd);
            }
            break;
            
        case GAME_EVENT_GAME_END:
            new_phase = GAME_PHASE_ENDED;
            break;
            
        case GAME_EVENT_WIN:
            new_phase = GAME_PHASE_WON;
            break;
            
        case GAME_EVENT_LOOSE:
            new_phase = GAME_PHASE_LOST;
            break;
            
        default:
            // No state change for other events
            return;
    }
    
    if (new_phase != old_phase) {
        current_phase = new_phase;
        ESP_LOGI(TAG, "Game phase changed: %s -> %s", 
                 phase_to_string(old_phase), 
                 phase_to_string(new_phase));
        
        // Call callback if registered
        if (state_change_callback) {
            state_change_callback(old_phase, new_phase);
        }
    }
}

game_phase_t game_state_get_phase(void) {
    return current_phase;
}

bool game_state_is_in_game(void) {
    return current_phase == GAME_PHASE_IN_GAME || 
           current_phase == GAME_PHASE_SPAWNING;
}

void game_state_set_callback(game_state_change_callback_t callback) {
    state_change_callback = callback;
}

void game_state_reset(void) {
    game_phase_t old_phase = current_phase;
    current_phase = GAME_PHASE_LOBBY;
    
    ESP_LOGI(TAG, "Game state reset to LOBBY");
    
    if (state_change_callback && old_phase != current_phase) {
        state_change_callback(old_phase, current_phase);
    }
}
