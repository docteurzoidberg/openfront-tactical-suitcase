#include "can_protocol.h"
#include <string.h>

/**
 * @brief Build a PLAY_SOUND CAN frame
 */
void can_build_play_sound(uint16_t sound_index, uint8_t flags, uint8_t volume_override, 
                          uint16_t request_id, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_PLAY_SOUND;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0-1: Sound index (little-endian)
    frame->data[0] = sound_index & 0xFF;
    frame->data[1] = (sound_index >> 8) & 0xFF;
    
    // Byte 2: Flags (interrupt, priority, loop)
    frame->data[2] = flags;
    
    // Byte 3: Volume override (0-100 or 0xFF for pot)
    frame->data[3] = volume_override;
    
    // Byte 4-5: Request ID (little-endian)
    frame->data[4] = request_id & 0xFF;
    frame->data[5] = (request_id >> 8) & 0xFF;
    
    // Byte 6-7: Reserved
    frame->data[6] = 0;
    frame->data[7] = 0;
}

/**
 * @brief Build a STOP_SOUND CAN frame
 */
void can_build_stop_sound(uint16_t sound_index, uint8_t flags, uint16_t request_id, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_STOP_SOUND;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0-1: Sound index (or 0xFFFF for any/current)
    frame->data[0] = sound_index & 0xFF;
    frame->data[1] = (sound_index >> 8) & 0xFF;
    
    // Byte 2: Flags (stop_all)
    frame->data[2] = flags;
    
    // Byte 3-4: Request ID
    frame->data[3] = request_id & 0xFF;
    frame->data[4] = (request_id >> 8) & 0xFF;
    
    // Byte 5-7: Reserved
    frame->data[5] = 0;
    frame->data[6] = 0;
    frame->data[7] = 0;
}

/**
 * @brief Parse a PLAY_SOUND frame (audio module receives this)
 */
bool can_parse_play_sound(const can_frame_t *frame, uint16_t *sound_index, 
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

/**
 * @brief Parse a STOP_SOUND frame (audio module receives this)
 */
bool can_parse_stop_sound(const can_frame_t *frame, uint16_t *sound_index, 
                         uint8_t *flags, uint16_t *request_id) {
    if (!frame || frame->id != CAN_ID_STOP_SOUND || frame->dlc < 5) {
        return false;
    }
    
    if (sound_index) {
        *sound_index = frame->data[0] | (frame->data[1] << 8);
    }
    if (flags) {
        *flags = frame->data[2];
    }
    if (request_id) {
        *request_id = frame->data[3] | (frame->data[4] << 8);
    }
    
    return true;
}

/**
 * @brief Build a SOUND_STATUS frame (audio module sends this)
 */
void can_build_sound_status(uint8_t state_bits, uint16_t current_sound, 
                           uint8_t error_code, uint8_t volume, uint16_t uptime,
                           can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_SOUND_STATUS;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0: State bits (ready, playing, muted, error)
    frame->data[0] = state_bits;
    
    // Byte 1-2: Current sound index (little-endian)
    frame->data[1] = current_sound & 0xFF;
    frame->data[2] = (current_sound >> 8) & 0xFF;
    
    // Byte 3: Error code
    frame->data[3] = error_code;
    
    // Byte 4: Volume (0-100 or 0xFF)
    frame->data[4] = volume;
    
    // Byte 5-6: Uptime in seconds (little-endian)
    frame->data[5] = uptime & 0xFF;
    frame->data[6] = (uptime >> 8) & 0xFF;
    
    // Byte 7: Reserved
    frame->data[7] = 0;
}

/**
 * @brief Build a SOUND_ACK frame (audio module sends this)
 */
void can_build_sound_ack(uint8_t ok, uint16_t sound_index, uint8_t error_code,
                        uint16_t request_id, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_SOUND_ACK;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0: OK flag (1=success, 0=failed)
    frame->data[0] = ok ? 1 : 0;
    
    // Byte 1-2: Echo of sound index
    frame->data[1] = sound_index & 0xFF;
    frame->data[2] = (sound_index >> 8) & 0xFF;
    
    // Byte 3: Error code (0 if ok)
    frame->data[3] = error_code;
    
    // Byte 4-5: Echo of request ID
    frame->data[4] = request_id & 0xFF;
    frame->data[5] = (request_id >> 8) & 0xFF;
    
    // Byte 6-7: Reserved
    frame->data[6] = 0;
    frame->data[7] = 0;
}
