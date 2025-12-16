#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

/**
 * @brief Event source types
 */
typedef enum {
    EVENT_SOURCE_BUTTON,
    EVENT_SOURCE_WEBSOCKET,
    EVENT_SOURCE_TIMER,
    EVENT_SOURCE_SYSTEM,
    EVENT_SOURCE_UNKNOWN
} event_source_t;

/**
 * @brief Internal event structure with routing info
 * Extends game_event_t with source tracking
 */
typedef struct {
    game_event_type_t type;
    event_source_t source;
    uint64_t timestamp;
    char message[128];
    char data[256];
} internal_event_t;

/**
 * @brief Event handler callback function type
 * 
 * @param event Pointer to the event
 * @return true if event was handled, false to continue propagation
 */
typedef bool (*event_handler_t)(const internal_event_t *event);

/**
 * @brief Initialize event dispatcher
 * 
 * Creates event queue and processing task
 * 
 * @return ESP_OK on success
 */
esp_err_t event_dispatcher_init(void);

/**
 * @brief Register an event handler for specific event type
 * 
 * @param event_type Event type to handle (or GAME_EVENT_INVALID for all events)
 * @param handler Callback function
 * @return ESP_OK on success
 */
esp_err_t event_dispatcher_register(game_event_type_t event_type, event_handler_t handler);

/**
 * @brief Unregister an event handler
 * 
 * @param event_type Event type
 * @param handler Handler to remove
 * @return ESP_OK on success
 */
esp_err_t event_dispatcher_unregister(game_event_type_t event_type, event_handler_t handler);

/**
 * @brief Post an event to the dispatcher queue
 * 
 * @param event Pointer to event structure
 * @return ESP_OK if event was queued
 */
esp_err_t event_dispatcher_post(const internal_event_t *event);

/**
 * @brief Post a simple event with just type and source
 * 
 * @param type Event type
 * @param source Event source
 * @return ESP_OK if event was queued
 */
esp_err_t event_dispatcher_post_simple(game_event_type_t type, event_source_t source);

/**
 * @brief Convert game_event_t to internal_event_t and post
 * 
 * @param game_event Game event structure
 * @param source Event source
 * @return ESP_OK if event was queued
 */
esp_err_t event_dispatcher_post_game_event(const game_event_t *game_event, event_source_t source);

/**
 * @brief Get event dispatcher queue handle (for advanced usage)
 * 
 * @return Queue handle or NULL if not initialized
 */
void* event_dispatcher_get_queue(void);

/**
 * @brief Check if event dispatcher is running
 * 
 * @return true if dispatcher task is active
 */
bool event_dispatcher_is_running(void);

#endif // EVENT_DISPATCHER_H
