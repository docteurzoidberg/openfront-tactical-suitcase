#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "hardware_module.h"

/**
 * @brief Initialize module manager
 * 
 * Must be called after I/O expanders are globally configured
 * 
 * @return ESP_OK on success
 */
esp_err_t module_manager_init(void);

/**
 * @brief Register a hardware module
 * 
 * @param module Pointer to module structure
 * @return ESP_OK on success
 */
esp_err_t module_manager_register(hardware_module_t *module);

/**
 * @brief Initialize all registered modules
 * 
 * Calls init() on each registered module
 * 
 * @return ESP_OK if all modules initialized successfully
 */
esp_err_t module_manager_init_all(void);

/**
 * @brief Update all modules (periodic)
 * 
 * Calls update() on each enabled module
 * Should be called regularly (e.g., from main loop or task)
 * 
 * @return ESP_OK on success
 */
esp_err_t module_manager_update_all(void);

/**
 * @brief Route event to all modules
 * 
 * Calls handle_event() on each module until one handles it
 * 
 * @param event Event to route
 * @return true if any module handled the event
 */
bool module_manager_route_event(const internal_event_t *event);

/**
 * @brief Get count of registered modules
 * 
 * @return Number of registered modules
 */
uint8_t module_manager_get_count(void);

/**
 * @brief Get module by index
 * 
 * @param index Module index
 * @return Pointer to module or NULL if invalid index
 */
hardware_module_t* module_manager_get_module(uint8_t index);

#endif // MODULE_MANAGER_H
