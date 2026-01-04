/**
 * @file i2s.c
 * @brief I2S Audio Interface Hardware Abstraction Layer Implementation
 */

#include "i2s.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "i2s";
static uint32_t s_current_sample_rate = 44100;
static i2s_chan_handle_t tx_handle = NULL;

esp_err_t i2s_init(uint32_t sample_rate)
{
    ESP_LOGI(TAG, "Initializing I2S @ %lu Hz", (unsigned long)sample_rate);

    // Channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 256;
    chan_cfg.auto_clear = true;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Standard I2S configuration matching ESP-ADF setup
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,  // CRITICAL: ES8388 NEEDS MCLK on GPIO 0!
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    // Set MCLK multiple to 256x (matches ES8388 config)
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    
    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S standard mode init failed: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        return ret;
    }
    
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel enable failed: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        return ret;
    }
    
    s_current_sample_rate = sample_rate;
    ESP_LOGI(TAG, "I2S initialized successfully");
    return ESP_OK;
}

esp_err_t i2s_set_sample_rate(uint32_t rate)
{
    if (!tx_handle) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Reconfiguring I2S to %lu Hz", (unsigned long)rate);
    
    // Disable channel before reconfiguration
    esp_err_t ret = i2s_channel_disable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Reconfigure clock
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    
    ret = i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure I2S clock: %s", esp_err_to_name(ret));
        i2s_channel_enable(tx_handle);  // Try to re-enable
        return ret;
    }
    
    // Re-enable channel
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to re-enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_current_sample_rate = rate;
    return ESP_OK;
}

esp_err_t i2s_write_audio(const void *data, size_t size, size_t *bytes_written)
{
    if (!tx_handle) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_FAIL;
    }
    
    return i2s_channel_write(tx_handle, data, size, bytes_written, portMAX_DELAY);
}
