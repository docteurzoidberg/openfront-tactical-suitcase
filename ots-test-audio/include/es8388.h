/**
 * @file es8388.h
 * @brief ES8388 codec driver (ESP-IDF native)
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ES8388 codec
 */
esp_err_t es8388_codec_init(void);

/**
 * @brief Set DAC volume (0-100)
 */
esp_err_t es8388_set_volume(int volume);

/**
 * @brief Start DAC output
 */
esp_err_t es8388_start(void);

/**
 * @brief Stop DAC output
 */
esp_err_t es8388_stop(void);

#ifdef __cplusplus
}
#endif
