/**
 * @file audio_player.h
 * @brief Unified audio playback - single entry point for all sounds
 * 
 * Plays sounds by ID from either SD card or embedded flash.
 * Priority: SD card first, then embedded fallback.
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "esp_err.h"
#include "audio_mixer.h"
#include "wav_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Play a sound by ID (unified entry point)
 * 
 * This is the main function to play any sound. It:
 * 1. First tries to load from SD card: /sdcard/sounds/XXXX.wav
 * 2. If not found, falls back to embedded sound in flash
 * 
 * Embedded sound IDs:
 * - 0-7: Game sounds (start, victory, defeat, death, alerts, launch)
 * - 100: Audio module ready (boot)
 * - 10000-10002: Test tones (440Hz, 880Hz, 220Hz)
 * - 10100: Quack sound
 * 
 * @param sound_id Sound index number
 * @param volume Volume level (0-100)
 * @param loop Enable looping playback
 * @param interrupt Interrupt currently playing sounds
 * @param handle Output parameter for mixer source handle
 * @return ESP_OK on success
 * @return ESP_ERR_NOT_FOUND if sound not found (SD or embedded)
 * @return ESP_FAIL on playback error
 */
esp_err_t audio_player_play_sound(uint16_t sound_id, 
                                   uint8_t volume,
                                   bool loop, 
                                   bool interrupt,
                                   audio_source_handle_t *handle);

/**
 * @brief Play a WAV file from SD card by path
 * 
 * Legacy function for direct file playback.
 * 
 * @param rel_path Relative path to WAV file (e.g., "sounds/hello.wav")
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t audio_player_play_wav(const char *rel_path);

/**
 * @brief Parse WAV file header
 * 
 * @param f Open file pointer (positioned at start of file)
 * @param out Output structure for WAV information
 * @return ESP_OK on success, ESP_FAIL on parse error
 */
esp_err_t audio_player_parse_wav_header(FILE *f, wav_info_t *out);

/**
 * @brief Get count of embedded sounds
 * @return Number of embedded sounds available
 */
size_t audio_player_get_embedded_count(void);

/**
 * @brief Get information about an embedded sound
 * 
 * @param index Index in embedded sounds array (0 to count-1)
 * @param sound_id Output: sound ID
 * @param name Output: sound name
 * @param size Output: sound size in bytes
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if index out of range
 */
esp_err_t audio_player_get_embedded_info(size_t index, uint16_t *sound_id, 
                                          const char **name, size_t *size);

/**
 * @brief Get total size of all embedded sounds
 * @return Total bytes used by embedded sounds
 */
size_t audio_player_get_total_embedded_size(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H
