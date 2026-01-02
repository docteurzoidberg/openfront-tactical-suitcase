#ifndef IO_TASK_H
#define IO_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize and start the dedicated I/O task
 * 
 * This task handles:
 * - Periodic button scanning
 * - LED state updates
 * - I/O expander communication
 * 
 * Must be called after io_expander_init() and module_io_init()
 * 
 * @return ESP_OK on success
 */
esp_err_t io_task_start(void);

#endif // IO_TASK_H
