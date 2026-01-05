#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief LED types for command targeting
 */
typedef enum {
    LED_TYPE_NUKE,     // Nuke button LEDs (0-2: atom, hydro, mirv)
    LED_TYPE_ALERT,    // Alert LEDs (0-5: warning, atom, hydro, mirv, land, naval)
    LED_TYPE_LINK      // Main power link LED
} led_type_t;

/**
 * @brief LED effect modes
 */
typedef enum {
    LED_EFFECT_OFF,           // Turn LED off
    LED_EFFECT_ON,            // Turn LED on (solid)
    LED_EFFECT_BLINK,         // Blink LED continuously
    LED_EFFECT_BLINK_TIMED    // Blink for specific duration then turn off
} led_effect_t;

/**
 * @brief LED control command structure
 */
typedef struct {
    led_type_t type;          // Which LED type to control
    uint8_t index;            // LED index within that type
    led_effect_t effect;      // Effect to apply
    uint32_t duration_ms;     // Duration for timed effects (0 = infinite)
    uint32_t blink_rate_ms;   // Blink interval in ms (default 500)
} led_command_t;

/**
 * @brief Initialize LED controller
 * 
 * Creates internal task and queue for LED management
 * 
 * @return ESP_OK on success
 */
esp_err_t led_controller_init(void);

/**
 * @brief Send command to LED controller
 * 
 * Non-blocking command submission via queue
 * 
 * @param cmd Pointer to LED command structure
 * @return true if command was queued successfully
 */
bool led_controller_send_command(const led_command_t *cmd);

/**
 * @brief Helper function to set nuke LED blinking for duration
 * 
 * @param index Nuke LED index (0=atom, 1=hydro, 2=mirv)
 * @param duration_ms Duration to blink in milliseconds
 * @return true if command was queued
 */
bool led_controller_nuke_blink(uint8_t index, uint32_t duration_ms);

/**
 * @brief Helper function to set alert LED on for duration
 * 
 * @param index Alert LED index (0=warning, 1=atom, 2=hydro, 3=mirv, 4=land, 5=naval)
 * @param duration_ms Duration to stay on in milliseconds
 * @return true if command was queued
 */
bool led_controller_alert_on(uint8_t index, uint32_t duration_ms);

/**
 * @brief Helper function to set link LED state
 * 
 * @param on true to turn on, false to turn off
 * @return true if command was queued
 */
bool led_controller_link_set(bool on);

/**
 * @brief Helper function to set link LED blinking at specified rate
 * 
 * @param blink_rate_ms Blink interval in milliseconds
 * @return true if command was queued
 */
bool led_controller_link_blink(uint32_t blink_rate_ms);

/**
 * @brief Get LED controller queue handle (for advanced usage)
 * 
 * @return Queue handle or NULL if not initialized
 */
QueueHandle_t led_controller_get_queue(void);

#endif // LED_HANDLER_H
