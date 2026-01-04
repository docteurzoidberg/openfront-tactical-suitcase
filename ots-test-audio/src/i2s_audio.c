/**
 * @file i2s_audio.c
 * @brief I2S audio output using ESP-IDF native driver
 */

#include "i2s_audio.h"
#include "board_config.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "I2S";

static i2s_chan_handle_t tx_handle = NULL;

esp_err_t i2s_audio_init(uint32_t sample_rate)
{
    ESP_LOGI(TAG, "Initializing I2S @ %lu Hz", (unsigned long)sample_rate);
    
    // Channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 240;
    chan_cfg.auto_clear = true;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Standard I2S configuration - Philips I2S format
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
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
        ESP_LOGE(TAG, "Failed to init standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        return ret;
    }
    
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable channel: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "I2S initialized successfully");
    ESP_LOGI(TAG, "  MCLK: GPIO %d", I2S_MCLK_IO);
    ESP_LOGI(TAG, "  BCK:  GPIO %d", I2S_BCK_IO);
    ESP_LOGI(TAG, "  WS:   GPIO %d", I2S_WS_IO);
    ESP_LOGI(TAG, "  DO:   GPIO %d", I2S_DO_IO);
    
    return ESP_OK;
}

esp_err_t i2s_audio_write(const void *data, size_t size, size_t *bytes_written)
{
    if (tx_handle == NULL) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_FAIL;
    }
    
    return i2s_channel_write(tx_handle, data, size, bytes_written, portMAX_DELAY);
}

esp_err_t i2s_audio_deinit(void)
{
    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        ESP_LOGI(TAG, "I2S deinitialized");
    }
    return ESP_OK;
}
