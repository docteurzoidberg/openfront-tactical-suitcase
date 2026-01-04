#include "es8388.h"
#include "i2c.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ES8388";
static i2c_master_dev_handle_t es8388_dev = NULL;

// ES8388 I2C address (detected at 0x10)
#define ES8388_ADDR 0x10

// ES8388 Register addresses
#define ES8388_CONTROL1         0x00
#define ES8388_CONTROL2         0x01
#define ES8388_CHIPPOWER        0x02
#define ES8388_ADCPOWER         0x03
#define ES8388_DACPOWER         0x04
#define ES8388_CHIPLOPOW1       0x05
#define ES8388_CHIPLOPOW2       0x06
#define ES8388_ANAVOLMANAG      0x07
#define ES8388_MASTERMODE       0x08
#define ES8388_ADCCONTROL1      0x09
#define ES8388_ADCCONTROL2      0x0A
#define ES8388_ADCCONTROL3      0x0B
#define ES8388_ADCCONTROL4      0x0C
#define ES8388_ADCCONTROL5      0x0D
#define ES8388_ADCCONTROL6      0x0E
#define ES8388_ADCCONTROL7      0x0F
#define ES8388_ADCCONTROL8      0x10
#define ES8388_ADCCONTROL9      0x11
#define ES8388_ADCCONTROL10     0x12
#define ES8388_ADCCONTROL11     0x13
#define ES8388_ADCCONTROL12     0x14
#define ES8388_ADCCONTROL13     0x15
#define ES8388_ADCCONTROL14     0x16
#define ES8388_DACCONTROL1      0x17
#define ES8388_DACCONTROL2      0x18
#define ES8388_DACCONTROL3      0x19
#define ES8388_DACCONTROL4      0x1A
#define ES8388_DACCONTROL5      0x1B
#define ES8388_DACCONTROL6      0x1C
#define ES8388_DACCONTROL7      0x1D
#define ES8388_DACCONTROL8      0x1E
#define ES8388_DACCONTROL9      0x1F
#define ES8388_DACCONTROL10     0x20
#define ES8388_DACCONTROL11     0x21
#define ES8388_DACCONTROL12     0x22
#define ES8388_DACCONTROL13     0x23
#define ES8388_DACCONTROL14     0x24
#define ES8388_DACCONTROL15     0x25
#define ES8388_DACCONTROL16     0x26
#define ES8388_DACCONTROL17     0x27
#define ES8388_DACCONTROL18     0x28
#define ES8388_DACCONTROL19     0x29
#define ES8388_DACCONTROL20     0x2A
#define ES8388_DACCONTROL21     0x2B
#define ES8388_DACCONTROL22     0x2C
#define ES8388_DACCONTROL23     0x2D
#define ES8388_DACCONTROL24     0x2E
#define ES8388_DACCONTROL25     0x2F
#define ES8388_DACCONTROL26     0x30
#define ES8388_DACCONTROL27     0x31
#define ES8388_DACCONTROL28     0x32
#define ES8388_DACCONTROL29     0x33
#define ES8388_DACCONTROL30     0x34

/**
 * @brief Write to ES8388 register via I2C
 */
static esp_err_t es8388_write_reg(uint8_t reg_addr, uint8_t val) {
    if (!es8388_dev) {
        ESP_LOGE(TAG, "ES8388 device not initialized");
        return ESP_FAIL;
    }
    
    uint8_t data[2] = {reg_addr, val};
    esp_err_t ret = i2c_master_transmit(es8388_dev, data, sizeof(data), 1000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write reg 0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Read from ES8388 register via I2C
 */
static uint8_t es8388_read_reg(uint8_t reg_addr) {
    if (!es8388_dev) {
        ESP_LOGE(TAG, "ES8388 device not initialized");
        return 0;
    }
    
    uint8_t data = 0;
    
    esp_err_t ret = i2c_master_transmit_receive(es8388_dev, &reg_addr, 1, &data, 1, 1000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read reg 0x%02X: %s", reg_addr, esp_err_to_name(ret));
        return 0;
    }
    
    return data;
}

esp_err_t es8388_init(uint32_t sample_rate) {
    ESP_LOGI(TAG, "Initializing ES8388 codec @ %lu Hz", sample_rate);
    
    // Get I2C bus handle
    i2c_master_bus_handle_t bus_handle = i2c_get_bus_handle();
    if (!bus_handle) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_FAIL;
    }
    
    // Create I2C device handle for ES8388
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8388_ADDR,
        .scl_speed_hz = 100000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &es8388_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ES8388 device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Reset and power-up sequence
    
    // 1. Reset all registers to default
    ret = es8388_write_reg(ES8388_CONTROL1, 0x80);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soft reset failed!");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = es8388_write_reg(ES8388_CONTROL1, 0x00);
    if (ret != ESP_OK) return ret;
    
    // 2. Configure chip power management
    ret = es8388_write_reg(ES8388_CHIPPOWER, 0x00);  // Power up all blocks
    if (ret != ESP_OK) return ret;
    
    // 3. Configure ADC and DAC power
    ret = es8388_write_reg(ES8388_DACPOWER, 0x3C);   // Power up DAC
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_CONTROL2, 0x50);   // Enable DAC reference
    if (ret != ESP_OK) return ret;
    
    // 4. Configure master/slave mode (slave mode, I2S format)
    ret = es8388_write_reg(ES8388_MASTERMODE, 0x00); // Slave mode
    if (ret != ESP_OK) return ret;
    
    // 5. Configure DAC control (16-bit I2S format)
    // 0x18 = standard 16-bit I2S format (verified from ESP-ADF source)
    ret = es8388_write_reg(ES8388_DACCONTROL1, 0x18); // 16-bit I2S
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL2, 0x02); // DACFsMode, single speed
    if (ret != ESP_OK) return ret;
    
    // 6. Configure output mixer
    ret = es8388_write_reg(ES8388_DACCONTROL16, 0x00); // LOUT1 from LDAC
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL17, 0x90); // ROUT1 from RDAC
    if (ret != ESP_OK) return ret;
    
    // 7. Set initial volume (moderate level)
    ret = es8388_write_reg(ES8388_DACCONTROL24, 0x1E); // LOUT1 volume
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL25, 0x1E); // ROUT1 volume
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL26, 0x1E); // LOUT2 volume
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL27, 0x1E); // ROUT2 volume
    if (ret != ESP_OK) return ret;
    
    // 8. Enable DAC volume control
    ret = es8388_write_reg(ES8388_DACCONTROL5, 0x00);  // Volume not muted
    if (ret != ESP_OK) return ret;
    
    // 9. Power up outputs
    ret = es8388_write_reg(ES8388_DACPOWER, 0x00);     // All DAC power on
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_CHIPLOPOW1, 0x00);   // All output power on
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_CHIPLOPOW2, 0x00);
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "ES8388 initialized successfully");
    
    return ESP_OK;
}

esp_err_t es8388_set_speaker_volume(uint8_t volume) {
    if (!es8388_dev) {
        ESP_LOGE(TAG, "ES8388 not initialized");
        return ESP_FAIL;
    }
    
    // Map 0-100 to ES8388 volume range (0x00 to 0x21)
    // 0x00 = +4.5dB, 0x21 = -45dB, each step is 1.5dB
    // Invert: 100 -> 0x00, 0 -> 0x21
    uint8_t vol_reg = (volume > 100) ? 0x00 : (0x21 - (volume * 0x21 / 100));
    
    ESP_LOGI(TAG, "Setting speaker volume to %d%% (reg: 0x%02X)", volume, vol_reg);
    
    // Set LOUT2/ROUT2 (speaker outputs)
    esp_err_t ret = es8388_write_reg(ES8388_DACCONTROL26, vol_reg);
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL27, vol_reg);
    
    return ret;
}

esp_err_t es8388_set_headphone_volume(uint8_t volume) {
    if (!es8388_dev) {
        ESP_LOGE(TAG, "ES8388 not initialized");
        return ESP_FAIL;
    }
    
    // Map 0-100 to ES8388 volume range
    uint8_t vol_reg = (volume > 100) ? 0x00 : (0x21 - (volume * 0x21 / 100));
    
    ESP_LOGI(TAG, "Setting headphone volume to %d%% (reg: 0x%02X)", volume, vol_reg);
    
    // Set LOUT1/ROUT1 (headphone outputs)
    esp_err_t ret = es8388_write_reg(ES8388_DACCONTROL24, vol_reg);
    if (ret != ESP_OK) return ret;
    
    ret = es8388_write_reg(ES8388_DACCONTROL25, vol_reg);
    
    return ret;
}

esp_err_t es8388_set_speaker_enable(bool enable) {
    if (!es8388_dev) {
        ESP_LOGE(TAG, "ES8388 not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "%s speaker output", enable ? "Enabling" : "Disabling");
    
    // Control LOUT2/ROUT2 power (speaker outputs)
    uint8_t power_reg = enable ? 0x00 : 0x0C;  // Bit 2-3: LOUT2/ROUT2 power down
    
    return es8388_write_reg(ES8388_CHIPLOPOW2, power_reg);
}
