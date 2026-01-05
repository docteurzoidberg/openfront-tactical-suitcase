/**
 * @file audio_console_ui.h
 * @brief Console UI formatting and display utilities
 */

#ifndef AUDIO_CONSOLE_UI_H
#define AUDIO_CONSOLE_UI_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Print console welcome banner
 */
void console_ui_print_banner(void);

/**
 * @brief Print mixer status information
 * 
 * @return ESP_OK on success
 */
esp_err_t console_ui_print_status(void);

/**
 * @brief Print currently playing sources
 * 
 * @return ESP_OK on success
 */
esp_err_t console_ui_print_playing(void);

/**
 * @brief Print system information (memory, PSRAM, etc.)
 * 
 * @return ESP_OK on success
 */
esp_err_t console_ui_print_sysinfo(void);

/**
 * @brief Print embedded tone information
 * 
 * @return ESP_OK on success
 */
esp_err_t console_ui_print_tone_info(void);

#endif // AUDIO_CONSOLE_UI_H
