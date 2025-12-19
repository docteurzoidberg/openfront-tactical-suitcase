#include "adc_driver.h"
#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char* TAG = "OTS_ADC";

// ADS1015 registers
#define ADS1015_REG_POINTER_CONVERSION  0x00
#define ADS1015_REG_POINTER_CONFIG      0x01

// ADS1015 config bits
#define ADS1015_CONFIG_OS_SINGLE        0x8000  // Start single conversion
#define ADS1015_CONFIG_MUX_AIN0_GND     0x4000  // AIN0 to GND
#define ADS1015_CONFIG_PGA_4_096V       0x0200  // ±4.096V range
#define ADS1015_CONFIG_MODE_SINGLE      0x0100  // Single-shot mode
#define ADS1015_CONFIG_DR_1600SPS       0x0080  // 1600 samples/sec
#define ADS1015_CONFIG_COMP_QUE_DISABLE 0x0003  // Disable comparator

static uint8_t adc_addr = ADS1015_I2C_ADDR;

// I2C helpers
static esp_err_t i2c_write_reg16(uint8_t reg, uint16_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (adc_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, (value >> 8) & 0xFF, true);  // MSB first
    i2c_master_write_byte(cmd, value & 0xFF, true);         // LSB
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read_reg16(uint8_t reg, uint16_t* value) {
    uint8_t msb, lsb;
    
    // Write register address
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (adc_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) return ret;
    
    // Read 2 bytes
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (adc_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &msb, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &lsb, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        *value = (msb << 8) | lsb;
    }
    
    return ret;
}

esp_err_t ads1015_init(uint8_t i2c_addr) {
    adc_addr = i2c_addr;
    
    // Test communication
    uint16_t config = ADS1015_CONFIG_OS_SINGLE |
                      ADS1015_CONFIG_MUX_AIN0_GND |
                      ADS1015_CONFIG_PGA_4_096V |
                      ADS1015_CONFIG_MODE_SINGLE |
                      ADS1015_CONFIG_DR_1600SPS |
                      ADS1015_CONFIG_COMP_QUE_DISABLE;
    
    esp_err_t ret = i2c_write_reg16(ADS1015_REG_POINTER_CONFIG, config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADS1015 initialized at 0x%02x", adc_addr);
    } else {
        ESP_LOGE(TAG, "Failed to initialize ADS1015: %s", esp_err_to_name(ret));
    }
    return ret;
}

int16_t ads1015_read_channel(uint8_t channel) {
    // Configure for single-shot conversion
    uint16_t mux_config = ADS1015_CONFIG_MUX_AIN0_GND;
    switch (channel) {
        case 0: mux_config = ADS1015_CONFIG_MUX_AIN0_GND; break;
        case 1: mux_config = 0x5000; break;  // AIN1
        case 2: mux_config = 0x6000; break;  // AIN2
        case 3: mux_config = 0x7000; break;  // AIN3
        default: return -1;
    }
    
    uint16_t config = ADS1015_CONFIG_OS_SINGLE |
                      mux_config |
                      ADS1015_CONFIG_PGA_4_096V |
                      ADS1015_CONFIG_MODE_SINGLE |
                      ADS1015_CONFIG_DR_1600SPS |
                      ADS1015_CONFIG_COMP_QUE_DISABLE;
    
    // Start conversion
    if (i2c_write_reg16(ADS1015_REG_POINTER_CONFIG, config) != ESP_OK) {
        return -1;
    }
    
    // Wait for conversion (max 1ms at 1600 SPS)
    vTaskDelay(pdMS_TO_TICKS(2));
    
    // Read result
    uint16_t value;
    if (i2c_read_reg16(ADS1015_REG_POINTER_CONVERSION, &value) != ESP_OK) {
        return -1;
    }
    
    // ADS1015 is 12-bit, left-aligned in 16-bit register
    return (int16_t)value >> 4;
}
