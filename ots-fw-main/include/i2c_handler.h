#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// Idempotent init of the shared I2C master bus (I2C_NUM_0).
// Uses pins from config.h (I2C_SDA_PIN/I2C_SCL_PIN).
esp_err_t ots_i2c_bus_init(void);

// Returns the shared bus handle if initialized, else NULL.
i2c_master_bus_handle_t ots_i2c_bus_get(void);

#ifdef __cplusplus
}
#endif
