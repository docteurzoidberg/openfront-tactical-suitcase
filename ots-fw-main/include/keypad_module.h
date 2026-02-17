#ifndef KEYPAD_MODULE_H
#define KEYPAD_MODULE_H

#include "hardware_module.h"

/**
 * @brief Keypad module interface
 * 
 * Handles CAN bus communication with external 15-key keyboard module
 * - Receives key events via CAN (0x431-0x43F)
 * - Sends LED commands via CAN (0x430, 0x440)
 * - Forwards key events to WebSocket
 * 
 * Hardware: M5Stack Stamp S3 keypad module (external, CAN-connected)
 * Keys: 15 mechanical switches (Cherry MX compatible)
 * LEDs: SK6812-MINI-E RGB (one per key)
 */

/**
 * @brief Get keypad module instance
 * 
 * @return Pointer to keypad module structure
 */
extern hardware_module_t keypad_module;

#endif // KEYPAD_MODULE_H
