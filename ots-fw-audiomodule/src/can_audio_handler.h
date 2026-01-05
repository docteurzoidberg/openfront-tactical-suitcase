/**
 * @file can_audio_handler.h
 * @brief Audio-specific CAN Message Handler
 * 
 * Handles incoming audio CAN messages (PLAY_SOUND, STOP_SOUND) and sends
 * periodic STATUS messages on the CAN bus.
 * 
 * See /prompts/CANBUS_MESSAGE_SPEC.md for protocol details.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize audio CAN handler
 * @param sd_mounted Pointer to SD card mount status flag
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t can_audio_handler_init(bool *sd_mounted);

/**
 * @brief Start audio CAN RX task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t can_audio_handler_start_task(void);

/**
 * @brief Notify CAN handler that a sound has finished playback
 * 
 * Called by mixer when a non-looping sound completes.
 * Sends SOUND_FINISHED CAN message.
 * 
 * @param queue_id Queue ID of the finished sound
 * @param sound_index Original sound index
 * @param reason Reason code (0=completed, 1=stopped, 2=error)
 */
void can_audio_handler_sound_finished(uint8_t queue_id, uint16_t sound_index, uint8_t reason);

#ifdef __cplusplus
}
#endif
