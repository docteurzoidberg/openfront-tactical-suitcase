#ifndef ALERT_MODULE_H
#define ALERT_MODULE_H

#include "hardware_module.h"

/**
 * @brief Alert module hardware interface
 * 
 * Handles 6 alert LEDs (output only)
 * 
 * Hardware mapping:
 * - Board 1 (OUTPUT): Pins 0-5 = Warning, Atom, Hydro, MIRV, Land, Naval LEDs
 */

// Pin definitions (on output board)
#define ALERT_OUTPUT_BOARD  1  // All outputs on board 1

#define ALERT_LED_WARNING_PIN  0
#define ALERT_LED_ATOM_PIN     1
#define ALERT_LED_HYDRO_PIN    2
#define ALERT_LED_MIRV_PIN     3
#define ALERT_LED_LAND_PIN     4
#define ALERT_LED_NAVAL_PIN    5

/**
 * @brief Get alert module instance
 * 
 * @return Pointer to alert module structure
 */
extern hardware_module_t alert_module;

#endif // ALERT_MODULE_H
