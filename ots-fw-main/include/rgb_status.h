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
 * - No WiFi: Red solid
 * - WiFi only (no WebSocket): Orange solid
 * - Fully connected: Green solid
 * - Hardware error: Red fast blink
 */

typedef enum {
    RGB_STATUS_DISCONNECTED,    // Red solid - No WiFi connection
    RGB_STATUS_WIFI_ONLY,       // Orange solid - WiFi connected, no WebSocket
    RGB_STATUS_CONNECTED,       // Green solid - WiFi + WebSocket connected
    RGB_STATUS_ERROR            // Red fast blink - Hardware error
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
