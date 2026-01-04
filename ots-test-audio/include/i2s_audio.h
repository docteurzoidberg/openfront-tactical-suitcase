/**
 * @file i2s_audio.h
 * @brief I2S audio output (ESP-IDF native driver)
 */

#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize I2S for audio output
 * @param sample_rate Sample rate in Hz
 */
esp_err_t i2s_audio_init(uint32_t sample_rate);

/**
 * @brief Write audio data to I2S
 * @param data Audio data buffer
 * @param size Size in bytes
 * @param bytes_written Number of bytes actually written
 */
esp_err_t i2s_audio_write(const void *data, size_t size, size_t *bytes_written);

/**
 * @brief Deinitialize I2S
 */
esp_err_t i2s_audio_deinit(void);

#ifdef __cplusplus
}
#endif
