/**
 * @file audio_tone_player.h
 * @brief Embedded tone playback management
 */

#ifndef AUDIO_TONE_PLAYER_H
#define AUDIO_TONE_PLAYER_H

#include "esp_err.h"
#include <stdint.h>

// Tone identifiers
typedef enum {
    TONE_ID_1 = 0,  // 1s @ 440Hz
    TONE_ID_2,      // 2s @ 880Hz
    TONE_ID_3,      // 5s @ 220Hz
    TONE_ID_MAX
} tone_id_t;

/**
 * @brief Play an embedded test tone
 * 
 * @param id Tone identifier
 * @param volume Volume (0-100)
 * @return ESP_OK on success, error otherwise
 */
esp_err_t tone_player_play(tone_id_t id, uint8_t volume);

/**
 * @brief Play all tones simultaneously (mixer test)
 * 
 * @return ESP_OK on success, error otherwise
 */
esp_err_t tone_player_mix_all(void);

/**
 * @brief Get tone information
 * 
 * @param id Tone identifier
 * @param size Output: tone size in bytes
 * @param description Output: tone description
 * @return ESP_OK on success, error otherwise
 */
esp_err_t tone_player_get_info(tone_id_t id, size_t *size, const char **description);

/**
 * @brief Get total size of all embedded tones
 * 
 * @return Total size in bytes
 */
size_t tone_player_get_total_size(void);

#endif // AUDIO_TONE_PLAYER_H
