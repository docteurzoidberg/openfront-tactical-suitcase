/**
 * @file matrix_scanner.h
 * @brief Matrix Scanner Module - 3×7 Key Matrix Scanning
 * 
 * Scans the keypad matrix at 200Hz (5ms intervals) with debouncing.
 * Generates key press/release events via callback.
 */

#ifndef MATRIX_SCANNER_H
#define MATRIX_SCANNER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Key state enumeration
 */
typedef enum {
    KEY_STATE_RELEASED = 0,
    KEY_STATE_PRESSED = 1
} key_state_t;

/**
 * @brief Key event callback function type
 * 
 * Called when a key state transition is detected (after debouncing).
 * 
 * @param key_id Key identifier (1-15)
 * @param state New stable state (pressed or released)
 */
typedef void (*key_event_callback_t)(uint8_t key_id, key_state_t state);

/**
 * @brief Initialize matrix scanner
 * 
 * Configures GPIO pins for matrix scanning but does not start scanning.
 * 
 * @param callback Function to call on key events (required)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t matrix_scanner_init(key_event_callback_t callback);

/**
 * @brief Start matrix scanning task
 * 
 * Creates FreeRTOS task that scans matrix at 200Hz.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t matrix_scanner_start(void);

/**
 * @brief Stop matrix scanning task
 * 
 * Stops and deletes the scanning task.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t matrix_scanner_stop(void);

/**
 * @brief Get current key state
 * 
 * Returns the last stable (debounced) state of a key.
 * 
 * @param key_id Key identifier (1-15)
 * @return Current key state (pressed or released)
 */
key_state_t matrix_scanner_get_key_state(uint8_t key_id);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_SCANNER_H
