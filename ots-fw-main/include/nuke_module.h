#ifndef NUKE_MODULE_H
#define NUKE_MODULE_H

#include "hardware_module.h"

/**
 * @brief Nuke module hardware interface
 * 
 * Handles 3 nuke buttons (input) and 3 nuke LEDs (output)
 * 
 * Hardware mapping:
 * - Board 0 (INPUT): Pins 1,2,3 = Atom, Hydro, MIRV buttons
 * - Board 1 (OUTPUT): Pins 8,9,10 = Atom, Hydro, MIRV LEDs
 */

// Pin definitions (on respective boards)
#define NUKE_INPUT_BOARD   0  // All inputs on board 0
#define NUKE_OUTPUT_BOARD  1  // All outputs on board 1

#define NUKE_BTN_ATOM_PIN   1
#define NUKE_BTN_HYDRO_PIN  2
#define NUKE_BTN_MIRV_PIN   3

#define NUKE_LED_ATOM_PIN   8
#define NUKE_LED_HYDRO_PIN  9
#define NUKE_LED_MIRV_PIN   10

/**
 * @brief Get nuke module instance
 * 
 * @return Pointer to nuke module structure
 */
extern hardware_module_t nuke_module;

#endif // NUKE_MODULE_H
