#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "protocol.h"

/**
 * @brief Button event structure
 */
typedef struct {
    uint8_t button_index;     // 0=atom, 1=hydro, 2=mirv
    bool pressed;             // true=pressed, false=released
    uint32_t timestamp_ms;    // Timestamp of the event
} button_event_t;

/**
 * @brief Button event callback function type
 * 
 * Called when a button press is detected (after debouncing)
 */
typedef void (*button_event_callback_t)(uint8_t button_index);

/**
 * @brief Initialize button handler
 * 
 * Sets up internal state for button debouncing
 * 
 * @return ESP_OK on success
 */
esp_err_t button_handler_init(void);

/**
 * @brief Scan all buttons and update state
 * 
 * Should be called periodically (e.g., every 50ms) from I/O task
 * Handles debouncing and generates button events
 * 
 * @return ESP_OK on success
 */
esp_err_t button_handler_scan(void);

/**
 * @brief Set callback for button press events
 * 
 * @param callback Function to call when button is pressed
 */
void button_handler_set_callback(button_event_callback_t callback);

/**
 * @brief Get button event queue for processing
 * 
 * @return Queue handle containing button_event_t structures
 */
QueueHandle_t button_handler_get_queue(void);

/**
 * @brief Check if a specific button is currently pressed
 * 
 * @param button_index Button index (0-2)
 * @return true if button is currently pressed
 */
bool button_handler_is_pressed(uint8_t button_index);

#endif // BUTTON_HANDLER_H
