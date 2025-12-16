#ifndef HARDWARE_MODULE_H
#define HARDWARE_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "event_dispatcher.h"

/**
 * @brief Module status information
 */
typedef struct {
    bool initialized;
    bool operational;
    uint32_t error_count;
    char last_error[64];
} module_status_t;

/**
 * @brief Hardware module interface
 * 
 * All hardware modules implement this interface for standardized management
 */
typedef struct hardware_module {
    const char *name;                           // Module name
    bool enabled;                               // Module enabled flag
    
    /**
     * @brief Initialize module hardware
     * Called once during system startup
     * @return ESP_OK on success
     */
    esp_err_t (*init)(void);
    
    /**
     * @brief Update module state (periodic call)
     * Called regularly by module manager for time-based operations
     * @return ESP_OK on success
     */
    esp_err_t (*update)(void);
    
    /**
     * @brief Handle incoming event
     * @param event Event to process
     * @return true if event was handled
     */
    bool (*handle_event)(const internal_event_t *event);
    
    /**
     * @brief Get module status
     * @param status Output status structure
     */
    void (*get_status)(module_status_t *status);
    
    /**
     * @brief Shutdown/cleanup module
     * @return ESP_OK on success
     */
    esp_err_t (*shutdown)(void);
    
} hardware_module_t;

#endif // HARDWARE_MODULE_H
