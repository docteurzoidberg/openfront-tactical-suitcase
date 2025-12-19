#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX_MCP_BOARDS 2

// Error recovery configuration
#define IO_EXPANDER_MAX_RETRIES 5
#define IO_EXPANDER_INITIAL_RETRY_DELAY_MS 100
#define IO_EXPANDER_MAX_RETRY_DELAY_MS 5000
#define IO_EXPANDER_HEALTH_CHECK_INTERVAL_MS 10000
#define IO_EXPANDER_MAX_CONSECUTIVE_ERRORS 3

typedef enum {
    IO_MODE_INPUT = 0,
    IO_MODE_OUTPUT = 1,
    IO_MODE_INPUT_PULLUP = 2
} io_mode_t;

/**
 * @brief I/O expander health status
 */
typedef struct {
    bool initialized;
    bool healthy;
    uint32_t error_count;
    uint32_t consecutive_errors;
    uint32_t recovery_count;
    uint64_t last_error_time;
    uint64_t last_health_check;
} io_expander_health_t;

/**
 * @brief Recovery callback function type
 */
typedef void (*io_expander_recovery_callback_t)(uint8_t board, bool was_down);

bool io_expander_begin(const uint8_t *addresses, uint8_t count);
esp_err_t io_expander_reinit_board(uint8_t board);
bool io_expander_health_check(void);
bool io_expander_get_health(uint8_t board, io_expander_health_t *status);
void io_expander_set_recovery_callback(io_expander_recovery_callback_t callback);
uint8_t io_expander_attempt_recovery(void);
void io_expander_reset_errors(uint8_t board);

bool io_expander_set_pin_mode(uint8_t board, uint8_t pin, io_mode_t mode);
bool io_expander_digital_write(uint8_t board, uint8_t pin, uint8_t value);
bool io_expander_digital_read(uint8_t board, uint8_t pin, uint8_t *value);
bool io_expander_is_initialized(void);
uint8_t io_expander_get_board_count(void);

#endif // IO_EXPANDER_H
