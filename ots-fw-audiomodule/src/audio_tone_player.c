/**
 * @file audio_tone_player.c
 * @brief Embedded tone playback management
 */

#include "audio_tone_player.h"
#include "audio_mixer.h"
#include "wav_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "embedded/tone_1s_440hz.h"
#include "embedded/tone_2s_880hz.h"
#include "embedded/tone_5s_220hz.h"

static const char *TAG = "TONE_PLAYER";

// Sizes (filled at init)
static size_t g_tone_sizes[TONE_ID_MAX];

// Init function to populate sizes
static bool g_initialized = false;

static void tone_player_init(void)
{
    if (g_initialized) return;
    
    g_tone_sizes[TONE_ID_1] = tone_1s_440hz_wav_len;
    g_tone_sizes[TONE_ID_2] = tone_2s_880hz_wav_len;
    g_tone_sizes[TONE_ID_3] = tone_5s_220hz_wav_len;
    
    g_initialized = true;
}

// Tone database
typedef struct {
    const uint8_t *data;
    size_t size;
    const char *description;
    uint32_t duration_ms;
    uint32_t frequency_hz;
} tone_info_t;

static const tone_info_t g_tones[TONE_ID_MAX] = {
    [TONE_ID_1] = {
        .data = tone_1s_440hz_wav,
        .size = 0,  // Filled at runtime
        .description = "tone 1 (1s, 440Hz)",
        .duration_ms = 1000,
        .frequency_hz = 440
    },
    [TONE_ID_2] = {
        .data = tone_2s_880hz_wav,
        .size = 0,  // Filled at runtime
        .description = "tone 2 (2s, 880Hz)",
        .duration_ms = 2000,
        .frequency_hz = 880
    },
    [TONE_ID_3] = {
        .data = tone_5s_220hz_wav,
        .size = 0,  // Filled at runtime
        .description = "tone 3 (5s, 220Hz)",
        .duration_ms = 5000,
        .frequency_hz = 220
    }
};

esp_err_t tone_player_play(tone_id_t id, uint8_t volume)
{
    if (id >= TONE_ID_MAX) {
        ESP_LOGE(TAG, "Invalid tone ID: %d", id);
        return ESP_ERR_INVALID_ARG;
    }
    
    const tone_info_t *tone = &g_tones[id];
    
    ESP_LOGI(TAG, "Playing %s...", tone->description);
    
    // Parse WAV header
    wav_info_t wav;
    esp_err_t ret = wav_parse_header_from_memory(tone->data, &wav);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse WAV header for tone %d", id);
        return ret;
    }
    
    // Create mixer source
    audio_source_handle_t handle;
    ret = audio_mixer_create_source_from_memory(
        tone->data + wav.data_offset, 
        wav.data_size, 
        volume, 
        false, // no loop
        false, // no interrupt
        &handle
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create mixer source for tone %d", id);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ %s started", tone->description);
    return ESP_OK;
}

esp_err_t tone_player_mix_all(void)
{
    ESP_LOGI(TAG, "Mixing all tones simultaneously...");
    
    for (tone_id_t id = TONE_ID_1; id < TONE_ID_MAX; id++) {
        ESP_LOGI(TAG, "Playing %s...", g_tones[id].description);
        
        esp_err_t ret = tone_player_play(id, 100);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to play tone %d", id);
            continue;
        }
        
        ESP_LOGI(TAG, "✓ %s started", g_tones[id].description);
        
        // Small delay between launches to avoid timing issues
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "✓ All tones started");
    return ESP_OK;
}

esp_err_t tone_player_get_info(tone_id_t id, size_t *size, const char **description)
{
    if (id >= TONE_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    tone_player_init();  // Ensure sizes are initialized
    
    if (size) {
        *size = g_tone_sizes[id];
    }
    
    if (description) {
        *description = g_tones[id].description;
    }
    
    return ESP_OK;
}

size_t tone_player_get_total_size(void)
{
    tone_player_init();  // Ensure sizes are initialized
    
    size_t total = 0;
    for (int i = 0; i < TONE_ID_MAX; i++) {
        total += g_tone_sizes[i];
    }
    return total;
}
