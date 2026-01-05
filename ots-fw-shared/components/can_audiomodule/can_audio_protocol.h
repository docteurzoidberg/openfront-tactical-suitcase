#ifndef CAN_AUDIO_PROTOCOL_H
#define CAN_AUDIO_PROTOCOL_H

#include "can_driver.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @file can_audio_protocol.h
 * @brief Audio-specific CAN protocol for OTS sound module
 * 
 * This file defines the audio module's CAN message format, IDs, and functions.
 * 
 * Audio protocol documentation:
 *   /prompts/CANBUS_MESSAGE_SPEC.md (protocol specification)
 *   /doc/developer/canbus-protocol.md (implementation guide)
 */

// ============================================================================
// AUDIO MODULE CAN MESSAGE IDs (0x420-0x42F block)
// ============================================================================

#define CAN_ID_PLAY_SOUND       0x420  // main → audio (PLAY request with loop/volume)
#define CAN_ID_STOP_SOUND       0x421  // main → audio (STOP by queue ID)
#define CAN_ID_STOP_ALL         0x422  // main → audio (STOP ALL sounds)
#define CAN_ID_SOUND_ACK        0x423  // audio → main (PLAY ACK with queue ID)
#define CAN_ID_STOP_ACK         0x424  // audio → main (STOP acknowledgment)
#define CAN_ID_SOUND_FINISHED   0x425  // audio → main (sound playback finished)
#define CAN_ID_SOUND_STATUS     0x426  // audio → main (periodic status - future enhancement)
// 0x427-0x42F: Reserved for future audio features

// ============================================================================
// PLAY_SOUND MESSAGE (0x420)
// ============================================================================

// Flags (byte 2)
#define CAN_AUDIO_FLAG_INTERRUPT      (1 << 0)  // Interrupt current playback
#define CAN_AUDIO_FLAG_HIGH_PRIORITY  (1 << 1)  // High priority sound (reserved)
#define CAN_AUDIO_FLAG_LOOP           (1 << 2)  // Loop playback until stopped

// ============================================================================
// STOP_SOUND MESSAGE (0x421)
// ============================================================================

// Flags (byte 2)
#define CAN_AUDIO_FLAG_STOP_ALL       (1 << 0)  // Stop all sounds (deprecated, use STOP_ALL)

// ============================================================================
// SOUND_STATUS MESSAGE (0x426) - Future Enhancement
// ============================================================================

// State bits (byte 0)
#define CAN_AUDIO_STATUS_READY        (1 << 0)  // Module ready
#define CAN_AUDIO_STATUS_SD_MOUNTED   (1 << 1)  // SD card mounted
#define CAN_AUDIO_STATUS_PLAYING      (1 << 2)  // Currently playing
#define CAN_AUDIO_STATUS_MUTED        (1 << 3)  // Muted by hardware switch
#define CAN_AUDIO_STATUS_ERROR        (1 << 4)  // Error state

// ============================================================================
// SPECIAL VALUES
// ============================================================================

#define CAN_AUDIO_SOUND_INDEX_ANY     0xFFFF    // For stop: any/current sound
#define CAN_AUDIO_VOLUME_USE_POT      0xFF      // Use hardware volume potentiometer
#define CAN_AUDIO_QUEUE_ID_INVALID    0x00      // Invalid queue ID (error case)

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

#define CAN_AUDIO_STATUS_INTERVAL_MS  5000      // STATUS message interval (5 seconds)
#define CAN_AUDIO_ACK_TIMEOUT_MS      200       // ACK response timeout (200ms)
#define CAN_AUDIO_RETRY_DELAY_MS      500       // Retry delay on mixer full (500ms)

// ============================================================================
// ERROR CODES
// ============================================================================

#define CAN_AUDIO_ERR_OK              0x00      // No error
#define CAN_AUDIO_ERR_FILE_NOT_FOUND  0x01      // Sound file not found on SD
#define CAN_AUDIO_ERR_SD_ERROR        0x02      // SD card read error
#define CAN_AUDIO_ERR_BUSY            0x03      // Already playing (no interrupt)
#define CAN_AUDIO_ERR_INVALID_INDEX   0x04      // Invalid sound index
#define CAN_AUDIO_ERR_MIXER_FULL      0x05      // No free mixer slots
#define CAN_AUDIO_ERR_INVALID_QUEUE_ID 0x06     // Queue ID not found

// ============================================================================
// SOUND_FINISHED REASON CODES
// ============================================================================

#define CAN_AUDIO_FINISHED_COMPLETED  0x00      // Played to end (non-loop)
#define CAN_AUDIO_FINISHED_STOPPED    0x01      // Stopped by user command
#define CAN_AUDIO_FINISHED_ERROR      0x02      // Error during playback

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Allocate next queue ID (1-255, wraps around)
 * @param current_id Pointer to current queue ID counter (in/out)
 * @return Next queue ID (1-255)
 * 
 * Thread-safe note: Caller must ensure thread safety if used in multi-threaded context
 */
static inline uint8_t can_audio_allocate_queue_id(uint8_t *current_id) {
    if (!current_id) return CAN_AUDIO_QUEUE_ID_INVALID;
    
    uint8_t id = (*current_id)++;
    if (*current_id == 0) {
        *current_id = 1;  // Skip 0 (reserved for errors), wrap to 1
    }
    return id;
}

/**
 * @brief Generate next request ID (wraps at 65535)
 * @param current_id Pointer to current request ID counter (in/out)
 * @return Next request ID (0-65535)
 * 
 * Thread-safe note: Caller must ensure thread safety if used in multi-threaded context
 */
static inline uint16_t can_audio_allocate_request_id(uint16_t *current_id) {
    if (!current_id) return 0;
    return (*current_id)++;  // Natural 16-bit wraparound
}

/**
 * @brief Check if queue ID is valid
 * @param queue_id Queue ID to check
 * @return true if valid (1-255), false if invalid (0)
 */
static inline bool can_audio_queue_id_is_valid(uint8_t queue_id) {
    return queue_id != CAN_AUDIO_QUEUE_ID_INVALID;
}

// ============================================================================
// PARSING FUNCTIONS (Audio Module Receives)
// ============================================================================

/**
 * @brief Parse a PLAY_SOUND frame
 * @param frame CAN frame
 * @param sound_index Output: sound index (0-65535)
 * @param flags Output: play flags (CAN_AUDIO_FLAG_*)
 * @param volume Output: volume (0-100, or CAN_AUDIO_VOLUME_USE_POT)
 * @param request_id Output: request ID for tracking
 * @return true if valid PLAY_SOUND frame
 */
bool can_audio_parse_play_sound(const can_frame_t *frame, uint16_t *sound_index, 
                                uint8_t *flags, uint8_t *volume, uint16_t *request_id);

/**
 * @brief Parse a STOP_SOUND frame
 * @param frame CAN frame
 * @param queue_id Output: queue ID to stop (1-255)
 * @param flags Output: stop flags
 * @param request_id Output: request ID for tracking
 * @return true if valid STOP_SOUND frame
 */
bool can_audio_parse_stop_sound(const can_frame_t *frame, uint8_t *queue_id, 
                                uint8_t *flags, uint16_t *request_id);

// ============================================================================
// BUILDING FUNCTIONS (Main Controller Sends)
// ============================================================================

/**
 * @brief Build a PLAY_SOUND frame
 * @param sound_index Sound index (0-65535)
 * @param flags Play flags (CAN_AUDIO_FLAG_*)
 * @param volume Volume (0-100, or CAN_AUDIO_VOLUME_USE_POT)
 * @param request_id Request ID for tracking
 * @param frame Output frame
 */
void can_audio_build_play_sound(uint16_t sound_index, uint8_t flags, 
                                uint8_t volume, uint16_t request_id, can_frame_t *frame);

/**
 * @brief Build a STOP_SOUND frame
 * @param queue_id Queue ID to stop (1-255)
 * @param flags Stop flags
 * @param request_id Request ID for tracking
 * @param frame Output frame
 */
void can_audio_build_stop_sound(uint8_t queue_id, uint8_t flags, 
                                uint16_t request_id, can_frame_t *frame);

/**
 * @brief Build a STOP_ALL frame
 * @param frame Output frame
 */
void can_audio_build_stop_all(can_frame_t *frame);

// ============================================================================
// BUILDING FUNCTIONS (Audio Module Sends)
// ============================================================================

/**
 * @brief Build a SOUND_STATUS frame
 * @param state_bits Status bits (CAN_AUDIO_STATUS_*)
 * @param current_sound Currently playing sound index (or 0xFFFF if none)
 * @param error_code Last error code (CAN_AUDIO_ERR_*)
 * @param volume Current master volume (0-100)
 * @param uptime Uptime in seconds (wraps at 65535)
 * @param frame Output frame
 */
void can_audio_build_sound_status(uint8_t state_bits, uint16_t current_sound, 
                                  uint8_t error_code, uint8_t volume, uint16_t uptime,
                                  can_frame_t *frame);

/**
 * @brief Build a SOUND_ACK frame
 * @param ok Success flag (1=success, 0=error)
 * @param sound_index Echoed sound index from request
 * @param queue_id Assigned queue ID (0=error, 1-255=valid handle)
 * @param error_code Error code if ok=0 (CAN_AUDIO_ERR_*)
 * @param request_id Echoed request ID from request
 * @param frame Output frame
 */
void can_audio_build_sound_ack(uint8_t ok, uint16_t sound_index, uint8_t queue_id,
                               uint8_t error_code, uint16_t request_id, can_frame_t *frame);

/**
 * @brief Build a SOUND_FINISHED frame
 * @param queue_id Queue ID that finished
 * @param sound_index Original sound index
 * @param reason Finish reason (CAN_AUDIO_FINISHED_*)
 * @param frame Output frame
 */
void can_audio_build_sound_finished(uint8_t queue_id, uint16_t sound_index, 
                                   uint8_t reason, can_frame_t *frame);

#endif // CAN_AUDIO_PROTOCOL_H
