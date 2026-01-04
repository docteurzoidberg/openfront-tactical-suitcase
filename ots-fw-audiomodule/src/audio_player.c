/**
 * @file audio_player.c
 * @brief WAV file playback implementation
 */

#include "audio_player.h"
#include "wav_utils.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "hardware/i2s.h"
#include "embedded/test_2sec_wav.h"  // 2-second test tone

static const char *TAG = "AUDIO_PLAYER";

esp_err_t audio_player_parse_wav_header(FILE *f, wav_info_t *out)
{
    // Use shared WAV parsing implementation
    return wav_parse_header(f, out);
}

esp_err_t audio_player_play_wav(const char *rel_path)
{
    char path[256];
    snprintf(path, sizeof(path), "/sdcard/%s", rel_path);

    ESP_LOGI(TAG, "Opening WAV file: %s", path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open WAV file: %s (SD card not mounted?)", path);
        return ESP_FAIL;
    }

    wav_info_t wav;
    esp_err_t ret = audio_player_parse_wav_header(f, &wav);
    if (ret != ESP_OK) {
        fclose(f);
        return ret;
    }

    // Reconfigure I2S if sample rate changed
    ret = i2s_set_sample_rate(wav.sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure I2S");
        fclose(f);
        return ret;
    }

    ESP_LOGI(TAG, "Starting playback: %s", rel_path);

    // Add to audio mixer for playback
    audio_source_handle_t handle;
    ret = audio_mixer_create_source(path, 80, false, false, &handle);
    
    fclose(f);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add source to mixer");
        return ret;
    }

    ESP_LOGI(TAG, "Playback started successfully (handle=%d)", handle);
    return ESP_OK;
}
esp_err_t audio_player_play_embedded_wav(void)
{
    size_t size = test_2sec_wav_len;
    const uint8_t *data = test_2sec_wav;
    
    ESP_LOGI(TAG, "Playing embedded WAV (%zu bytes)", size);
    
    // Parse WAV header from memory
    if (size < 44) {
        ESP_LOGE(TAG, "Embedded file too small for WAV header");
        return ESP_FAIL;
    }
    
    wav_info_t wav;
    esp_err_t ret = wav_parse_header_from_memory(data, &wav);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse embedded WAV header");
        return ret;
    }
    
    // Diagnostic: Log WAV details
    ESP_LOGI(TAG, "WAV: offset=%d, data_size=%u, rate=%u, ch=%d, bits=%d",
             (int)wav.data_offset, (unsigned)wav.data_size, 
             (unsigned)wav.sample_rate, wav.num_channels, wav.bits_per_sample);
    
    // Reconfigure I2S if sample rate different
    ret = i2s_set_sample_rate(wav.sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure I2S");
        return ret;
    }
    
    // Create source from embedded data
    audio_source_handle_t handle;
    ret = audio_mixer_create_source_from_memory(
        data + wav.data_offset,
        wav.data_size,
        80,   // 80% volume
        false, // No loop
        false, // Don't interrupt
        &handle
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create mixer source from embedded data");
        return ret;
    }
    
    ESP_LOGI(TAG, "Embedded WAV playback started (handle=%d)", handle);
    return ESP_OK;
}
esp_err_t audio_player_play_sound_by_index(uint16_t sound_index, 
                                            uint8_t volume,
                                            bool loop, 
                                            bool interrupt,
                                            audio_source_handle_t *handle) 
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/sdcard/sounds/%04d.wav", sound_index);
    
    ESP_LOGI(TAG, "Playing sound %d: %s vol=%d%% loop=%d int=%d",
             sound_index, filepath, volume, loop, interrupt);
    
    esp_err_t ret = audio_mixer_create_source(filepath, volume, loop, interrupt, handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create audio source: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}
