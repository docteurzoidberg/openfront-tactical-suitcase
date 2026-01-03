#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include "can_driver.h"

/**
 * @file can_protocol.h
 * @brief CAN protocol definitions for OTS sound module communication
 * 
 * Application-specific protocol layer built on top of the generic can_driver component.
 * Defines message IDs, payload formats, and helper functions for sound module communication.
 */

// CAN message ID definitions (from sound-module.md spec)
#define CAN_ID_PLAY_SOUND   0x420  // main → sound
#define CAN_ID_STOP_SOUND   0x421  // main → sound
#define CAN_ID_SOUND_STATUS 0x422  // sound → main
#define CAN_ID_SOUND_ACK    0x423  // sound → main

// CAN command codes
#define CAN_CMD_PLAY_SOUND  0x01
#define CAN_CMD_STOP_SOUND  0x02
#define CAN_CMD_STATUS      0x80
#define CAN_CMD_ACK         0x81

// PLAY_SOUND flags (byte 1)
#define CAN_FLAG_INTERRUPT      (1 << 0)  // Interrupt current playback
#define CAN_FLAG_HIGH_PRIORITY  (1 << 1)  // High priority sound
#define CAN_FLAG_LOOP           (1 << 2)  // Loop playback

// STOP_SOUND flags (byte 1)
#define CAN_FLAG_STOP_ALL       (1 << 0)  // Stop all sounds

// SOUND_STATUS state bits (byte 1)
#define CAN_STATUS_READY        (1 << 0)  // Module ready
#define CAN_STATUS_SD_MOUNTED   (1 << 1)  // SD card mounted
#define CAN_STATUS_PLAYING      (1 << 2)  // Currently playing
#define CAN_STATUS_MUTED        (1 << 3)  // Muted by switch
#define CAN_STATUS_ERROR        (1 << 4)  // Error state

// Special values
#define CAN_SOUND_INDEX_ANY     0xFFFF    // For stop: any/current sound
#define CAN_VOLUME_USE_POT      0xFF      // Use volume potentiometer

/**
 * @brief Build a PLAY_SOUND CAN frame
 * 
 * @param sound_index Sound index (0-65535)
 * @param flags Playback flags (interrupt, priority, loop)
 * @param volume_override Volume 0-100, or CAN_VOLUME_USE_POT (0xFF)
 * @param request_id Optional request ID for correlation (can be 0)
 * @param frame Output frame structure
 */
void can_build_play_sound(uint16_t sound_index, uint8_t flags, uint8_t volume_override, 
                          uint16_t request_id, can_frame_t *frame);

/**
 * @brief Build a STOP_SOUND CAN frame
 * 
 * @param sound_index Sound index to stop, or CAN_SOUND_INDEX_ANY (0xFFFF)
 * @param flags Stop flags (stop_all)
 * @param request_id Optional request ID for correlation (can be 0)
 * @param frame Output frame structure
 */
void can_build_stop_sound(uint16_t sound_index, uint8_t flags, uint16_t request_id, can_frame_t *frame);

/**
 * @brief Parse a SOUND_STATUS frame
 * 
 * @param frame Received CAN frame
 * @param state_bits Output: State bits (ready, sd_mounted, playing, etc.)
 * @param current_sound Output: Currently playing sound index (0xFFFF if none)
 * @param error_code Output: Last error code (0 if none)
 * @param volume Output: Current volume (0-100, 0xFF if unknown)
 * @param uptime Output: Uptime in seconds
 * @return true if frame is valid SOUND_STATUS, false otherwise
 */
bool can_parse_sound_status(const can_frame_t *frame, uint8_t *state_bits,
                           uint16_t *current_sound, uint8_t *error_code,
                           uint8_t *volume, uint16_t *uptime);

/**
 * @brief Parse a SOUND_ACK frame
 * 
 * @param frame Received CAN frame
 * @param ok Output: 1 if ok, 0 if failed
 * @param sound_index Output: Echo of requested sound index
 * @param error_code Output: Error code (0 if ok)
 * @param request_id Output: Echo of request ID
 * @return true if frame is valid SOUND_ACK, false otherwise
 */
bool can_parse_sound_ack(const can_frame_t *frame, uint8_t *ok,
                        uint16_t *sound_index, uint8_t *error_code,
                        uint16_t *request_id);

#endif // CAN_PROTOCOL_H
