#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include "can_driver.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @file can_protocol.h
 * @brief CAN protocol for OTS sound module communication (audio module side)
 */

// CAN message ID definitions
#define CAN_ID_PLAY_SOUND   0x420  // main → sound
#define CAN_ID_STOP_SOUND   0x421  // main → sound
#define CAN_ID_SOUND_STATUS 0x422  // sound → main
#define CAN_ID_SOUND_ACK    0x423  // sound → main

// PLAY_SOUND flags (byte 2)
#define CAN_FLAG_INTERRUPT      (1 << 0)  // Interrupt current playback
#define CAN_FLAG_HIGH_PRIORITY  (1 << 1)  // High priority sound
#define CAN_FLAG_LOOP           (1 << 2)  // Loop playback

// STOP_SOUND flags (byte 2)
#define CAN_FLAG_STOP_ALL       (1 << 0)  // Stop all sounds

// SOUND_STATUS state bits (byte 0)
#define CAN_STATUS_READY        (1 << 0)  // Module ready
#define CAN_STATUS_SD_MOUNTED   (1 << 1)  // SD card mounted
#define CAN_STATUS_PLAYING      (1 << 2)  // Currently playing
#define CAN_STATUS_MUTED        (1 << 3)  // Muted by switch
#define CAN_STATUS_ERROR        (1 << 4)  // Error state

// Special values
#define CAN_SOUND_INDEX_ANY     0xFFFF    // For stop: any/current sound
#define CAN_VOLUME_USE_POT      0xFF      // Use volume potentiometer

// Error codes
#define CAN_ERR_OK              0x00      // No error
#define CAN_ERR_FILE_NOT_FOUND  0x01      // Sound file not found
#define CAN_ERR_SD_ERROR        0x02      // SD card read error
#define CAN_ERR_BUSY            0x03      // Already playing (no interrupt)
#define CAN_ERR_INVALID_INDEX   0x04      // Invalid sound index

/**
 * @brief Parse a PLAY_SOUND frame (audio module receives)
 */
bool can_parse_play_sound(const can_frame_t *frame, uint16_t *sound_index, 
                         uint8_t *flags, uint8_t *volume, uint16_t *request_id);

/**
 * @brief Parse a STOP_SOUND frame (audio module receives)
 */
bool can_parse_stop_sound(const can_frame_t *frame, uint16_t *sound_index, 
                         uint8_t *flags, uint16_t *request_id);

/**
 * @brief Build a SOUND_STATUS frame (audio module sends)
 */
void can_build_sound_status(uint8_t state_bits, uint16_t current_sound, 
                           uint8_t error_code, uint8_t volume, uint16_t uptime,
                           can_frame_t *frame);

/**
 * @brief Build a SOUND_ACK frame (audio module sends)
 */
void can_build_sound_ack(uint8_t ok, uint16_t sound_index, uint8_t error_code,
                        uint16_t request_id, can_frame_t *frame);

#endif // CAN_PROTOCOL_H
