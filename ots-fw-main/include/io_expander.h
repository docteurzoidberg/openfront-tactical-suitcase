#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX_MCP_BOARDS 2

typedef enum {
    IO_MODE_INPUT = 0,
    IO_MODE_OUTPUT = 1,
    IO_MODE_INPUT_PULLUP = 2
} io_mode_t;

bool io_expander_begin(const uint8_t *addresses, uint8_t count);
bool io_expander_set_pin_mode(uint8_t board, uint8_t pin, io_mode_t mode);
bool io_expander_digital_write(uint8_t board, uint8_t pin, uint8_t value);
bool io_expander_digital_read(uint8_t board, uint8_t pin, uint8_t *value);
bool io_expander_is_initialized(void);
uint8_t io_expander_get_board_count(void);

#endif // IO_EXPANDER_H
