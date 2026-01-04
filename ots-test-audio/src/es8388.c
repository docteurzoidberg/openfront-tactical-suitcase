/**
 * @file es8388.c
 * @brief ES8388 codec driver implementation
 * Based on ESP-ADF es8388.c
 */

#include "es8388.h"
#include "board_config.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ES8388";

// ES8388 registers (from ESP-ADF)
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
#define ES8388_ADCCONTROL2      0x0a
#define ES8388_ADCCONTROL3      0x0b
#define ES8388_ADCCONTROL4      0x0c
#define ES8388_ADCCONTROL5      0x0d
#define ES8388_ADCCONTROL6      0x0e
#define ES8388_ADCCONTROL7      0x0f
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
#define ES8388_DACCONTROL4      0x1a
#define ES8388_DACCONTROL5      0x1b
#define ES8388_DACCONTROL6      0x1c
#define ES8388_DACCONTROL7      0x1d
#define ES8388_DACCONTROL8      0x1e
#define ES8388_DACCONTROL9      0x1f
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
#define ES8388_DACCONTROL20     0x2a
#define ES8388_DACCONTROL21     0x2b
#define ES8388_DACCONTROL22     0x2c
#define ES8388_DACCONTROL23     0x2d
#define ES8388_DACCONTROL24     0x2e
#define ES8388_DACCONTROL25     0x2f
#define ES8388_DACCONTROL26     0x30
#define ES8388_DACCONTROL27     0x31
#define ES8388_DACCONTROL28     0x32
#define ES8388_DACCONTROL29     0x33
#define ES8388_DACCONTROL30     0x34

// DLL registers for improved sample rate handling
#define ES8388_LRCMHIGH         0x35
#define ES8388_LRCMLOW          0x36
#define ES8388_SDPHIGH          0x37
#define ES8388_SDPLOW           0x38
#define ES8388_SCLKMODE         0x39

static esp_err_t es8388_write_reg(uint8_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8388_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

static esp_err_t i2c_bus_init(void)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &i2c_cfg);
    if (ret != ESP_OK) return ret;
    
    return i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t es8388_codec_init(void)
{
    esp_err_t ret = ESP_OK;
    
    // Configure power amplifier GPIO but keep it OFF initially
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PA_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PA_ENABLE_GPIO, 0);  // Keep PA OFF during init
    ESP_LOGI(TAG, "Power amplifier configured (GPIO %d) - keeping OFF", PA_ENABLE_GPIO);
    
    // Initialize I2C
    ret = i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C initialized");
    
    // ES8388 initialization sequence (from ESP-ADF)
    ESP_LOGI(TAG, "Initializing ES8388 codec...");
    
    // Mute DAC first (soft ramp disabled)
    ret |= es8388_write_reg(ES8388_DACCONTROL3, 0x04);
    
    // Power management
    ret |= es8388_write_reg(ES8388_CONTROL2, 0x50);     // VREF setup
    ret |= es8388_write_reg(ES8388_CHIPPOWER, 0x00);    // Power up all blocks
    
    // Disable internal DLL for better sample rate handling
    ret |= es8388_write_reg(0x35, 0xA0);
    ret |= es8388_write_reg(0x37, 0xD0);
    ret |= es8388_write_reg(0x39, 0xD0);
    
    // Slave mode (ESP32 is I2S master)
    ret |= es8388_write_reg(ES8388_MASTERMODE, 0x00);
    
    // Power down DAC during configuration
    ret |= es8388_write_reg(ES8388_DACPOWER, 0xC0);     // Disable DAC
    ret |= es8388_write_reg(ES8388_CONTROL1, 0x12);     // Play mode
    
    // DAC I2S configuration - 16-bit I2S format (from ESP-ADF: 0x18 = 16bit iis)
    ret |= es8388_write_reg(ES8388_DACCONTROL1, 0x18);
    ret |= es8388_write_reg(ES8388_DACCONTROL2, 0x02);      // Single speed, 256x
    
    // DAC digital volume (0dB)
    ret |= es8388_write_reg(ES8388_DACCONTROL4, 0x00);
    ret |= es8388_write_reg(ES8388_DACCONTROL5, 0x00);
    
    // Keep DAC muted during initialization (will unmute in es8388_start)
    ret |= es8388_write_reg(ES8388_DACCONTROL3, 0x04);  // Muted
    
    // DAC to output mixer enable
    ret |= es8388_write_reg(ES8388_DACCONTROL16, 0x00);     // LLIN1-LOUT1, RLIN1-ROUT1
    ret |= es8388_write_reg(ES8388_DACCONTROL17, 0x90);     // Left DAC to left mixer
    ret |= es8388_write_reg(ES8388_DACCONTROL20, 0x90);     // Right DAC to right mixer
    
    // Output volume (MAXIMUM for testing - 0x00 = max, 0x1E = default)
    ret |= es8388_write_reg(ES8388_DACCONTROL24, 0x00);     // LOUT1 volume MAX
    ret |= es8388_write_reg(ES8388_DACCONTROL25, 0x00);     // ROUT1 volume MAX
    ret |= es8388_write_reg(ES8388_DACCONTROL26, 0x00);     // LOUT2 volume
    ret |= es8388_write_reg(ES8388_DACCONTROL27, 0x00);     // ROUT2 volume
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8388 register configuration failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "ES8388 codec initialized successfully");
    return ESP_OK;
}

esp_err_t es8388_set_volume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    // Convert 0-100 to ES8388 range (0-33 per channel, 0=loudest)
    uint8_t reg_val = (100 - volume) * 33 / 100;
    
    esp_err_t ret = ESP_OK;
    ret |= es8388_write_reg(ES8388_DACCONTROL24, reg_val);
    ret |= es8388_write_reg(ES8388_DACCONTROL25, reg_val);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Volume set to %d%% (reg=0x%02x)", volume, reg_val);
    }
    
    return ret;
}

esp_err_t es8388_start(void)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Starting DAC output");
    
    // Critical: Reset and restart state machine
    ret |= es8388_write_reg(ES8388_CHIPPOWER, 0xF0);   // Reset state machine
    ret |= es8388_write_reg(ES8388_CHIPPOWER, 0x00);   // Start state machine
    
    // Power up DAC outputs (still muted from init)
    ret |= es8388_write_reg(ES8388_DACPOWER, 0x3C);
    ESP_LOGI(TAG, "DAC powered up (muted), waiting for power-up transient...");
    
    // Wait for DAC power-up transient to settle
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Unmute DAC - audio signal will now appear at DAC output
    ret |= es8388_write_reg(ES8388_DACCONTROL3, 0x00);  // Unmute
    ESP_LOGI(TAG, "DAC unmuted, waiting for output signal to stabilize...");
    
    // Wait for audio signal to stabilize at DAC output (coupling caps charging, etc)
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // NOW enable power amplifier - DAC is outputting stable audio signal
    gpio_set_level(PA_ENABLE_GPIO, 1);
    ESP_LOGI(TAG, "Power amplifier enabled - audio should be clean");
    
    return ret;
}

esp_err_t es8388_stop(void)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Stopping DAC output");
    
    // Disable power amplifier FIRST (prevents pop on shutdown)
    gpio_set_level(PA_ENABLE_GPIO, 0);
    ESP_LOGI(TAG, "Power amplifier disabled");
    
    // Small delay for PA to turn off
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Mute DAC
    ret |= es8388_write_reg(ES8388_DACCONTROL3, 0x04);
    
    // Power down DAC
    ret |= es8388_write_reg(ES8388_DACPOWER, 0xC0);
    
    return ret;
}
