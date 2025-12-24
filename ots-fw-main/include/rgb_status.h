#ifndef RGB_STATUS_H
#define RGB_STATUS_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief RGB LED Status Indicator
 * 
 * Simple status-only RGB LED control for ESP32-S3 onboard LED
 * 
 * Status mapping:
 * - DISCONNECTED: Off
 * - WIFI_CONNECTING: Blue
 * - WIFI_ONLY: Yellow (WiFi connected, no WebSocket clients)
 * - USERSCRIPT_CONNECTED: Purple
 * - GAME_STARTED: Green
 * - ERROR: Red
 */

typedef enum {
    RGB_STATUS_DISCONNECTED = 0,
    RGB_STATUS_WIFI_CONNECTING,
    RGB_STATUS_WIFI_ONLY,
    RGB_STATUS_USERSCRIPT_CONNECTED,
    RGB_STATUS_GAME_STARTED,
    RGB_STATUS_ERROR,
    RGB_STATUS__COUNT
} rgb_status_t;

/**
 * @brief Initialize RGB status LED
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rgb_status_init(void);

/**
 * @brief Set RGB status LED state
 * 
 * @param status New status to display
 */
void rgb_status_set(rgb_status_t status);

/**
 * @brief Get current RGB status
 * 
 * @return Current status
 */
rgb_status_t rgb_status_get(void);

#endif // RGB_STATUS_H
