#include "can_protocol.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CAN_PROTO";

/**
 * @brief Build a PLAY_SOUND CAN frame
 */
void can_build_play_sound(uint16_t sound_index, uint8_t flags, uint8_t volume_override,
                          uint16_t request_id, can_frame_t *frame) {
    if (!frame) return;
    
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = CAN_ID_PLAY_SOUND;
    frame->dlc = 8;
    frame->extended = false;
    frame->rtr = false;
    frame->data[0] = CAN_CMD_PLAY_SOUND;
    frame->data[1] = flags;
    frame->data[2] = (uint8_t)(sound_index & 0xFF);        // soundIndex low byte
    frame->data[3] = (uint8_t)((sound_index >> 8) & 0xFF); // soundIndex high byte
    frame->data[4] = volume_override;
    frame->data[5] = 0;  // reserved
    frame->data[6] = (uint8_t)(request_id & 0xFF);         // requestId low byte
    frame->data[7] = (uint8_t)((request_id >> 8) & 0xFF);  // requestId high byte
    
    // Log decoded message
    ESP_LOGI(TAG, "Build PLAY_SOUND: idx=%u flags=0x%02X vol=%u reqID=%u",
             sound_index, flags, volume_override, request_id);
}

/**
 * @brief Build a STOP_SOUND CAN frame
 */
void can_build_stop_sound(uint16_t sound_index, uint8_t flags, uint16_t request_id, can_frame_t *frame) {
    if (!frame) return;
    
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = CAN_ID_STOP_SOUND;
    frame->dlc = 8;
    frame->extended = false;
    frame->rtr = false;
    frame->data[0] = CAN_CMD_STOP_SOUND;
    frame->data[1] = flags;
    frame->data[2] = (uint8_t)(sound_index & 0xFF);        // soundIndex low byte
    frame->data[3] = (uint8_t)((sound_index >> 8) & 0xFF); // soundIndex high byte
    frame->data[4] = 0;  // reserved
    frame->data[5] = 0;  // reserved
    frame->data[6] = (uint8_t)(request_id & 0xFF);         // requestId low byte
    frame->data[7] = (uint8_t)((request_id >> 8) & 0xFF);  // requestId high byte
    
    ESP_LOGI(TAG, "Build STOP_SOUND: idx=%u flags=0x%02X reqID=%u",
             sound_index, flags, request_id);
}

/**
 * @brief Parse a SOUND_STATUS frame
 */
bool can_parse_sound_status(const can_frame_t *frame, uint8_t *state_bits,
                           uint16_t *current_sound, uint8_t *error_code,
                           uint8_t *volume, uint16_t *uptime) {
    if (!frame || frame->id != CAN_ID_SOUND_STATUS || frame->dlc < 8) {
        return false;
    }
    
    if (frame->data[0] != CAN_CMD_STATUS) {
        return false;
    }
    
    if (state_bits) *state_bits = frame->data[1];
    if (current_sound) *current_sound = frame->data[2] | (frame->data[3] << 8);
    if (error_code) *error_code = frame->data[4];
    if (volume) *volume = frame->data[5];
    if (uptime) *uptime = frame->data[6] | (frame->data[7] << 8);
    
    return true;
}

/**
 * @brief Parse a SOUND_ACK frame
 */
bool can_parse_sound_ack(const can_frame_t *frame, uint8_t *ok,
                        uint16_t *sound_index, uint8_t *error_code,
                        uint16_t *request_id) {
    if (!frame || frame->id != CAN_ID_SOUND_ACK || frame->dlc < 8) {
        return false;
    }
    
    if (frame->data[0] != CAN_CMD_ACK) {
        return false;
    }
    
    if (ok) *ok = frame->data[1];
    if (sound_index) *sound_index = frame->data[2] | (frame->data[3] << 8);
    if (error_code) *error_code = frame->data[4];
    if (request_id) *request_id = frame->data[6] | (frame->data[7] << 8);
    
    return true;
}
