#ifndef MAIN_POWER_MODULE_H
#define MAIN_POWER_MODULE_H

#include "hardware_module.h"

/**
 * @brief Main power module hardware interface
 * 
 * Handles the main link LED (output only)
 * 
 * Hardware mapping:
 * - Board 1 (OUTPUT): Pin 7 = Link LED
 */

// Pin definitions (on output board)
#define MAIN_POWER_OUTPUT_BOARD  1  // All outputs on board 1

#define MAIN_POWER_LINK_LED_PIN  7

/**
 * @brief Get main power module instance
 * 
 * @return Pointer to main power module structure
 */
extern hardware_module_t main_power_module;

#endif // MAIN_POWER_MODULE_H
