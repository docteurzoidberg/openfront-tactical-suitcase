#ifndef SOUND_MODULE_H
#define SOUND_MODULE_H

#include "hardware_module.h"

/**
 * @file sound_module.h
 * @brief Sound Module - Audio feedback via CAN bus to ESP32-A1S audio controller
 * 
 * This module handles SOUND_PLAY events from the game and sends CAN bus commands
 * to the external ESP32-A1S audio module (ots-fw-audiomodule).
 * 
 * The sound module does NOT use MCP23017 I/O expanders. It communicates with
 * a separate ESP32 controller via CAN bus.
 * 
 * Hardware:
 * - External ESP32-A1S Audio Development Kit
 * - CAN bus interface (planned)
 * - SD card with audio files on the audio controller
 * 
 * Communication:
 * - CAN bus (500 kbps, 11-bit IDs)
 * - Message IDs: 0x420 (PLAY), 0x421 (STOP), 0x422 (STATUS), 0x423 (ACK)
 * 
 * Current Status: MOCK MODE
 * - CAN messages are logged but not physically transmitted
 * - Physical CAN implementation pending (will use ESP-IDF TWAI driver)
 */

/**
 * @brief Sound catalog - maps sound IDs to CAN sound indices
 * 
 * These indices correspond to files on the audio module's SD card:
 * - /sounds/0001.mp3 (or .wav)
 * - /sounds/0002.mp3
 * - etc.
 * 
 * TODO: Sync with WEBSOCKET_MESSAGE_SPEC.md Sound Catalog section
 */
typedef enum {
    SOUND_INDEX_GAME_START = 1,
    SOUND_INDEX_ALERT_ATOM = 10,
    SOUND_INDEX_ALERT_HYDRO = 11,
    SOUND_INDEX_ALERT_MIRV = 12,
    SOUND_INDEX_ALERT_LAND = 13,
    SOUND_INDEX_ALERT_NAVAL = 14,
    SOUND_INDEX_NUKE_LAUNCH = 20,
    SOUND_INDEX_NUKE_EXPLODE = 21,
    SOUND_INDEX_NUKE_INTERCEPT = 22,
    SOUND_INDEX_VICTORY = 30,
    SOUND_INDEX_DEFEAT = 31,
    SOUND_INDEX_TEST_BEEP = 100,
} sound_index_t;

/**
 * @brief Get the sound module instance
 * @return Pointer to hardware_module_t interface
 */
hardware_module_t* sound_module_get(void);

/**
 * @brief Play a sound via CAN bus
 * 
 * Builds and sends a CAN PLAY_SOUND message to the audio controller.
 * Non-blocking - does not wait for playback completion or ACK.
 * 
 * @param sound_index Sound index (0-65535) corresponding to SD card file
 * @param interrupt If true, interrupts currently playing sound
 * @param high_priority If true, marks as high-priority sound
 * @return ESP_OK on success (message sent), error code otherwise
 */
esp_err_t sound_module_play(uint16_t sound_index, bool interrupt, bool high_priority);

/**
 * @brief Stop currently playing sound
 * 
 * @param sound_index Specific sound to stop, or 0xFFFF for any/current
 * @param stop_all If true, stops all queued sounds
 * @return ESP_OK on success
 */
esp_err_t sound_module_stop(uint16_t sound_index, bool stop_all);

#endif // SOUND_MODULE_H
