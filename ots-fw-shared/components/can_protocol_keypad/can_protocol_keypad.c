#include "can_protocol_keypad.h"
#include <string.h>

/**
 * @file can_protocol_keypad.c
 * @brief Keypad-specific CAN protocol implementation
 * 
 * Implements message parsing and building functions for the OTS keypad module.
 */

// ============================================================================
// PARSING FUNCTIONS (Keypad Module Receives)
// ============================================================================

bool can_keypad_parse_led_set(const can_frame_t *frame, uint8_t *key_id,
                               uint8_t *state, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!frame || frame->id != CAN_ID_KEYPAD_LED_SET || frame->dlc < 5) {
        return false;
    }
    
    if (key_id) {
        *key_id = frame->data[0];
    }
    if (state) {
        *state = frame->data[1];
    }
    if (r) {
        *r = frame->data[2];
    }
    if (g) {
        *g = frame->data[3];
    }
    if (b) {
        *b = frame->data[4];
    }
    
    return true;
}

bool can_keypad_parse_led_bulk(const can_frame_t *frame, uint16_t *mask,
                                uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *state) {
    if (!frame || frame->id != CAN_ID_KEYPAD_LED_BULK || frame->dlc < 8) {
        return false;
    }
    
    if (mask) {
        *mask = frame->data[0] | (frame->data[1] << 8);  // Little-endian
    }
    if (r) {
        *r = frame->data[2];
    }
    if (g) {
        *g = frame->data[3];
    }
    if (b) {
        *b = frame->data[4];
    }
    if (state) {
        *state = frame->data[5];
    }
    
    return true;
}

// ============================================================================
// BUILDING FUNCTIONS (Keypad Module Sends)
// ============================================================================

void can_keypad_build_key_event(uint8_t key_id, uint8_t state, 
                                 uint16_t timestamp, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    // Dynamic CAN ID: 0x430 + key_id
    frame->id = CAN_ID_KEYPAD_BASE + key_id;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 4;
    
    // Byte 0: Key ID (1-15)
    frame->data[0] = key_id;
    
    // Byte 1: State (0=released, 1=pressed)
    frame->data[1] = state;
    
    // Byte 2-3: Timestamp (little-endian)
    frame->data[2] = timestamp & 0xFF;
    frame->data[3] = (timestamp >> 8) & 0xFF;
}

// ============================================================================
// BUILDING FUNCTIONS (Main Controller Sends)
// ============================================================================

void can_keypad_build_led_set(uint8_t key_id, uint8_t state,
                               uint8_t r, uint8_t g, uint8_t b, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_KEYPAD_LED_SET;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 5;
    
    // Byte 0: Key ID (1-15 or 0xFF for all)
    frame->data[0] = key_id;
    
    // Byte 1: State (0=off, 1=on)
    frame->data[1] = state;
    
    // Byte 2-4: RGB color
    frame->data[2] = r;
    frame->data[3] = g;
    frame->data[4] = b;
}

void can_keypad_build_led_bulk(uint16_t mask, uint8_t state,
                                uint8_t r, uint8_t g, uint8_t b, can_frame_t *frame) {
    memset(frame, 0, sizeof(can_frame_t));
    
    frame->id = CAN_ID_KEYPAD_LED_BULK;
    frame->extended = false;
    frame->rtr = false;
    frame->dlc = 8;
    
    // Byte 0-1: Key bitmask (little-endian)
    frame->data[0] = mask & 0xFF;
    frame->data[1] = (mask >> 8) & 0xFF;
    
    // Byte 2-4: RGB color
    frame->data[2] = r;
    frame->data[3] = g;
    frame->data[4] = b;
    
    // Byte 5: State (0=off, 1=on)
    frame->data[5] = state;
    
    // Byte 6-7: Reserved
    frame->data[6] = 0x00;
    frame->data[7] = 0x00;
}
