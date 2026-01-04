/**
 * @file main.c
 * @brief ESP32-A1S Audio Module - Main Application
 * 
 * Multi-source WAV playback system with CAN bus control and serial commands.
 * Uses modular architecture with hardware abstraction layer.
 */

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "can_driver.h"
#include "can_handler.h"
#include "audio_mixer.h"
#include "audio_player.h"
#include "audio_console.h"

// Hardware abstraction layer
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/i2s.h"
#include "hardware/sdcard.h"
#include "hardware/es8388.h"

// Configuration
#include "board_config.h"

static const char *TAG = "MAIN";

// Global state
static bool g_sd_mounted = false;


/*------------------------------------------------------------------------
 *  Main Application
 *-----------------------------------------------------------------------*/

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-A1S Audio Module Starting ===");

    // Mount SD card
    esp_err_t ret = sdcard_init();
    if (ret == ESP_OK) {
        g_sd_mounted = true;
        ESP_LOGI(TAG, "SD card mounted successfully");
    } else {
        ESP_LOGW(TAG, "SD card mount failed, continuing without SD");
        g_sd_mounted = false;
    }
    
    // Initialize hardware layer
    ESP_LOGI(TAG, "Initializing hardware...");
    ESP_ERROR_CHECK(gpio_init());
    ESP_ERROR_CHECK(i2c_init());
    ESP_ERROR_CHECK(i2s_init(DEFAULT_SAMPLE_RATE));
    
    // Initialize ES8388 codec
    bool codec_ok = false;
    ESP_LOGI(TAG, "Initializing ES8388 codec @ %d Hz", DEFAULT_SAMPLE_RATE);
    ret = es8388_init(DEFAULT_SAMPLE_RATE);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ES8388 codec initialized successfully");
        
        // Start the codec (unmute and power up)
        ret = es8388_start();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "ES8388 codec started and unmuted");
            
            // Enable BOTH outputs at 80% volume (speaker AND line-out/headphone)
            es8388_set_speaker_volume(80);    // LOUT2/ROUT2 (speaker outputs)
            es8388_set_headphone_volume(80);  // LOUT1/ROUT1 (line-out/headphone jack)
            es8388_set_speaker_enable(true);
            ESP_LOGI(TAG, "ES8388: Enabled both speaker and line-out outputs at 80%");
            codec_ok = true;
        } else {
            ESP_LOGW(TAG, "ES8388 codec start failed");
        }
    } else {
        ESP_LOGW(TAG, "ES8388 codec init failed");
        ESP_LOGW(TAG, "Audio output will not work, but system will continue");
        codec_ok = false;
    }
    
    ESP_LOGI(TAG, "Hardware initialized @ %d Hz", DEFAULT_SAMPLE_RATE);
    
    // Initialize audio mixer (may fail if I2S/codec not working)
    ESP_LOGI(TAG, "Initializing audio mixer...");
    ret = audio_mixer_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio mixer init failed (audio won't work without codec)");
    } else {
        ESP_LOGI(TAG, "Audio mixer initialized successfully");
        
        // Only enable audio hardware if codec initialized successfully
        if (codec_ok) {
            audio_mixer_set_hardware_ready(true);
        } else {
            ESP_LOGW(TAG, "Codec not available - audio playback disabled");
            audio_mixer_set_hardware_ready(false);
        }
    }
    
    // Initialize CAN driver
    ESP_LOGI(TAG, "Initializing CAN driver...");
    can_config_t can_config = {
        .tx_gpio = CAN_TX_GPIO,
        .rx_gpio = CAN_RX_GPIO,
        .bitrate = CAN_BITRATE,
        .loopback = false,
        .mock_mode = false
    };
    ret = can_driver_init(&can_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "CAN driver init failed, running in mock mode");
    } else {
        ESP_LOGI(TAG, "CAN driver initialized @ %d bps", CAN_BITRATE);
    }

    // Initialize and start CAN handler
    ESP_ERROR_CHECK(can_handler_init(&g_sd_mounted));
    ESP_ERROR_CHECK(can_handler_start_task());
    ESP_LOGI(TAG, "CAN handler started");
    
    // Initialize interactive console (unified audio control)
    ESP_LOGI(TAG, "=== Audio Console Starting ===");
    ESP_ERROR_CHECK(audio_console_init());
    ESP_ERROR_CHECK(audio_console_start());
    ESP_LOGI(TAG, "Audio console ready - type 'help' for commands");
    
    if (!codec_ok) {
        ESP_LOGW(TAG, "*** CODEC FAILED - AUDIO WILL NOT WORK ***");
    }
}
