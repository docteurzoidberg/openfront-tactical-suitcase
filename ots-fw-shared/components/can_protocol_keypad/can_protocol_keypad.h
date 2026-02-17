#ifndef CAN_PROTOCOL_KEYPAD_H
#define CAN_PROTOCOL_KEYPAD_H

#include "can_driver.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @file can_protocol_keypad.h
 * @brief Keypad-specific CAN protocol for OTS 15-key keyboard module
 * 
 * This file defines the keypad module's CAN message format, IDs, and functions.
 * 
 * Protocol documentation:
 *   /prompts/CANBUS_MESSAGE_SPEC.md (protocol specification)
 *   /doc/developer/canbus-protocol.md (implementation guide)
 * 
 * Hardware: M5Stack Stamp S3 (ESP32-S3)
 * Keys: 15 mechanical switches (Cherry MX compatible)
 * LEDs: SK6812-MINI-E RGB (one per key)
 * Module Type: MODULE_TYPE_KEYPAD (0x02)
 */

// ============================================================================
// KEYPAD MODULE CAN MESSAGE IDs (0x430-0x43F block)
// ============================================================================

#define CAN_ID_KEYPAD_BASE          0x430  // Base ID for keypad module
#define CAN_ID_KEYPAD_LED_SET       0x430  // main → keypad (Set single key LED)
#define CAN_ID_KEYPAD_LED_BULK      0x440  // main → keypad (Set multiple key LEDs)

// KEY_EVENT CAN IDs are dynamic: CAN_ID_KEYPAD_BASE + key_id (1-15)
// Results in 0x431-0x43F for keys 1-15
#define CAN_ID_KEYPAD_KEY_EVENT(key_id)  (CAN_ID_KEYPAD_BASE + (key_id))

// ============================================================================
// KEY EVENT CONSTANTS
// ============================================================================

// Key states
#define CAN_KEYPAD_STATE_RELEASED   0x00   // Key released
#define CAN_KEYPAD_STATE_PRESSED    0x01   // Key pressed

// Key ID range
#define CAN_KEYPAD_KEY_MIN          1      // First key ID
#define CAN_KEYPAD_KEY_MAX          15     // Last key ID
#define CAN_KEYPAD_NUM_KEYS         15     // Total keys

// Special key IDs for LED commands
#define CAN_KEYPAD_ALL_KEYS         0xFF   // Apply to all keys

// ============================================================================
// LED CONSTANTS
// ============================================================================

// LED states
#define CAN_KEYPAD_LED_OFF          0x00   // LED off
#define CAN_KEYPAD_LED_ON           0x01   // LED on

// LED color presets (RGB values)
#define CAN_KEYPAD_COLOR_OFF        0, 0, 0
#define CAN_KEYPAD_COLOR_RED        255, 0, 0
#define CAN_KEYPAD_COLOR_GREEN      0, 255, 0
#define CAN_KEYPAD_COLOR_BLUE       0, 0, 255
#define CAN_KEYPAD_COLOR_YELLOW     255, 255, 0
#define CAN_KEYPAD_COLOR_CYAN       0, 255, 255
#define CAN_KEYPAD_COLOR_MAGENTA    255, 0, 255
#define CAN_KEYPAD_COLOR_WHITE      255, 255, 255

// ============================================================================
// MESSAGE STRUCTURES
// ============================================================================

/**
 * @brief KEY_EVENT message data (0x431-0x43F)
 * Direction: Keypad → Main Controller
 * DLC: 4 bytes
 */
typedef struct {
    uint8_t key_id;      // Key ID (1-15)
    uint8_t state;       // 0=released, 1=pressed
    uint16_t timestamp;  // Timestamp in milliseconds (little-endian)
} __attribute__((packed)) can_keypad_event_t;

/**
 * @brief LED_SET message data (0x430)
 * Direction: Main Controller → Keypad
 * DLC: 5 bytes
 */
typedef struct {
    uint8_t key_id;      // Key ID (1-15) or 0xFF for all keys
    uint8_t state;       // 0=off, 1=on
    uint8_t r;           // Red component (0-255)
    uint8_t g;           // Green component (0-255)
    uint8_t b;           // Blue component (0-255)
} __attribute__((packed)) can_keypad_led_set_t;

/**
 * @brief LED_BULK message data (0x440)
 * Direction: Main Controller → Keypad
 * DLC: 8 bytes
 */
typedef struct {
    uint16_t mask;       // Key bitmask (bit 0=K1, bit 14=K15, little-endian)
    uint8_t r;           // Red component (0-255)
    uint8_t g;           // Green component (0-255)
    uint8_t b;           // Blue component (0-255)
    uint8_t state;       // 0=off, 1=on
    uint8_t reserved[2]; // Reserved for future use
} __attribute__((packed)) can_keypad_led_bulk_t;

// ============================================================================
// PARSING FUNCTIONS (Keypad Module Receives)
// ============================================================================

/**
 * @brief Parse LED_SET message
 * @param frame CAN frame to parse
 * @param key_id Output: Key ID (1-15 or 0xFF for all)
 * @param state Output: LED state (0=off, 1=on)
 * @param r Output: Red component
 * @param g Output: Green component
 * @param b Output: Blue component
 * @return true if valid LED_SET message, false otherwise
 */
bool can_keypad_parse_led_set(const can_frame_t *frame, uint8_t *key_id,
                               uint8_t *state, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * @brief Parse LED_BULK message
 * @param frame CAN frame to parse
 * @param mask Output: Key bitmask (15 bits)
 * @param r Output: Red component
 * @param g Output: Green component
 * @param b Output: Blue component
 * @param state Output: LED state (0=off, 1=on)
 * @return true if valid LED_BULK message, false otherwise
 */
bool can_keypad_parse_led_bulk(const can_frame_t *frame, uint16_t *mask,
                                uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *state);

// ============================================================================
// BUILDING FUNCTIONS (Keypad Module Sends)
// ============================================================================

/**
 * @brief Build KEY_EVENT message for key press/release
 * @param key_id Key ID (1-15)
 * @param state Key state (0=released, 1=pressed)
 * @param timestamp Timestamp in milliseconds
 * @param frame Output CAN frame
 */
void can_keypad_build_key_event(uint8_t key_id, uint8_t state, 
                                 uint16_t timestamp, can_frame_t *frame);

// ============================================================================
// BUILDING FUNCTIONS (Main Controller Sends)
// ============================================================================

/**
 * @brief Build LED_SET message to set single key LED color and state
 * @param key_id Key ID (1-15) or 0xFF for all keys
 * @param state LED state (0=off, 1=on)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param frame Output CAN frame
 */
void can_keypad_build_led_set(uint8_t key_id, uint8_t state,
                               uint8_t r, uint8_t g, uint8_t b, can_frame_t *frame);

/**
 * @brief Build LED_BULK message to set multiple key LEDs at once
 * @param mask Key bitmask (bit 0=K1, bit 1=K2, ..., bit 14=K15)
 * @param state LED state (0=off, 1=on)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param frame Output CAN frame
 */
void can_keypad_build_led_bulk(uint16_t mask, uint8_t state,
                                uint8_t r, uint8_t g, uint8_t b, can_frame_t *frame);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Check if CAN ID is a key event (0x431-0x43F)
 * @param can_id CAN message ID
 * @return true if key event ID, false otherwise
 */
static inline bool can_keypad_is_key_event(uint32_t can_id) {
    return (can_id >= 0x431 && can_id <= 0x43F);
}

/**
 * @brief Extract key ID from key event CAN ID
 * @param can_id CAN message ID (0x431-0x43F)
 * @return Key ID (1-15), or 0 if invalid
 */
static inline uint8_t can_keypad_get_key_id_from_can_id(uint32_t can_id) {
    if (!can_keypad_is_key_event(can_id)) {
        return 0;
    }
    return (uint8_t)(can_id - CAN_ID_KEYPAD_BASE);
}

/**
 * @brief Validate key ID is in range
 * @param key_id Key ID to validate
 * @return true if valid (1-15), false otherwise
 */
static inline bool can_keypad_is_valid_key_id(uint8_t key_id) {
    return (key_id >= CAN_KEYPAD_KEY_MIN && key_id <= CAN_KEYPAD_KEY_MAX);
}

/**
 * @brief Create key bitmask for single key
 * @param key_id Key ID (1-15)
 * @return Bitmask with single bit set
 */
static inline uint16_t can_keypad_key_to_mask(uint8_t key_id) {
    if (!can_keypad_is_valid_key_id(key_id)) {
        return 0;
    }
    return (uint16_t)(1 << (key_id - 1));
}

/**
 * @brief Create key bitmask for multiple keys
 * @param key_ids Array of key IDs (1-15)
 * @param count Number of keys in array
 * @return Combined bitmask
 */
static inline uint16_t can_keypad_keys_to_mask(const uint8_t *key_ids, uint8_t count) {
    uint16_t mask = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (can_keypad_is_valid_key_id(key_ids[i])) {
            mask |= can_keypad_key_to_mask(key_ids[i]);
        }
    }
    return mask;
}

/**
 * @brief Create bitmask for all keys
 * @return Bitmask with all 15 key bits set
 */
static inline uint16_t can_keypad_all_keys_mask(void) {
    return 0x7FFF;  // 15 bits set (bits 0-14)
}

#endif // CAN_PROTOCOL_KEYPAD_H
