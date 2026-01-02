#include "adc_driver.h"
#include <driver/i2c_master.h>
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
static i2c_master_dev_handle_t adc_device = NULL;

// I2C helpers (NEW driver)
static esp_err_t i2c_write_reg16(uint8_t reg, uint16_t value) {
    if (!adc_device) return ESP_FAIL;
    
    uint8_t data[3] = {
        reg,
        (value >> 8) & 0xFF,  // MSB first
        value & 0xFF          // LSB
    };
    
    return i2c_master_transmit(adc_device, data, 3, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t i2c_read_reg16(uint8_t reg, uint16_t* value) {
    if (!adc_device || !value) return ESP_FAIL;
    
    uint8_t data[2];
    
    // Write register address, then read 2 bytes
    esp_err_t ret = i2c_master_transmit(adc_device, &reg, 1, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) return ret;
    
    ret = i2c_master_receive(adc_device, data, 2, 1000 / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
        *value = (data[0] << 8) | data[1];
    }
    
    return ret;
}

esp_err_t ads1015_init(i2c_master_bus_handle_t bus, uint8_t i2c_addr) {
    adc_addr = i2c_addr;
    
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Add ADC device to bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = adc_addr,
        .scl_speed_hz = 100000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_config, &adc_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ADC device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Test communication
    uint16_t config = ADS1015_CONFIG_OS_SINGLE |
                      ADS1015_CONFIG_MUX_AIN0_GND |
                      ADS1015_CONFIG_PGA_4_096V |
                      ADS1015_CONFIG_MODE_SINGLE |
                      ADS1015_CONFIG_DR_1600SPS |
                      ADS1015_CONFIG_COMP_QUE_DISABLE;
    
    ret = i2c_write_reg16(ADS1015_REG_POINTER_CONFIG, config);
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
