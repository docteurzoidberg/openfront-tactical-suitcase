/**
 * @file i2s.h
 * @brief I2S Audio Interface Hardware Abstraction Layer
 * 
 * Manages I2S peripheral for audio output to codec
 */

#pragma once

#include "esp_err.h"
#include "driver/i2s_std.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2S configuration
#ifndef I2S_BCK_IO
#define I2S_BCK_IO     GPIO_NUM_27  // Bit clock
#endif
#ifndef I2S_WS_IO
#define I2S_WS_IO      GPIO_NUM_25  // Word select (LRCK)
#endif
#ifndef I2S_DO_IO
#define I2S_DO_IO      GPIO_NUM_26  // Data out
#endif
#ifndef I2S_DI_IO
#define I2S_DI_IO      I2S_GPIO_UNUSED  // Data in (not used for playback)
#endif

/**
 * @brief Initialize I2S peripheral
 * @param sample_rate Sample rate in Hz (e.g., 44100, 48000)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t i2s_init(uint32_t sample_rate);

/**
 * @brief Reconfigure I2S sample rate
 * @param sample_rate New sample rate in Hz
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t i2s_set_sample_rate(uint32_t sample_rate);

/**
 * @brief Write audio data to I2S
 * @param data Pointer to audio data buffer
 * @param size Size of data in bytes
 * @param bytes_written Pointer to store actual bytes written
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t i2s_write_audio(const void *data, size_t size, size_t *bytes_written);

#ifdef __cplusplus
}
#endif
