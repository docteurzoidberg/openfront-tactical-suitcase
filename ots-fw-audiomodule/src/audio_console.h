/**
 * @file audio_console.h
 * @brief Interactive console for audio control
 * 
 * Unified console interface for:
 * - WAV file playback from SD card
 * - Embedded test tone playback
 * - Audio mixer control
 * 
 * Uses ESP Console API with proper command handling.
 */

#ifndef AUDIO_CONSOLE_H
#define AUDIO_CONSOLE_H

#include "esp_err.h"

/**
 * @brief Initialize audio console system
 * 
 * Registers all console commands and displays welcome banner
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_console_init(void);

/**
 * @brief Start audio console REPL
 * 
 * Starts ESP Console REPL for interactive commands
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_console_start(void);

/**
 * @brief Stop audio console task
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_console_stop(void);

#endif // AUDIO_CONSOLE_H
