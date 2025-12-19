#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <esp_err.h>
#include <stdint.h>

// Default I2C address for ADS1015
#define ADS1015_I2C_ADDR 0x48

// ADS1015 channels
#define ADS1015_CHANNEL_AIN0 0
#define ADS1015_CHANNEL_AIN1 1
#define ADS1015_CHANNEL_AIN2 2
#define ADS1015_CHANNEL_AIN3 3

/**
 * @brief Initialize ADS1015 ADC
 * 
 * @param i2c_addr I2C address (default: 0x48)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ads1015_init(uint8_t i2c_addr);

/**
 * @brief Read ADC value from channel
 * 
 * @param channel Channel number (0-3)
 * @return int16_t ADC value (0-4095 for 12-bit), -1 on error
 */
int16_t ads1015_read_channel(uint8_t channel);

#endif // ADC_DRIVER_H
