#include "can_audio_protocol.h"
#include <string.h>

/**
 * @file can_audio_protocol.c
 * @brief Audio-specific CAN protocol implementation
 */

// ============================================================================
// PARSING FUNCTIONS (Audio Module Receives)
// ============================================================================

bool can_audio_parse_play_sound(const can_frame_t *frame, uint16_t *sound_index, 
                                uint8_t *flags, uint8_t *volume, uint16_t *request_id) {
    if (!frame || frame->id != CAN_ID_PLAY_SOUND || frame->dlc < 6) {
        return false;
    }
    
    if (sound_index) {
        *sound_index = frame->data[0] | (frame->data[1] << 8);
    }
    if (flags) {
        *flags = frame->data[2];
    }
    if (volume) {
        *volume = frame->data[3];
    }
    if (request_id) {
        *request_id = frame->data[4] | (frame->data[5] << 8);
    }
    
    return true;
}

bool can_audio_parse_stop_sound(const can_frame_t *frame, uint8_t *queue_id, 
                                uint8_t *flags, uint16_t *request_id) {
    if (!frame || frame->id != CAN_ID_STOP_SOUND || frame->dlc < 5) {
        return false;
    }
    
    if (queue_id) {
        *queue_id = frame->data[0];  // Queue ID is single byte
    }
    if (flags) {
        *flags = frame->data[2];
    }
    if (request_id) {
        *request_id = frame->data[3] | (frame->data[4] << 8);
    }
    
    return true;
}

// ============================================================================
// BUILDING FUNCTIONS (Main Controller Sends)
// ============================================================================

void can_audio_build_play_sound(uint16_t sound_index, uint8_t flags, 
                                uint8_t volume, uint16_t request_id, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_PLAY_SOUND;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 6;
    
    // Byte 0-1: Sound index (little-endian)
    frame->data[0] = sound_index & 0xFF;
    frame->data[1] = (sound_index >> 8) & 0xFF;
    
    // Byte 2: Flags
    frame->data[2] = flags;
    
    // Byte 3: Volume (0-100 or CAN_AUDIO_VOLUME_USE_POT)
    frame->data[3] = volume;
    
    // Byte 4-5: Request ID (little-endian)
    frame->data[4] = request_id & 0xFF;
    frame->data[5] = (request_id >> 8) & 0xFF;
}

void can_audio_build_stop_sound(uint8_t queue_id, uint8_t flags, 
                                uint16_t request_id, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_STOP_SOUND;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 5;
    
    // Byte 0: Queue ID
    frame->data[0] = queue_id;
    
    // Byte 1: Reserved
    frame->data[1] = 0;
    
    // Byte 2: Flags
    frame->data[2] = flags;
    
    // Byte 3-4: Request ID (little-endian)
    frame->data[3] = request_id & 0xFF;
    frame->data[4] = (request_id >> 8) & 0xFF;
}

void can_audio_build_stop_all(can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_STOP_ALL;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 0;  // No data needed
}

// ============================================================================
// BUILDING FUNCTIONS (Audio Module Sends)
// ============================================================================

void can_audio_build_sound_status(uint8_t state_bits, uint16_t current_sound, 
                                  uint8_t error_code, uint8_t volume, uint16_t uptime,
                                  can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_SOUND_STATUS;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0: State bits
    frame->data[0] = state_bits;
    
    // Byte 1-2: Current sound index (little-endian)
    frame->data[1] = current_sound & 0xFF;
    frame->data[2] = (current_sound >> 8) & 0xFF;
    
    // Byte 3: Error code
    frame->data[3] = error_code;
    
    // Byte 4: Volume (0-100)
    frame->data[4] = volume;
    
    // Byte 5-6: Uptime in seconds (little-endian, wraps at 65535)
    frame->data[5] = uptime & 0xFF;
    frame->data[6] = (uptime >> 8) & 0xFF;
    
    // Byte 7: Active source count (could be added here)
    frame->data[7] = 0;
}

void can_audio_build_sound_ack(uint8_t ok, uint16_t sound_index, uint8_t queue_id,
                               uint8_t error_code, uint16_t request_id, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_SOUND_ACK;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // CRITICAL: Match CANBUS_MESSAGE_SPEC.md format exactly
    // Byte 0: Sound Index (echoed from request, 8-bit only)
    frame->data[0] = sound_index & 0xFF;
    
    // Byte 1: Status Code (0x00=success, 0x01=file not found, 0x02=mixer full, etc.)
    frame->data[1] = ok ? 0x00 : error_code;
    
    // Byte 2: Queue ID (1-255, or 0x00=invalid on error)
    frame->data[2] = queue_id;
    
    // Byte 3: Reserved (must be 0x00)
    frame->data[3] = 0x00;
    
    // Bytes 4-7: Reserved (must be 0x00)
    frame->data[4] = 0x00;
    frame->data[5] = 0x00;
    frame->data[6] = 0x00;
    
    // Byte 7: Reserved (already zeroed by memset)
    frame->data[7] = 0;
}

void can_audio_build_sound_finished(uint8_t queue_id, uint16_t sound_index, 
                                   uint8_t reason, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_SOUND_FINISHED;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0: Queue ID
    frame->data[0] = queue_id;
    
    // Byte 1-2: Sound index (little-endian)
    frame->data[1] = sound_index & 0xFF;
    frame->data[2] = (sound_index >> 8) & 0xFF;
    
    // Byte 3: Reason (0=completed, 1=stopped, 2=error)
    frame->data[3] = reason;
    
    // Byte 4-7: Reserved
    frame->data[4] = 0;
    frame->data[5] = 0;
    frame->data[6] = 0;
    frame->data[7] = 0;
}
