/**
 * @file led_controller.h
 * @brief LED Controller Module - SK6812-MINI-E RGB LED Control
 * 
 * Controls 15 RGB LEDs using RMT peripheral.
 * Maintains LED state and provides batch update capability.
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB color structure
 */
typedef struct {
    uint8_t r;  ///< Red (0-255)
    uint8_t g;  ///< Green (0-255)
    uint8_t b;  ///< Blue (0-255)
} led_rgb_t;

/**
 * @brief Initialize LED controller
 * 
 * Configures RMT peripheral and sets all LEDs to OFF.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_controller_init(void);

/**
 * @brief Set single key LED state and color
 * 
 * @param key_id Key identifier (1-15)
 * @param on LED on/off state
 * @param color RGB color (only used if on=true)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_controller_set_key(uint8_t key_id, bool on, led_rgb_t color);

/**
 * @brief Set multiple key LEDs using bitmask
 * 
 * @param key_bitmask Bitmask of keys (bit 0=K1, bit 14=K15)
 * @param on LED on/off state
 * @param color RGB color (only used if on=true)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_controller_set_bulk(uint16_t key_bitmask, bool on, led_rgb_t color);

/**
 * @brief Set all LEDs to same state and color
 * 
 * @param on LED on/off state
 * @param color RGB color (only used if on=true)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_controller_set_all(bool on, led_rgb_t color);

/**
 * @brief Update physical LEDs from state buffer
 * 
 * Writes current LED state to hardware via RMT.
 * Call after one or more set_*() calls to apply changes.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_controller_update(void);

/**
 * @brief Get current key LED state
 * 
 * @param key_id Key identifier (1-15)
 * @param on Output: LED on/off state
 * @param color Output: RGB color
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_controller_get_key(uint8_t key_id, bool *on, led_rgb_t *color);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROLLER_H
