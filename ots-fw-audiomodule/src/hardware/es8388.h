#ifndef ES8388_DRIVER_H
#define ES8388_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initialize ES8388 codec
 * 
 * Configures the ES8388 audio codec for playback:
 * - Soft reset
 * - PLL configuration for specified sample rate
 * - I2S interface setup
 * - DAC and output mixer enable
 * - Speaker output configuration
 * 
 * @param sample_rate Sample rate in Hz (e.g., 44100, 48000)
 * @return ESP_OK on success, error otherwise
 */
esp_err_t es8388_init(uint32_t sample_rate);

/**
 * @brief Set speaker volume
 * 
 * @param volume Volume level 0-100 (0=min, 100=max)
 * @return ESP_OK on success, error otherwise
 */
esp_err_t es8388_set_speaker_volume(uint8_t volume);

/**
 * @brief Set headphone volume
 * 
 * @param volume Volume level 0-100 (0=min, 100=max)
 * @return ESP_OK on success, error otherwise
 */
esp_err_t es8388_set_headphone_volume(uint8_t volume);

/**
 * @brief Enable/disable speaker PA (power amplifier)
 * 
 * @param enable true to enable, false to disable
 * @return ESP_OK on success, error otherwise
 */
esp_err_t es8388_set_speaker_enable(bool enable);

#endif // ES8388_DRIVER_H
