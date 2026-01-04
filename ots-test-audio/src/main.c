/**
 * @file main.c
 * @brief Simple audio test - play embedded WAV from flash
 * Pure ESP-IDF, no ADF dependencies
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "es8388.h"
#include "i2s_audio.h"
#include "test_tone_data.h"

static const char *TAG = "MAIN";

// Simple WAV header structure
typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

/**
 * @brief Parse WAV header (minimal parser)
 */
static esp_err_t parse_wav_header(const uint8_t *data, size_t size, wav_info_t *info)
{
    if (size < 44) {
        ESP_LOGE(TAG, "File too small for WAV header");
        return ESP_FAIL;
    }
    
    // Check RIFF header
    if (memcmp(data, "RIFF", 4) != 0) {
        ESP_LOGE(TAG, "Not a RIFF file");
        return ESP_FAIL;
    }
    
    // Check WAVE format
    if (memcmp(data + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Not a WAVE file");
        return ESP_FAIL;
    }
    
    // Find fmt chunk
    const uint8_t *p = data + 12;
    const uint8_t *end = data + size;
    bool found_fmt = false;
    bool found_data = false;
    
    while (p < end - 8) {
        uint32_t chunk_size = *(uint32_t *)(p + 4);
        
        if (memcmp(p, "fmt ", 4) == 0) {
            if (p + 8 + chunk_size > end) break;
            
            // uint16_t audio_format = *(uint16_t *)(p + 8);
            info->num_channels = *(uint16_t *)(p + 10);
            info->sample_rate = *(uint32_t *)(p + 12);
            // uint32_t byte_rate = *(uint32_t *)(p + 16);
            // uint16_t block_align = *(uint16_t *)(p + 20);
            info->bits_per_sample = *(uint16_t *)(p + 22);
            
            found_fmt = true;
        }
        else if (memcmp(p, "data", 4) == 0) {
            info->data_offset = (p + 8) - data;
            info->data_size = chunk_size;
            found_data = true;
            break;
        }
        
        p += 8 + chunk_size;
    }
    
    if (!found_fmt || !found_data) {
        ESP_LOGE(TAG, "Invalid WAV format (fmt=%d, data=%d)", found_fmt, found_data);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WAV parsed: %lu Hz, %d-ch, %d-bit, %lu bytes PCM",
             (unsigned long)info->sample_rate,
             info->num_channels,
             info->bits_per_sample,
             (unsigned long)info->data_size);
    
    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  OTS Audio Test - Pure ESP-IDF");
    ESP_LOGI(TAG, "========================================");
    
    const uint8_t *wav_data = data_test_tone_wav;
    size_t wav_size = data_test_tone_wav_len;
    ESP_LOGI(TAG, "Embedded WAV size: %zu bytes", wav_size);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Parse WAV header
    wav_info_t wav_info;
    ret = parse_wav_header(wav_data, wav_size, &wav_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse WAV");
        return;
    }
    
    // Initialize codec (will be muted during init)
    ESP_LOGI(TAG, "[ 1 ] Initialize ES8388 codec (muted)");
    ret = es8388_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Codec init failed");
        return;
    }
    
    // Initialize I2S
    ESP_LOGI(TAG, "[ 2 ] Initialize I2S");
    ret = i2s_audio_init(wav_info.sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed");
        return;
    }
    
    // Set volume to MAXIMUM for testing
    ESP_LOGI(TAG, "[ 3 ] Set volume to 100%% (MAXIMUM)");
    es8388_set_volume(100);
    
    // Option 1: Pre-fill with silence first (prevents clicks by ensuring clean start)
    // Uncomment to test silence pre-fill approach
    /*
    ESP_LOGI(TAG, "[ 4A ] Pre-filling I2S with silence...");
    size_t silence_written = 0;
    ret = i2s_audio_write(silence_buffer, SILENCE_BUFFER_SIZE, &silence_written);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Silence pre-fill failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "    Pre-filled %zu bytes silence (100ms)", silence_written);
    */
    
    // Pre-fill I2S buffers with actual audio data
    ESP_LOGI(TAG, "[ 4 ] Pre-filling I2S buffers with audio...");
    const uint8_t *pcm_data = wav_data + wav_info.data_offset;
    const size_t prefill_size = 8192;  // Pre-fill 8KB
    size_t bytes_written = 0;
    ret = i2s_audio_write(pcm_data, prefill_size, &bytes_written);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S pre-fill failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "   Pre-filled %zu bytes audio data", bytes_written);
    
    // NOW start DAC with buffers ready
    ESP_LOGI(TAG, "[ 5 ] Start DAC output (buffers ready)");
    es8388_start();
    
    // Longer delay for DAC to fully stabilize with data in buffers
    ESP_LOGI(TAG, "   Waiting for DAC stabilization...");
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "   DAC stable, continuing playback");
    
    // Continue playing remaining audio
    ESP_LOGI(TAG, "[ 6 ] Playing remaining audio (%lu bytes PCM)...", 
             (unsigned long)wav_info.data_size);
    
    // Start from where pre-fill left off
    size_t offset = bytes_written;
    size_t remaining = wav_info.data_size - offset;
    const size_t chunk_size = 4096;  // Write in 4KB chunks
    
    while (remaining > 0) {
        size_t to_write = (remaining < chunk_size) ? remaining : chunk_size;
        size_t bytes_written = 0;
        
        ret = i2s_audio_write(pcm_data + offset, to_write, &bytes_written);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
            break;
        }
        
        offset += bytes_written;
        remaining -= bytes_written;
        
        // Progress indicator every 10%
        static int last_pct = -1;
        int pct = (offset * 100) / wav_info.data_size;
        if (pct >= last_pct + 10) {
            ESP_LOGI(TAG, "  Progress: %d%% (%zu / %lu bytes)", 
                     pct, offset, (unsigned long)wav_info.data_size);
            last_pct = pct;
        }
    }
    
    ESP_LOGI(TAG, "[ 7 ] Playback complete");
    ESP_LOGI(TAG, "  Total written: %zu bytes", offset);
    
    // Small delay for I2S DMA to finish
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Stop DAC
    es8388_stop();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Test complete - system idle");
    ESP_LOGI(TAG, "========================================");
    
    // Idle
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
