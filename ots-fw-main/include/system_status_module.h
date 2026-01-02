/**
 * @file system_status_module.h
 * @brief System Status Display Module - Shows boot/connection/lobby screens on LCD
 * 
 * This module manages the LCD display for non-game states:
 * - Boot splash screen
 * - Waiting for connection
 * - Connected / waiting for game
 * 
 * When game is active (IN_GAME), this module yields control to troops_module.
 */

#ifndef SYSTEM_STATUS_MODULE_H
#define SYSTEM_STATUS_MODULE_H

#include <esp_err.h>
#include "hardware_module.h"

/**
 * @brief Get the system status module instance
 * @return Pointer to hardware module interface
 */
const hardware_module_t* system_status_module_get(void);

/**
 * @brief Trigger a display refresh (mark display as dirty)
 * 
 * Call this to force the module to re-check connection/portal state
 * and update the LCD display accordingly.
 */
void system_status_refresh_display(void);

#endif // SYSTEM_STATUS_MODULE_H
