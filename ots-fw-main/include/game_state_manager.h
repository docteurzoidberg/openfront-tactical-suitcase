#ifndef GAME_STATE_MANAGER_H
#define GAME_STATE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

/**
 * @brief Game phase states
 * Must stay in sync with ots-shared/src/game.ts GamePhase
 */
typedef enum {
    GAME_PHASE_LOBBY = 0,
    GAME_PHASE_SPAWNING,
    GAME_PHASE_IN_GAME,
    GAME_PHASE_WON,
    GAME_PHASE_LOST,
    GAME_PHASE_ENDED
} game_phase_t;

/**
 * @brief Callback for game state changes
 */
typedef void (*game_state_change_callback_t)(game_phase_t old_phase, game_phase_t new_phase);

/**
 * @brief Initialize game state manager
 * 
 * @return ESP_OK on success
 */
esp_err_t game_state_init(void);

/**
 * @brief Update game state based on event
 * 
 * @param event_type Game event type from protocol
 */
void game_state_update(game_event_type_t event_type);

/**
 * @brief Get current game phase
 * 
 * @return Current game phase
 */
game_phase_t game_state_get_phase(void);

/**
 * @brief Check if currently in an active game
 * 
 * @return true if in game phase
 */
bool game_state_is_in_game(void);

/**
 * @brief Set callback for state changes
 * 
 * @param callback Function to call when state changes
 */
void game_state_set_callback(game_state_change_callback_t callback);

/**
 * @brief Reset all game state (for testing or manual reset)
 */
void game_state_reset(void);

#endif // GAME_STATE_H
