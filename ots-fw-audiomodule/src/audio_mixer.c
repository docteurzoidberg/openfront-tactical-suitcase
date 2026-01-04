#include "audio_mixer.h"
#include "audio_decoder.h"
#include "wav_utils.h"
#include "hardware/i2s.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MIXER";

// Audio source structure
typedef struct {
    bool active;
    audio_source_state_t state;
    char filepath[128];
    uint8_t volume;              // 0-100
    bool loop;
    
    // Stream buffer for PCM data
    StreamBufferHandle_t buffer;
    
    // Decoder task and params
    TaskHandle_t decoder_task;
    decoder_params_t decoder_params;
    
    // WAV info
    wav_info_t wav_info;
    
    // Playback state
    uint32_t samples_played;
    bool stopping;
    bool eof_reached;
} audio_source_t;

// Global mixer state
static struct {
    bool initialized;
    bool hardware_ready;  // Track if I2S/codec hardware is working
    audio_source_t sources[MAX_AUDIO_SOURCES];
    SemaphoreHandle_t mutex;
    TaskHandle_t mixer_task;
    int16_t *mix_buffer;  // Dynamically allocated stereo mix buffer
} g_mixer = {0};

// Forward declarations
static void mixer_task(void *arg);

/**
 * @brief Initialize audio mixer
 */
esp_err_t audio_mixer_init(void) {
    if (g_mixer.initialized) {
        ESP_LOGW(TAG, "Mixer already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing audio mixer (max %d sources)", MAX_AUDIO_SOURCES);
    
    // Assume hardware is NOT ready (main.c will set this after successful I2S/codec init)
    g_mixer.hardware_ready = false;
    
    // Allocate mix buffer from heap (use PSRAM if available, fallback to internal RAM)
    g_mixer.mix_buffer = (int16_t *)heap_caps_malloc(SOURCE_BUFFER_SAMPLES * 2 * sizeof(int16_t), 
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (g_mixer.mix_buffer == NULL) {
        ESP_LOGW(TAG, "PSRAM allocation failed, using internal RAM for mix buffer");
        g_mixer.mix_buffer = (int16_t *)malloc(SOURCE_BUFFER_SAMPLES * 2 * sizeof(int16_t));
        if (g_mixer.mix_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate mix buffer");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGI(TAG, "Mix buffer allocated from PSRAM (%zu bytes)", SOURCE_BUFFER_SAMPLES * 2 * sizeof(int16_t));
    }
    
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
        8192,  // Increased stack size for buffers
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
 * @brief Set hardware ready state
 */
void audio_mixer_set_hardware_ready(bool ready) {
    g_mixer.hardware_ready = ready;
    if (ready) {
        ESP_LOGI(TAG, "Audio hardware ready - I2S output enabled");
    } else {
        ESP_LOGW(TAG, "Audio hardware not ready - I2S output disabled");
    }
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
    src->stopping = false;
    src->eof_reached = false;
    
    // Create stream buffer (2x buffer size for safety)
    src->buffer = xStreamBufferCreate(SOURCE_BUFFER_SAMPLES * 4, 1);
    if (src->buffer == NULL) {
        ESP_LOGE(TAG, "Failed to create stream buffer for source %d", slot);
        src->active = false;
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    // Setup decoder parameters
    src->decoder_params.slot = slot;
    strncpy(src->decoder_params.filepath, filepath, sizeof(src->decoder_params.filepath) - 1);
    src->decoder_params.loop = loop;
    src->decoder_params.buffer = src->buffer;
    src->decoder_params.stopping = &src->stopping;
    src->decoder_params.eof_reached = &src->eof_reached;
    src->decoder_params.wav_info = &src->wav_info;
    src->decoder_params.memory_data = NULL;  // File source, not memory
    src->decoder_params.memory_size = 0;
    
    // Create decoder task
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "dec_%d", slot);
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_decoder_task,
        task_name,
        4096,
        &src->decoder_params,
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
 * @brief Create an audio source from memory buffer
 */
esp_err_t audio_mixer_create_source_from_memory(const uint8_t *pcm_data, size_t pcm_size,
                                                 uint8_t volume, bool loop, bool interrupt,
                                                 audio_source_handle_t *handle) {
    if (!g_mixer.initialized) {
        ESP_LOGE(TAG, "Mixer not initialized");
        return ESP_FAIL;
    }
    
    if (!pcm_data || pcm_size == 0) {
        ESP_LOGE(TAG, "Invalid PCM data");
        return ESP_ERR_INVALID_ARG;
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
    snprintf(src->filepath, sizeof(src->filepath), "[memory:%zu]", pcm_size);
    src->volume = volume;
    src->loop = loop;
    src->samples_played = 0;
    src->stopping = false;
    src->eof_reached = false;
    
    // Create stream buffer - Cap size to prevent DRAM overflow
    // We stream in chunks anyway, so no need to buffer the entire file
    size_t buffer_size = SOURCE_BUFFER_SAMPLES * 4;  // Fixed 16KB buffer
    
    // Create buffer (will use internal RAM, but limited size)
    src->buffer = xStreamBufferCreate(buffer_size, 1);
    if (src->buffer == NULL) {
        ESP_LOGE(TAG, "Failed to create stream buffer for source %d", slot);
        src->active = false;
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Memory source %d: allocated %zu bytes buffer (PCM size: %zu)", slot, buffer_size, pcm_size);
    
    // Setup decoder parameters for memory source
    src->decoder_params.slot = slot;
    snprintf(src->decoder_params.filepath, sizeof(src->decoder_params.filepath), "[memory:%zu]", pcm_size);
    src->decoder_params.loop = loop;
    src->decoder_params.buffer = src->buffer;
    src->decoder_params.stopping = &src->stopping;
    src->decoder_params.eof_reached = &src->eof_reached;
    src->decoder_params.wav_info = &src->wav_info;
    src->decoder_params.memory_data = pcm_data;
    src->decoder_params.memory_size = pcm_size;
    
    // Create decoder task to stream memory data
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "memdec_%d", slot);
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_decoder_task,
        task_name,
        4096,
        &src->decoder_params,
        8,
        &src->decoder_task,
        tskNO_AFFINITY
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create memory decoder task for source %d", slot);
        vStreamBufferDelete(src->buffer);
        src->active = false;
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    if (handle) {
        *handle = slot;
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    ESP_LOGI(TAG, "Created memory source %d: %zu bytes vol=%d%%", slot, pcm_size, volume);
    
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
        src->stopping = true;
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
            g_mixer.sources[i].stopping = true;
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
        size_t max_samples = 0;  // Track maximum samples mixed
        
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
            
            // Track the maximum number of samples we've mixed
            if (samples > max_samples) {
                max_samples = samples;
            }
            
            active_sources++;
        }
        
        xSemaphoreGive(g_mixer.mutex);
        
        // Write to I2S only if hardware is ready
        if (g_mixer.hardware_ready) {
            // Always write full buffer to maintain continuous I2S stream
            i2s_write_audio(i2s_buffer, sizeof(i2s_buffer), &bytes_written);
        }
        
        // Small yield to prevent watchdog
        if (active_sources == 0 || !g_mixer.hardware_ready) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
