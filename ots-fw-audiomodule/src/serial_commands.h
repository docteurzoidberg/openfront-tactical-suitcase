/**
 * @file serial_commands.h
 * @brief Serial UART Command Processing
 * 
 * Handles interactive serial commands for testing and debugging
 */

#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Command entry mapping command string to WAV filename
 */
typedef struct {
    const char *cmd;       // Command string to match
    const char *filename;  // WAV filename in /sdcard
} serial_command_entry_t;

/**
 * @brief Callback function type for playing WAV files
 * @param filename Relative path to WAV file (e.g., "track1.wav")
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*play_wav_callback_t)(const char *filename);

/**
 * @brief Initialize serial command processor with command table
 * @param command_table Array of command entries
 * @param table_len Number of entries in command table
 * @param play_callback Function to call when playing WAV files
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t serial_commands_init(const serial_command_entry_t *command_table,
                               size_t table_len,
                               play_wav_callback_t play_callback);

/**
 * @brief Start serial command processing task
 * Creates FreeRTOS task that reads from stdin and processes commands
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t serial_commands_start_task(void);

/**
 * @brief Process a single command string
 * @param cmd Command string to process (null-terminated)
 * @return ESP_OK if command found and executed, ESP_ERR_NOT_FOUND otherwise
 */
esp_err_t serial_commands_handle(const char *cmd);

#ifdef __cplusplus
}
#endif
