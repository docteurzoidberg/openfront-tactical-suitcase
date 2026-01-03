#include "audio_mixer.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MIXER";

// WAV file info
typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

// Audio source structure
typedef struct {
    bool active;
    audio_source_state_t state;
    char filepath[128];
    uint8_t volume;              // 0-100
    bool loop;
    
    // Stream buffer for PCM data
    StreamBufferHandle_t buffer;
    
    // Decoder task
    TaskHandle_t decoder_task;
    
    // WAV info
    wav_info_t wav_info;
    
    // Playback state
    uint32_t samples_played;
    bool eof_reached;
} audio_source_t;

// Global mixer state
static struct {
    bool initialized;
    audio_source_t sources[MAX_AUDIO_SOURCES];
    SemaphoreHandle_t mutex;
    TaskHandle_t mixer_task;
    int16_t mix_buffer[SOURCE_BUFFER_SAMPLES * 2];  // Stereo
} g_mixer = {0};

// Forward declarations
static void decoder_task(void *arg);
static void mixer_task(void *arg);
static esp_err_t parse_wav_header(FILE *fp, wav_info_t *info);

/**
 * @brief Initialize audio mixer
 */
esp_err_t audio_mixer_init(void) {
    if (g_mixer.initialized) {
        ESP_LOGW(TAG, "Mixer already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing audio mixer (max %d sources)", MAX_AUDIO_SOURCES);
    
    // Create mutex
    g_mixer.mutex = xSemaphoreCreateMutex();
    if (g_mixer.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    // Initialize all sources as inactive
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        g_mixer.sources[i].active = false;
        g_mixer.sources[i].state = SOURCE_STATE_IDLE;
        g_mixer.sources[i].buffer = NULL;
        g_mixer.sources[i].decoder_task = NULL;
    }
    
    // Create mixer task
    xTaskCreatePinnedToCore(
        mixer_task,
        "mixer",
        4096,
        NULL,
        10,  // High priority
        &g_mixer.mixer_task,
        tskNO_AFFINITY
    );
    
    g_mixer.initialized = true;
    ESP_LOGI(TAG, "Audio mixer initialized");
    
    return ESP_OK;
}

/**
 * @brief Create a new audio source
 */
esp_err_t audio_mixer_create_source(const char *filepath, uint8_t volume,
                                     bool loop, bool interrupt,
                                     audio_source_handle_t *handle) {
    if (!g_mixer.initialized) {
        ESP_LOGE(TAG, "Mixer not initialized");
        return ESP_FAIL;
    }
    
    if (volume > 100) volume = 100;
    
    // Stop all sources if interrupt flag set
    if (interrupt) {
        audio_mixer_stop_all();
    }
    
    // Find free slot
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    int slot = -1;
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (!g_mixer.sources[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(g_mixer.mutex);
        ESP_LOGW(TAG, "No free source slots (max %d)", MAX_AUDIO_SOURCES);
        return ESP_FAIL;
    }
    
    audio_source_t *src = &g_mixer.sources[slot];
    
    // Initialize source
    src->active = true;
    src->state = SOURCE_STATE_PLAYING;
    strncpy(src->filepath, filepath, sizeof(src->filepath) - 1);
    src->volume = volume;
    src->loop = loop;
    src->samples_played = 0;
    src->eof_reached = false;
    
    // Create stream buffer (2x buffer size for safety)
    src->buffer = xStreamBufferCreate(SOURCE_BUFFER_SAMPLES * 4, 1);
    if (src->buffer == NULL) {
        ESP_LOGE(TAG, "Failed to create stream buffer for source %d", slot);
        src->active = false;
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    // Create decoder task
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "dec_%d", slot);
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        decoder_task,
        task_name,
        4096,
        (void *)(intptr_t)slot,
        8,  // Medium priority
        &src->decoder_task,
        tskNO_AFFINITY
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create decoder task for source %d", slot);
        vStreamBufferDelete(src->buffer);
        src->active = false;
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    *handle = slot;
    
    xSemaphoreGive(g_mixer.mutex);
    
    ESP_LOGI(TAG, "Created source %d: %s vol=%d%% loop=%d", 
             slot, filepath, volume, loop);
    
    return ESP_OK;
}

/**
 * @brief Stop an audio source
 */
esp_err_t audio_mixer_stop_source(audio_source_handle_t handle) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    audio_source_t *src = &g_mixer.sources[handle];
    if (src->active && src->state == SOURCE_STATE_PLAYING) {
        src->state = SOURCE_STATE_STOPPING;
        ESP_LOGI(TAG, "Stopping source %d", handle);
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    return ESP_OK;
}

/**
 * @brief Stop all audio sources
 */
esp_err_t audio_mixer_stop_all(void) {
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (g_mixer.sources[i].active && 
            g_mixer.sources[i].state == SOURCE_STATE_PLAYING) {
            g_mixer.sources[i].state = SOURCE_STATE_STOPPING;
        }
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    ESP_LOGI(TAG, "Stopping all sources");
    
    return ESP_OK;
}

/**
 * @brief Set source volume
 */
esp_err_t audio_mixer_set_volume(audio_source_handle_t handle, uint8_t volume) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (volume > 100) volume = 100;
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    if (g_mixer.sources[handle].active) {
        g_mixer.sources[handle].volume = volume;
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    return ESP_OK;
}

/**
 * @brief Get number of active sources
 */
int audio_mixer_get_active_count(void) {
    int count = 0;
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (g_mixer.sources[i].active && 
            g_mixer.sources[i].state == SOURCE_STATE_PLAYING) {
            count++;
        }
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    return count;
}

/**
 * @brief Check if a source is still playing
 */
bool audio_mixer_is_playing(audio_source_handle_t handle) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return false;
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    bool playing = g_mixer.sources[handle].active && 
                   g_mixer.sources[handle].state == SOURCE_STATE_PLAYING;
    
    xSemaphoreGive(g_mixer.mutex);
    
    return playing;
}

/**
 * @brief Parse WAV header
 */
static esp_err_t parse_wav_header(FILE *fp, wav_info_t *info) {
    uint8_t header[44];
    
    if (fread(header, 1, 44, fp) != 44) {
        return ESP_FAIL;
    }
    
    // Check RIFF header
    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        return ESP_FAIL;
    }
    
    info->num_channels = header[22] | (header[23] << 8);
    info->sample_rate = header[24] | (header[25] << 8) | 
                       (header[26] << 16) | (header[27] << 24);
    info->bits_per_sample = header[34] | (header[35] << 8);
    
    // Find data chunk
    info->data_offset = 44;
    info->data_size = header[40] | (header[41] << 8) | 
                     (header[42] << 16) | (header[43] << 8);
    
    return ESP_OK;
}

/**
 * @brief Decoder task - reads WAV file and feeds stream buffer
 */
static void decoder_task(void *arg) {
    int slot = (int)(intptr_t)arg;
    audio_source_t *src = &g_mixer.sources[slot];
    
    ESP_LOGI(TAG, "Decoder task started for source %d", slot);
    
    FILE *fp = NULL;
    int16_t read_buffer[512];
    
    while (true) {
        // Check if we should stop
        if (src->state == SOURCE_STATE_STOPPING) {
            break;
        }
        
        // Open file if not already open
        if (fp == NULL) {
            fp = fopen(src->filepath, "rb");
            if (fp == NULL) {
                ESP_LOGE(TAG, "Failed to open %s", src->filepath);
                break;
            }
            
            // Parse WAV header
            if (parse_wav_header(fp, &src->wav_info) != ESP_OK) {
                ESP_LOGE(TAG, "Invalid WAV file: %s", src->filepath);
                fclose(fp);
                break;
            }
            
            ESP_LOGI(TAG, "Source %d: %luHz %dch %dbit", 
                     slot, (unsigned long)src->wav_info.sample_rate, 
                     src->wav_info.num_channels, src->wav_info.bits_per_sample);
        }
        
        // Read PCM data
        size_t samples_to_read = sizeof(read_buffer) / (src->wav_info.num_channels * 2);
        size_t bytes_read = fread(read_buffer, 1, 
                                  samples_to_read * src->wav_info.num_channels * 2, fp);
        
        if (bytes_read == 0) {
            // EOF reached
            if (src->loop) {
                // Loop back to beginning
                fseek(fp, src->wav_info.data_offset, SEEK_SET);
                continue;
            } else {
                // End of playback
                src->eof_reached = true;
                break;
            }
        }
        
        // Write to stream buffer (blocking if buffer full)
        xStreamBufferSend(src->buffer, read_buffer, bytes_read, portMAX_DELAY);
    }
    
    // Cleanup
    if (fp != NULL) {
        fclose(fp);
    }
    
    src->state = SOURCE_STATE_STOPPED;
    
    ESP_LOGI(TAG, "Decoder task stopped for source %d", slot);
    
    vTaskDelete(NULL);
}

/**
 * @brief Mixer task - combines all sources and outputs to I2S
 */
static void mixer_task(void *arg) {
    int16_t i2s_buffer[512 * 2];  // Stereo
    int16_t source_samples[512 * 2];
    size_t bytes_written;
    
    ESP_LOGI(TAG, "Mixer task started");
    
    while (true) {
        // Clear mix buffer
        memset(i2s_buffer, 0, sizeof(i2s_buffer));
        
        int active_sources = 0;
        
        // Mix all active sources
        xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
        
        for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
            audio_source_t *src = &g_mixer.sources[i];
            
            if (!src->active || src->state != SOURCE_STATE_PLAYING) {
                // Cleanup stopped sources
                if (src->active && src->state == SOURCE_STATE_STOPPED) {
                    if (src->buffer) {
                        vStreamBufferDelete(src->buffer);
                        src->buffer = NULL;
                    }
                    src->active = false;
                }
                continue;
            }
            
            // Read from source buffer
            size_t bytes_available = xStreamBufferReceive(
                src->buffer, 
                source_samples, 
                sizeof(source_samples), 
                0  // Non-blocking
            );
            
            if (bytes_available == 0) {
                // No data available - check if EOF
                if (src->eof_reached) {
                    src->state = SOURCE_STATE_STOPPED;
                }
                continue;
            }
            
            // Mix into output buffer with volume control
            int samples = bytes_available / 2;  // 16-bit samples
            for (int j = 0; j < samples; j++) {
                // Apply volume (0-100 to 0-1.0)
                int32_t sample = (source_samples[j] * src->volume) / 100;
                
                // Mix (saturating add to prevent clipping)
                int32_t mixed = i2s_buffer[j] + sample;
                if (mixed > 32767) mixed = 32767;
                if (mixed < -32768) mixed = -32768;
                
                i2s_buffer[j] = (int16_t)mixed;
            }
            
            active_sources++;
        }
        
        xSemaphoreGive(g_mixer.mutex);
        
        // Write to I2S (always write to prevent underruns, silence if no sources)
        i2s_write(I2S_NUM_0, i2s_buffer, sizeof(i2s_buffer), &bytes_written, portMAX_DELAY);
        
        // Small yield to prevent watchdog
        if (active_sources == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
