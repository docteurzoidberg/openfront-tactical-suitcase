/**
 * @file can_handler.h
 * @brief CAN Bus Message Handler
 * 
 * Handles incoming CAN messages (PLAY_SOUND, STOP_SOUND) and sends
 * periodic STATUS messages on the CAN bus.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize CAN handler
 * @param sd_mounted Pointer to SD card mount status flag
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t can_handler_init(bool *sd_mounted);

/**
 * @brief Start CAN RX task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t can_handler_start_task(void);

#ifdef __cplusplus
}
#endif
