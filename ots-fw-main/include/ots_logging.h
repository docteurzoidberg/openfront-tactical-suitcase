#ifndef OTS_LOGGING_H
#define OTS_LOGGING_H

#include "esp_err.h"

/**
 * @brief Initialize firmware logging configuration.
 *
 * Applies the global log level filter for the serial console.
 *
 * Controlled via `OTS_LOG_LEVEL` (see include/config.h).
 */
esp_err_t ots_logging_init(void);

#endif // OTS_LOGGING_H
