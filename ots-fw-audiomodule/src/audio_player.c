/**
 * @file audio_player.c
 * @brief Unified audio playback - single entry point for all sounds
 * 
 * Plays sounds by ID from either SD card or embedded flash.
 * Priority: SD card first, then embedded fallback.
 */

#include "audio_player.h"
#include "wav_utils.h"
#include "audio_mixer.h"
#include "hardware/i2s.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

// Include all embedded sounds
#include "embedded/game_sound_0000_22050_8bit.h"
#include "embedded/game_sound_0001_22050_8bit.h"
#include "embedded/game_sound_0002_22050_8bit.h"
#include "embedded/game_sound_0003_22050_8bit.h"
#include "embedded/game_sound_0004_22050_8bit.h"
#include "embedded/game_sound_0005_22050_8bit.h"
#include "embedded/game_sound_0006_22050_8bit.h"
#include "embedded/game_sound_0007_22050_8bit.h"
#include "embedded/game_sound_10000_22050_8bit.h"
#include "embedded/game_sound_10001_22050_8bit.h"
#include "embedded/game_sound_10002_22050_8bit.h"
#include "embedded/game_sound_10100_22050_8bit.h"

static const char *TAG = "AUDIO_PLAYER";

// ============================================================================
// Embedded Sound Registry
// ============================================================================

typedef struct {
    uint16_t sound_id;
    const uint8_t *data;
    size_t size;
    const char *name;
} embedded_sound_t;

// Unified embedded sounds table - all sounds in one place
static const embedded_sound_t embedded_sounds[] = {
    // Game sounds (0-7)
    {    0, game_sound_0000_22050_8bit_wav, 0, "game_start" },
    {    1, game_sound_0001_22050_8bit_wav, 0, "game_victory" },
    {    2, game_sound_0002_22050_8bit_wav, 0, "game_defeat" },
    {    3, game_sound_0003_22050_8bit_wav, 0, "game_death" },
    {    4, game_sound_0004_22050_8bit_wav, 0, "game_alert_nuke" },
    {    5, game_sound_0005_22050_8bit_wav, 0, "game_alert_land" },
    {    6, game_sound_0006_22050_8bit_wav, 0, "game_alert_naval" },
    {    7, game_sound_0007_22050_8bit_wav, 0, "game_nuke_launch" },
    // Test tones (10000-10002)
    {10000, game_sound_10000_22050_8bit_wav, 0, "test_tone_440hz_1s" },
    {10001, game_sound_10001_22050_8bit_wav, 0, "test_tone_880hz_2s" },
    {10002, game_sound_10002_22050_8bit_wav, 0, "test_tone_220hz_5s" },
    // Special sounds
    {10100, game_sound_10100_22050_8bit_wav, 0, "quack" },
};

#define NUM_EMBEDDED_SOUNDS (sizeof(embedded_sounds) / sizeof(embedded_sound_t))

// Sizes array (filled at runtime from _len symbols)
static size_t embedded_sound_sizes[NUM_EMBEDDED_SOUNDS];
static bool embedded_sounds_initialized = false;

/**
 * @brief Initialize embedded sound sizes from linker symbols
 */
static void init_embedded_sounds(void)
{
    if (embedded_sounds_initialized) return;
    
    // Game sounds 0-7
    embedded_sound_sizes[0]  = game_sound_0000_22050_8bit_wav_len;
    embedded_sound_sizes[1]  = game_sound_0001_22050_8bit_wav_len;
    embedded_sound_sizes[2]  = game_sound_0002_22050_8bit_wav_len;
    embedded_sound_sizes[3]  = game_sound_0003_22050_8bit_wav_len;
    embedded_sound_sizes[4]  = game_sound_0004_22050_8bit_wav_len;
    embedded_sound_sizes[5]  = game_sound_0005_22050_8bit_wav_len;
    embedded_sound_sizes[6]  = game_sound_0006_22050_8bit_wav_len;
    embedded_sound_sizes[7]  = game_sound_0007_22050_8bit_wav_len;
    // Test tones 10000-10002
    embedded_sound_sizes[8]  = game_sound_10000_22050_8bit_wav_len;
    embedded_sound_sizes[9]  = game_sound_10001_22050_8bit_wav_len;
    embedded_sound_sizes[10] = game_sound_10002_22050_8bit_wav_len;
    // Special sounds
    embedded_sound_sizes[11] = game_sound_10100_22050_8bit_wav_len;
    
    embedded_sounds_initialized = true;
    ESP_LOGI(TAG, "Initialized %d embedded sounds", NUM_EMBEDDED_SOUNDS);
}

/**
 * @brief Find embedded sound by ID
 * @return Pointer to embedded_sound_t or NULL if not found
 */
static const embedded_sound_t* find_embedded_sound(uint16_t sound_id, size_t *out_size)
{
    init_embedded_sounds();
    
    for (size_t i = 0; i < NUM_EMBEDDED_SOUNDS; i++) {
        if (embedded_sounds[i].sound_id == sound_id) {
            if (out_size) {
                *out_size = embedded_sound_sizes[i];
            }
            return &embedded_sounds[i];
        }
    }
    return NULL;
}

// ============================================================================
// Playback Functions
// ============================================================================

/**
 * @brief Play embedded sound from flash memory
 */
static esp_err_t play_embedded_sound(const embedded_sound_t *sound, size_t size,
                                      uint8_t volume, bool loop, bool interrupt,
                                      audio_source_handle_t *handle)
{
    ESP_LOGI(TAG, "Playing embedded '%s' (ID %d, %zu bytes)", 
             sound->name, sound->sound_id, size);
    
    // Parse WAV header
    if (size < 44) {
        ESP_LOGE(TAG, "Embedded file too small for WAV header");
        return ESP_FAIL;
    }
    
    wav_info_t wav;
    esp_err_t ret = wav_parse_header_from_memory(sound->data, &wav);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse embedded WAV header");
        return ret;
    }
    
    ESP_LOGI(TAG, "WAV: %uHz %dch %dbit, %u bytes PCM",
             (unsigned)wav.sample_rate, wav.num_channels, 
             wav.bits_per_sample, (unsigned)wav.data_size);
    
    // Create mixer source from embedded data
    ret = audio_mixer_create_source_from_memory(
        sound->data + wav.data_offset,
        wav.data_size,
        &wav,       // Pass wav_info for 8-bit to 16-bit conversion
        volume,
        loop,
        interrupt,
        handle
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create mixer source");
        return ret;
    }
    
    ESP_LOGI(TAG, "Playback started (handle=%d)", *handle);
    return ESP_OK;
}

/**
 * @brief Try to play sound from SD card
 * @return ESP_OK if played, ESP_ERR_NOT_FOUND if file doesn't exist
 */
static esp_err_t try_play_from_sd(uint16_t sound_id, uint8_t volume,
                                   bool loop, bool interrupt,
                                   audio_source_handle_t *handle)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/sdcard/sounds/%04d.wav", sound_id);
    
    // Try to open file
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;  // File doesn't exist
    }
    
    // Validate WAV header
    wav_info_t wav;
    esp_err_t ret = wav_parse_header(f, &wav);
    fclose(f);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD file exists but invalid WAV: %s", filepath);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Valid WAV - create mixer source
    ESP_LOGI(TAG, "Playing from SD: %s", filepath);
    ret = audio_mixer_create_source(filepath, volume, loop, interrupt, handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SD source: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

// ============================================================================
// Public API
// ============================================================================

esp_err_t audio_player_play_sound(uint16_t sound_id, 
                                   uint8_t volume,
                                   bool loop, 
                                   bool interrupt,
                                   audio_source_handle_t *handle)
{
    ESP_LOGI(TAG, "Play sound %d: vol=%d%% loop=%d int=%d",
             sound_id, volume, loop, interrupt);
    
    // PRIORITY 1: Try SD card first
    esp_err_t ret = try_play_from_sd(sound_id, volume, loop, interrupt, handle);
    if (ret == ESP_OK) {
        return ESP_OK;  // Successfully played from SD
    }
    
    // PRIORITY 2: Fall back to embedded sound
    size_t size;
    const embedded_sound_t *embedded = find_embedded_sound(sound_id, &size);
    
    if (embedded) {
        ESP_LOGI(TAG, "SD not found, using embedded '%s'", embedded->name);
        return play_embedded_sound(embedded, size, volume, loop, interrupt, handle);
    }
    
    // No SD card file and no embedded fallback
    ESP_LOGE(TAG, "Sound %d: not found on SD or embedded", sound_id);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t audio_player_parse_wav_header(FILE *f, wav_info_t *out)
{
    return wav_parse_header(f, out);
}

// Legacy function - redirects to unified API
esp_err_t audio_player_play_wav(const char *rel_path)
{
    char path[256];
    snprintf(path, sizeof(path), "/sdcard/%s", rel_path);

    ESP_LOGI(TAG, "Opening WAV file: %s", path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open WAV file: %s", path);
        return ESP_FAIL;
    }

    wav_info_t wav;
    esp_err_t ret = wav_parse_header(f, &wav);
    fclose(f);
    
    if (ret != ESP_OK) {
        return ret;
    }

    // Create mixer source
    audio_source_handle_t handle;
    ret = audio_mixer_create_source(path, 80, false, false, &handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add source to mixer");
        return ret;
    }

    ESP_LOGI(TAG, "Playback started (handle=%d)", handle);
    return ESP_OK;
}

// ============================================================================
// Utility Functions
// ============================================================================

size_t audio_player_get_embedded_count(void)
{
    return NUM_EMBEDDED_SOUNDS;
}

esp_err_t audio_player_get_embedded_info(size_t index, uint16_t *sound_id, 
                                          const char **name, size_t *size)
{
    if (index >= NUM_EMBEDDED_SOUNDS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    init_embedded_sounds();
    
    if (sound_id) *sound_id = embedded_sounds[index].sound_id;
    if (name) *name = embedded_sounds[index].name;
    if (size) *size = embedded_sound_sizes[index];
    
    return ESP_OK;
}

size_t audio_player_get_total_embedded_size(void)
{
    init_embedded_sounds();
    
    size_t total = 0;
    for (size_t i = 0; i < NUM_EMBEDDED_SOUNDS; i++) {
        total += embedded_sound_sizes[i];
    }
    return total;
}
