#ifndef WS2812_RMT_H
#define WS2812_RMT_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief RGB color structure
 */
typedef struct {
    uint8_t r;  ///< Red component (0-255)
    uint8_t g;  ///< Green component (0-255)
    uint8_t b;  ///< Blue component (0-255)
} ws2812_color_t;

/**
 * @brief WS2812 configuration
 */
typedef struct {
    int gpio_num;           ///< GPIO pin number for WS2812 data line
    uint32_t led_count;     ///< Number of LEDs in the strip (default: 1)
    uint32_t resolution_hz; ///< RMT resolution in Hz (default: 10MHz)
} ws2812_config_t;

/**
 * @brief Initialize WS2812 LED strip driver
 * 
 * @param config WS2812 configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_init(const ws2812_config_t *config);

/**
 * @brief Set color of single LED
 * 
 * @param index LED index (0-based)
 * @param color RGB color to set
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_set_pixel(uint32_t index, ws2812_color_t color);

/**
 * @brief Set color of all LEDs
 * 
 * @param color RGB color to set
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_set_all(ws2812_color_t color);

/**
 * @brief Update LED strip (send data to hardware)
 * 
 * Call this after setting pixel colors to actually update the LEDs
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_update(void);

/**
 * @brief Check if WS2812 driver is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool ws2812_is_initialized(void);

/**
 * @brief Deinitialize WS2812 driver and free resources
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_deinit(void);

#endif // WS2812_RMT_H
