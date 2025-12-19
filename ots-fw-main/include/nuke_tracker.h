#ifndef NUKE_TRACKER_H
#define NUKE_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Nuke types tracked by the system
 */
typedef enum {
    NUKE_TYPE_ATOM = 0,
    NUKE_TYPE_HYDRO = 1,
    NUKE_TYPE_MIRV = 2,
    NUKE_TYPE_COUNT = 3
} nuke_type_t;

/**
 * @brief Direction of nuke (incoming or outgoing)
 */
typedef enum {
    NUKE_DIR_INCOMING = 0,  // Alert module tracks these
    NUKE_DIR_OUTGOING = 1   // Nuke module tracks these
} nuke_direction_t;

/**
 * @brief Nuke state in flight
 */
typedef enum {
    NUKE_STATE_IN_FLIGHT = 0,
    NUKE_STATE_EXPLODED = 1,
    NUKE_STATE_INTERCEPTED = 2
} nuke_state_t;

/**
 * @brief Initialize the nuke tracker
 * 
 * @return ESP_OK on success
 */
esp_err_t nuke_tracker_init(void);

/**
 * @brief Register a nuke launch (incoming or outgoing)
 * 
 * @param unit_id Unique unit ID from game
 * @param type Nuke type (atom/hydro/mirv)
 * @param direction Incoming or outgoing
 * @return ESP_OK on success
 */
esp_err_t nuke_tracker_register_launch(uint32_t unit_id, nuke_type_t type, nuke_direction_t direction);

/**
 * @brief Mark a nuke as resolved (exploded or intercepted)
 * 
 * @param unit_id Unique unit ID from game
 * @param exploded true if exploded, false if intercepted
 * @return ESP_OK on success
 */
esp_err_t nuke_tracker_resolve_nuke(uint32_t unit_id, bool exploded);

/**
 * @brief Get count of active (in-flight) nukes of a specific type and direction
 * 
 * @param type Nuke type to query
 * @param direction Direction to query
 * @return Number of active nukes, or 0 if none
 */
uint8_t nuke_tracker_get_active_count(nuke_type_t type, nuke_direction_t direction);

/**
 * @brief Clear all tracked nukes (e.g., on game end)
 */
void nuke_tracker_clear_all(void);

/**
 * @brief Get statistics for debugging
 * 
 * @param type Nuke type
 * @param direction Direction
 * @param out_in_flight Output: count in flight
 * @param out_exploded Output: count exploded
 * @param out_intercepted Output: count intercepted
 */
void nuke_tracker_get_stats(nuke_type_t type, nuke_direction_t direction,
                           uint8_t *out_in_flight, uint8_t *out_exploded, 
                           uint8_t *out_intercepted);

#endif // NUKE_TRACKER_H
