/**
 * @file i2c.h
 * @brief I2C Bus Hardware Abstraction Layer
 * 
 * Initializes I2C master for codec communication
 */

#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C configuration
#define I2C_PORT       I2C_NUM_0
#ifndef I2C_SDA_IO
#define I2C_SDA_IO     GPIO_NUM_33
#endif
#ifndef I2C_SCL_IO
#define I2C_SCL_IO     GPIO_NUM_32
#endif
#define I2C_FREQ_HZ    100000  // 100 kHz

/**
 * @brief Initialize I2C master bus
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t i2c_init(void);

/**
 * @brief Get I2C bus handle for adding devices
 * @return I2C master bus handle
 */
i2c_master_bus_handle_t i2c_get_bus_handle(void);

#ifdef __cplusplus
}
#endif
