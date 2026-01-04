/**
 * @file gpio.h
 * @brief ESP32-A1S Board GPIO Hardware Abstraction Layer
 * 
 * Manages board-specific GPIOs:
 * - Power amplifier enable (PA)
 * - Status LED
 * - Headphone detection
 * - Button inputs (for future use)
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPIO pin definitions
#define PA_ENABLE_GPIO GPIO_NUM_21         // Power amplifier enable
#define GREEN_LED_GPIO GPIO_NUM_22         // Status LED
#define HEADPHONE_DETECT_GPIO GPIO_NUM_39  // Headphone jack detection (active-low)

// Button GPIOs (for future use)
#define BUTTON_MODE_GPIO GPIO_NUM_36       // KEY1
#define BUTTON_REC_GPIO GPIO_NUM_13        // KEY2 (conflicts with SD card CS)
#define BUTTON_PLAY_GPIO GPIO_NUM_19       // KEY3
#define BUTTON_SET_GPIO GPIO_NUM_23        // KEY4
#define BUTTON_VOLDOWN_GPIO GPIO_NUM_18    // KEY5
#define BUTTON_VOLUP_GPIO GPIO_NUM_5       // KEY6

/**
 * @brief Initialize all board GPIO pins
 * - Configures PA enable, status LED, headphone detection
 * - Does NOT configure buttons (reserved for future use)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gpio_init(void);

/**
 * @brief Enable/disable power amplifier
 * @param enable true to enable PA, false to disable
 */
void gpio_set_pa_enable(bool enable);

/**
 * @brief Control status LED
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_led(bool on);

/**
 * @brief Get current headphone detection state
 * @return true if headphones are plugged in, false otherwise
 */
bool gpio_get_headphone_state(void);

/**
 * @brief Update headphone detection state
 * Call periodically to detect plug/unplug events
 * @return true if state changed, false if no change
 */
bool gpio_update_headphone_detection(void);

/**
 * @brief Get button GPIO numbers (for future use)
 * These functions return the GPIO pin number for each button
 */
int8_t gpio_get_button_mode(void);
int8_t gpio_get_button_rec(void);
int8_t gpio_get_button_play(void);
int8_t gpio_get_button_set(void);
int8_t gpio_get_button_voldown(void);
int8_t gpio_get_button_volup(void);

#ifdef __cplusplus
}
#endif
