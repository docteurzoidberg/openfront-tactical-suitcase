#include "ac101_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AC101";

// AC101 I2C address
#define AC101_ADDR 0x1A

// AC101 Register addresses
#define CHIP_AUDIO_RS      0x00
#define PLL_CTRL1          0x01
#define PLL_CTRL2          0x02
#define SYSCLK_CTRL        0x03
#define MOD_CLK_ENA        0x04
#define MOD_RST_CTRL       0x05
#define I2S_SR_CTRL        0x06
#define I2S1LCK_CTRL       0x10
#define I2S1_SDOUT_CTRL    0x11
#define I2S1_SDIN_CTRL     0x12
#define I2S1_MXR_SRC       0x13
#define ADC_SRCBST_CTRL    0x52
#define ADC_SRC            0x51
#define ADC_DIG_CTRL       0x40
#define ADC_APC_CTRL       0x50
#define DAC_MXR_SRC        0x4C
#define DAC_DIG_CTRL       0x48
#define OMIXER_SR          0x54
#define OMIXER_DACA_CTRL   0x53
#define HPOUT_CTRL         0x56
#define SPKOUT_CTRL        0x58

// Sample rate values
#define SAMPLE_RATE_8000   0x0000
#define SAMPLE_RATE_11025  0x1000
#define SAMPLE_RATE_12000  0x2000
#define SAMPLE_RATE_16000  0x3000
#define SAMPLE_RATE_22050  0x4000
#define SAMPLE_RATE_24000  0x5000
#define SAMPLE_RATE_32000  0x6000
#define SAMPLE_RATE_44100  0x7000
#define SAMPLE_RATE_48000  0x8000
#define SAMPLE_RATE_96000  0x9000

/**
 * @brief Write to AC101 register via I2C
 */
static esp_err_t ac101_write_reg(uint8_t reg_addr, uint16_t val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AC101_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, (val >> 8) & 0xFF, true);
    i2c_master_write_byte(cmd, val & 0xFF, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write reg 0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Read from AC101 register via I2C
 */
static uint16_t ac101_read_reg(uint8_t reg_addr) {
    uint8_t data[2] = {0};
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AC101_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AC101_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read reg 0x%02X", reg_addr);
        return 0;
    }
    
    return (data[0] << 8) | data[1];
}

/**
 * @brief Map sample rate to AC101 register value
 */
static uint16_t ac101_get_sample_rate_value(uint32_t sample_rate) {
    switch (sample_rate) {
        case 8000:   return SAMPLE_RATE_8000;
        case 11025:  return SAMPLE_RATE_11025;
        case 12000:  return SAMPLE_RATE_12000;
        case 16000:  return SAMPLE_RATE_16000;
        case 22050:  return SAMPLE_RATE_22050;
        case 24000:  return SAMPLE_RATE_24000;
        case 32000:  return SAMPLE_RATE_32000;
        case 44100:  return SAMPLE_RATE_44100;
        case 48000:  return SAMPLE_RATE_48000;
        case 96000:  return SAMPLE_RATE_96000;
        default:     return SAMPLE_RATE_44100; // Default to 44.1kHz
    }
}

/**
 * @brief Initialize AC101 codec
 */
esp_err_t ac101_init(uint32_t sample_rate) {
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing AC101 codec @ %lu Hz", (unsigned long)sample_rate);
    
    // Soft reset
    ret = ac101_write_reg(CHIP_AUDIO_RS, 0x0123);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soft reset failed!");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for reset
    
    ESP_LOGI(TAG, "AC101 reset complete");
    
    // Speaker output control
    ret |= ac101_write_reg(SPKOUT_CTRL, 0xE880);
    
    // PLL configuration (from 256*44.1KHz MCLK)
    ret |= ac101_write_reg(PLL_CTRL1, 0x014F);
    ret |= ac101_write_reg(PLL_CTRL2, 0x8600);
    
    // Clock system
    ret |= ac101_write_reg(SYSCLK_CTRL, 0x8B08);
    ret |= ac101_write_reg(MOD_CLK_ENA, 0x800C);
    ret |= ac101_write_reg(MOD_RST_CTRL, 0x800C);
    
    // Sample rate
    uint16_t sr_val = ac101_get_sample_rate_value(sample_rate);
    ret |= ac101_write_reg(I2S_SR_CTRL, sr_val);
    ESP_LOGI(TAG, "Sample rate set to %lu Hz (reg=0x%04X)", (unsigned long)sample_rate, sr_val);
    
    // I2S interface configuration
    ret |= ac101_write_reg(I2S1LCK_CTRL, 0x8850);     // BCLK/LRCK
    ret |= ac101_write_reg(I2S1_SDOUT_CTRL, 0xC000);
    ret |= ac101_write_reg(I2S1_SDIN_CTRL, 0xC000);
    ret |= ac101_write_reg(I2S1_MXR_SRC, 0x2200);
    
    // ADC configuration
    ret |= ac101_write_reg(ADC_SRCBST_CTRL, 0xCCC4);
    ret |= ac101_write_reg(ADC_SRC, 0x2020);
    ret |= ac101_write_reg(ADC_DIG_CTRL, 0x8000);
    ret |= ac101_write_reg(ADC_APC_CTRL, 0xBBC3);
    
    // DAC path configuration
    ret |= ac101_write_reg(DAC_MXR_SRC, 0xCC00);
    ret |= ac101_write_reg(DAC_DIG_CTRL, 0x8000);
    ret |= ac101_write_reg(OMIXER_SR, 0x0081);
    ret |= ac101_write_reg(OMIXER_DACA_CTRL, 0xF080);
    
    // Enable speaker output
    ret |= ac101_write_reg(SPKOUT_CTRL, 0xEABD);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AC101 initialization complete");
    } else {
        ESP_LOGE(TAG, "AC101 initialization failed");
    }
    
    return ret;
}

/**
 * @brief Set speaker volume
 */
esp_err_t ac101_set_speaker_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    
    // Map 0-100 to AC101 range (0-31, stored in bits 0-4)
    uint8_t ac101_vol = (volume * 31) / 100;
    
    uint16_t reg_val = ac101_read_reg(SPKOUT_CTRL);
    reg_val &= ~0x1F; // Clear volume bits
    reg_val |= (ac101_vol & 0x1F);
    
    ESP_LOGI(TAG, "Setting speaker volume: %d%% (reg=0x%04X)", volume, reg_val);
    return ac101_write_reg(SPKOUT_CTRL, reg_val);
}

/**
 * @brief Set headphone volume
 */
esp_err_t ac101_set_headphone_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    
    // Map 0-100 to AC101 range (0-63, stored in bits 4-9)
    uint8_t ac101_vol = (volume * 63) / 100;
    
    uint16_t reg_val = ac101_read_reg(HPOUT_CTRL);
    reg_val &= ~(0x3F << 4); // Clear volume bits
    reg_val |= ((ac101_vol & 0x3F) << 4);
    
    ESP_LOGI(TAG, "Setting headphone volume: %d%% (reg=0x%04X)", volume, reg_val);
    return ac101_write_reg(HPOUT_CTRL, reg_val);
}

/**
 * @brief Enable/disable speaker PA
 */
esp_err_t ac101_set_speaker_enable(bool enable) {
    ESP_LOGI(TAG, "Speaker PA %s", enable ? "enabled" : "disabled");
    
    if (enable) {
        return ac101_write_reg(SPKOUT_CTRL, 0xEABD);
    } else {
        return ac101_write_reg(SPKOUT_CTRL, 0xE880);
    }
}
