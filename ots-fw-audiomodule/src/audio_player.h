/**
 * @file audio_player.h
 * @brief WAV file playback module
 * 
 * Handles parsing and playback of WAV files from SD card.
 * Integrates with audio_mixer for multi-source playback.
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "esp_err.h"
#include "audio_mixer.h"
#include "wav_utils.h"  // Use shared WAV utilities

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Play a WAV file from SD card
 * 
 * Opens a WAV file from SD card, parses the header, and starts playback
 * using the audio mixer. File path is relative to /sdcard/.
 * 
 * @param rel_path Relative path to WAV file (e.g., "sounds/hello.wav")
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t audio_player_play_wav(const char *rel_path);

/**
 * @brief Play a sound by index with mixing support
 * 
 * Plays a numbered sound file (e.g., sound 1 = /sdcard/sounds/0001.wav)
 * with volume control, looping, and interrupt capabilities.
 * 
 * @param sound_index Sound index number (1-9999)
 * @param volume Volume level (0-100)
 * @param loop Enable looping playback
 * @param interrupt Interrupt currently playing sounds
 * @param handle Output parameter for mixer source handle
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t audio_player_play_sound_by_index(uint16_t sound_index, 
                                            uint8_t volume,
                                            bool loop, 
                                            bool interrupt,
                                            audio_source_handle_t *handle);

/**
 * @brief Play embedded WAV file from firmware flash
 * 
 * Plays the WAV file that was embedded into firmware at compile time.
 * This is used for system sounds that don't require SD card.
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t audio_player_play_embedded_wav(void);

/**
 * @brief Parse WAV file header
 * 
 * Internal function to parse WAV file format and extract metadata.
 * 
 * @param f Open file pointer (positioned at start of file)
 * @param out Output structure for WAV information
 * @return ESP_OK on success, ESP_FAIL on parse error
 */
esp_err_t audio_player_parse_wav_header(FILE *f, wav_info_t *out);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H
