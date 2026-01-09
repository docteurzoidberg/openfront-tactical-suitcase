#include "audio_mixer.h"
#include "audio_volume.h"
#include "audio_decoder.h"
#include "wav_utils.h"
#include "hardware/i2s.h"
#include "can_audio_handler.h"
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
    
    // CAN protocol integration
    uint8_t queue_id;            // CAN queue ID (1-255, 0=not set)
    uint16_t sound_index;        // Original sound index from play request
    
    // Stream buffer for PCM data (allocated in PSRAM)
    StreamBufferHandle_t buffer;
    bool buffer_in_psram;        // Track if buffer is in PSRAM
    uint8_t *buffer_storage;     // Storage pointer for cleanup
    bool pending_cleanup;        // Mark for deferred cleanup
    
    // Decoder task and params
    TaskHandle_t decoder_task;
    decoder_params_t decoder_params;
    
    // WAV info
    wav_info_t wav_info;
    
    // Playback state
    uint32_t samples_played;
    bool stopping;
    bool eof_reached;
    TickType_t drain_start_tick; // When draining started (for I2S buffer flush)
} audio_source_t;

// Global mixer state
static struct {
    bool initialized;
    bool hardware_ready;  // Track if I2S/codec hardware is working
    audio_source_t sources[MAX_AUDIO_SOURCES];
    SemaphoreHandle_t mutex;
    TaskHandle_t mixer_task;
    int16_t *mix_buffer;  // Dynamically allocated stereo mix buffer
    uint8_t master_volume;  // Master volume 0-100
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
        g_mixer.sources[i].queue_id = 0;        // 0 = not set
        g_mixer.sources[i].sound_index = 0xFFFF; // Invalid
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
    g_mixer.master_volume = 100;  // Default to full volume
    ESP_LOGI(TAG, "Audio mixer initialized");
    
    return ESP_OK;
}

/**
 * @brief Set hardware ready state
 */
void audio_mixer_set_hardware_ready(bool ready) {
    if (g_mixer.mutex) {
        xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    }
    g_mixer.hardware_ready = ready;
    if (g_mixer.mutex) {
        xSemaphoreGive(g_mixer.mutex);
    }
    
    if (ready) {
        ESP_LOGI(TAG, "Audio hardware ready - I2S output enabled");
    } else {
        ESP_LOGW(TAG, "Audio hardware not ready - I2S output disabled");
    }
}

/**
 * @brief Set master volume
 */
void audio_mixer_set_master_volume(uint8_t volume) {
    if (volume > 100) {
        volume = 100;
    }
    
    if (g_mixer.mutex) {
        xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
        g_mixer.master_volume = volume;
        xSemaphoreGive(g_mixer.mutex);
    } else {
        g_mixer.master_volume = volume;
    }
    
    ESP_LOGI(TAG, "Master volume set to %d%%", volume);
}

/**
 * @brief Get master volume
 */
uint8_t audio_mixer_get_master_volume(void) {
    uint8_t vol = 0;
    
    if (g_mixer.mutex) {
        xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
        vol = g_mixer.master_volume;
        xSemaphoreGive(g_mixer.mutex);
    } else {
        vol = g_mixer.master_volume;
    }
    
    return vol;
}

/**
 * @brief Get information about a specific source
 */
esp_err_t audio_mixer_get_source_info(audio_source_handle_t handle,
                                       char *filepath, size_t filepath_size,
                                       uint8_t *volume,
                                       int *state) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    if (!g_mixer.sources[handle].active) {
        xSemaphoreGive(g_mixer.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Copy filepath if buffer provided
    if (filepath && filepath_size > 0) {
        strncpy(filepath, g_mixer.sources[handle].filepath, filepath_size - 1);
        filepath[filepath_size - 1] = '\0';
    }
    
    // Copy volume if pointer provided
    if (volume) {
        *volume = g_mixer.sources[handle].volume;
    }
    
    // Copy state if pointer provided
    if (state) {
        *state = (int)g_mixer.sources[handle].state;
    }
    
    xSemaphoreGive(g_mixer.mutex);
    return ESP_OK;
}

/**
 * @brief Pause a playing source
 */
esp_err_t audio_mixer_pause_source(audio_source_handle_t handle) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    if (!g_mixer.sources[handle].active) {
        xSemaphoreGive(g_mixer.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (g_mixer.sources[handle].state == SOURCE_STATE_PLAYING) {
        g_mixer.sources[handle].state = SOURCE_STATE_PAUSED;
        ESP_LOGI(TAG, "Source %d paused", handle);
    }
    
    xSemaphoreGive(g_mixer.mutex);
    return ESP_OK;
}

/**
 * @brief Resume a paused source
 */
esp_err_t audio_mixer_resume_source(audio_source_handle_t handle) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    if (!g_mixer.sources[handle].active) {
        xSemaphoreGive(g_mixer.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (g_mixer.sources[handle].state == SOURCE_STATE_PAUSED) {
        g_mixer.sources[handle].state = SOURCE_STATE_PLAYING;
        ESP_LOGI(TAG, "Source %d resumed", handle);
    }
    
    xSemaphoreGive(g_mixer.mutex);
    return ESP_OK;
}

/**
 * @brief Find a free source slot
 * @return Slot index (0-3) or -1 if none available
 */
static int find_free_slot(void) {
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (!g_mixer.sources[i].active) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Cleanup source buffers if slot was previously used
 */
static void cleanup_source_slot(audio_source_t *src, int slot) {
    if (!src->active) {
        // Normal case: source is not active, safe to cleanup
        if (src->buffer) {
            vStreamBufferDelete(src->buffer);
            src->buffer = NULL;
        }
        if (src->buffer_storage) {
            free(src->buffer_storage);
            src->buffer_storage = NULL;
        }
    }
}

/**
 * @brief Create stream buffer for source (tries PSRAM first)
 * @return true on success
 */
static bool create_source_buffer(audio_source_t *src, int slot) {
    size_t buffer_size = SOURCE_BUFFER_SAMPLES * 4;  // 16KB buffer
    
    src->buffer_storage = (uint8_t *)heap_caps_malloc(
        buffer_size + sizeof(StaticStreamBuffer_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (src->buffer_storage != NULL) {
        src->buffer = xStreamBufferCreateStatic(
            buffer_size, 1, src->buffer_storage,
            (StaticStreamBuffer_t *)(src->buffer_storage + buffer_size));
        src->buffer_in_psram = true;
        ESP_LOGI(TAG, "Source %d: Buffer in PSRAM (%zu bytes)", slot, buffer_size);
    } else {
        ESP_LOGW(TAG, "Source %d: Using internal RAM (PSRAM full)", slot);
        src->buffer = xStreamBufferCreate(buffer_size, 1);
        src->buffer_in_psram = false;
        src->buffer_storage = NULL;
    }
    
    return (src->buffer != NULL);
}

/**
 * @brief Initialize source common fields
 */
static void init_source_common(audio_source_t *src, int slot,
                               const char *name, uint8_t volume, bool loop) {
    src->active = true;
    src->state = SOURCE_STATE_PLAYING;
    src->pending_cleanup = false;
    strncpy(src->filepath, name, sizeof(src->filepath) - 1);
    src->volume = volume;
    src->loop = loop;
    src->samples_played = 0;
    src->stopping = false;
    src->eof_reached = false;
    
    // Setup decoder params (common fields)
    src->decoder_params.slot = slot;
    strncpy(src->decoder_params.filepath, name, sizeof(src->decoder_params.filepath) - 1);
    src->decoder_params.loop = loop;
    src->decoder_params.buffer = src->buffer;
    src->decoder_params.stopping = &src->stopping;
    src->decoder_params.eof_reached = &src->eof_reached;
    src->decoder_params.wav_info = &src->wav_info;
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
    
    if (interrupt) {
        audio_mixer_stop_all();
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    int slot = find_free_slot();
    if (slot == -1) {
        xSemaphoreGive(g_mixer.mutex);
        ESP_LOGW(TAG, "No free source slots (max %d)", MAX_AUDIO_SOURCES);
        return ESP_FAIL;
    }
    
    audio_source_t *src = &g_mixer.sources[slot];
    cleanup_source_slot(src, slot);
    
    if (!create_source_buffer(src, slot)) {
        ESP_LOGE(TAG, "Failed to create stream buffer for source %d", slot);
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    init_source_common(src, slot, filepath, volume, loop);
    
    // File source specific: decoder will parse wav_info
    src->decoder_params.memory_data = NULL;
    src->decoder_params.memory_size = 0;
    
    // Create decoder task
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "dec_%d", slot);
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_decoder_task, task_name, 4096,
        &src->decoder_params, 8, &src->decoder_task, tskNO_AFFINITY);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create decoder task for source %d", slot);
        vStreamBufferDelete(src->buffer);
        if (src->buffer_storage) free(src->buffer_storage);
        src->active = false;
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    *handle = slot;
    xSemaphoreGive(g_mixer.mutex);
    
    ESP_LOGI(TAG, "Created source %d: %s vol=%d%% loop=%d", slot, filepath, volume, loop);
    return ESP_OK;
}

/**
 * @brief Create an audio source from memory buffer
 */
esp_err_t audio_mixer_create_source_from_memory(const uint8_t *pcm_data, size_t pcm_size,
                                                 const wav_info_t *wav_info,
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
    
    if (interrupt) {
        audio_mixer_stop_all();
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    int slot = find_free_slot();
    if (slot == -1) {
        xSemaphoreGive(g_mixer.mutex);
        ESP_LOGW(TAG, "No free source slots (max %d)", MAX_AUDIO_SOURCES);
        return ESP_FAIL;
    }
    
    audio_source_t *src = &g_mixer.sources[slot];
    cleanup_source_slot(src, slot);
    
    if (!create_source_buffer(src, slot)) {
        ESP_LOGE(TAG, "Failed to create stream buffer for source %d", slot);
        xSemaphoreGive(g_mixer.mutex);
        return ESP_FAIL;
    }
    
    char name[64];
    snprintf(name, sizeof(name), "[memory:%zu]", pcm_size);
    init_source_common(src, slot, name, volume, loop);
    
    // Memory source specific: copy wav_info
    if (wav_info != NULL) {
        memcpy(&src->wav_info, wav_info, sizeof(wav_info_t));
    } else {
        // Default format
        memset(&src->wav_info, 0, sizeof(wav_info_t));
        src->wav_info.bits_per_sample = 16;
        src->wav_info.num_channels = 2;
        src->wav_info.sample_rate = 44100;
    }
    
    src->decoder_params.memory_data = pcm_data;
    src->decoder_params.memory_size = pcm_size;
    
    // Create decoder task (larger stack for format conversion)
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "memdec_%d", slot);
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_decoder_task, task_name, 8192,
        &src->decoder_params, 8, &src->decoder_task, tskNO_AFFINITY);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create memory decoder task for source %d", slot);
        vStreamBufferDelete(src->buffer);
        if (src->buffer_storage) free(src->buffer_storage);
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
        // Send SOUND_FINISHED notification if it has a queue_id
        if (src->queue_id != 0) {
            ESP_LOGI(TAG, "Source %d stopped by user: queue_id=%d sound_index=%d", 
                     handle, src->queue_id, src->sound_index);
            can_audio_handler_sound_finished(src->queue_id, src->sound_index, 1);  // 1 = stopped by user
        }
        
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
    
    // Wait for hardware to be ready before starting mixer loop
    while (!g_mixer.hardware_ready) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Hardware ready, starting mixer loop");
    
    while (true) {
        // Clear mix buffer
        memset(i2s_buffer, 0, sizeof(i2s_buffer));
        
        int active_sources = 0;
        size_t max_samples = 0;  // Track maximum samples mixed
        
        // Mix all active sources
        xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
        
        for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
            audio_source_t *src = &g_mixer.sources[i];
            
            // Mark stopped sources as inactive (but don't cleanup buffers here!)
            if (src->active && src->state == SOURCE_STATE_STOPPED) {
                // Send SOUND_FINISHED notification for non-looping sounds with queue_id
                if (!src->loop && src->queue_id != 0) {
                    ESP_LOGI(TAG, "Source %d finished: queue_id=%d sound_index=%d", 
                             i, src->queue_id, src->sound_index);
                    can_audio_handler_sound_finished(src->queue_id, src->sound_index, 0);  // 0 = completed normally
                }
                
                src->active = false;
                // Buffers will be cleaned up when this slot is reused
                continue;
            }
            
            // Skip inactive or non-playing sources
            if (!src->active || src->state != SOURCE_STATE_PLAYING) {
                // Check if draining state - wait for I2S DMA buffer to flush
                // I2S DMA: 8 descriptors x 256 frames @ 44100Hz = ~23ms
                // Use 30ms to be safe
                #define I2S_DRAIN_TIME_MS 30
                if (src->state == SOURCE_STATE_DRAINING) {
                    TickType_t elapsed = xTaskGetTickCount() - src->drain_start_tick;
                    if (elapsed >= pdMS_TO_TICKS(I2S_DRAIN_TIME_MS)) {
                        src->state = SOURCE_STATE_STOPPED;
                    }
                }
                continue;
            }
            
            // Read from source buffer
            // Limit read size to what we can process in one iteration:
            // - Mono source: 512 samples (1024 bytes) -> 512 stereo frames output
            // - Stereo source: 512 stereo pairs (2048 bytes) -> 512 stereo frames output
            size_t max_bytes = (src->wav_info.num_channels == 1) ? 1024 : 2048;
            size_t bytes_available = xStreamBufferReceive(
                src->buffer, 
                source_samples, 
                max_bytes, 
                0  // Non-blocking
            );
            
            if (bytes_available == 0) {
                // No data available - check if EOF
                if (src->eof_reached) {
                    // Start draining - wait for I2S DMA buffer to flush (~23ms)
                    src->state = SOURCE_STATE_DRAINING;
                    src->drain_start_tick = xTaskGetTickCount();
                }
                continue;
            }
            
            // Mix into output buffer with volume control
            // MONO OUTPUT MODE: Always output mono duplicated on both I2S channels
            // This ensures single-speaker setup (LOUT or ROUT) gets full audio
            int samples = bytes_available / 2;  // 16-bit samples
            
            if (src->wav_info.num_channels == 1) {
                // MONO SOURCE: Duplicate each sample to both L and R channels
                for (int j = 0; j < samples && (j * 2 + 1) < (512 * 2); j++) {
                    // Apply source volume
                    int32_t sample = source_samples[j];
                    if (src->volume < 100) {
                        sample = (sample * src->volume) / 100;
                    }
                    
                    // Mix to both LEFT and RIGHT channels (saturating add)
                    // Left channel
                    int32_t mixed_l = i2s_buffer[j * 2] + sample;
                    if (mixed_l > 32767) mixed_l = 32767;
                    if (mixed_l < -32768) mixed_l = -32768;
                    i2s_buffer[j * 2] = (int16_t)mixed_l;
                    
                    // Right channel (same sample)
                    int32_t mixed_r = i2s_buffer[j * 2 + 1] + sample;
                    if (mixed_r > 32767) mixed_r = 32767;
                    if (mixed_r < -32768) mixed_r = -32768;
                    i2s_buffer[j * 2 + 1] = (int16_t)mixed_r;
                }
                
                // Track stereo samples mixed (mono samples * 2)
                int stereo_samples = samples * 2;
                if (stereo_samples > max_samples) {
                    max_samples = stereo_samples;
                }
            } else {
                // STEREO SOURCE: Downmix to mono, then duplicate to both channels
                // This ensures full audio on single speaker (LOUT or ROUT)
                int stereo_samples = samples / 2;  // Stereo pairs (L+R)
                
                for (int j = 0; j < stereo_samples && (j * 2 + 1) < (512 * 2); j++) {
                    // Read stereo pair from source
                    int16_t left_in = source_samples[j * 2];
                    int16_t right_in = source_samples[j * 2 + 1];
                    
                    // Downmix to mono: average L and R channels
                    int32_t mono_sample = ((int32_t)left_in + (int32_t)right_in) / 2;
                    
                    // Apply source volume
                    if (src->volume < 100) {
                        mono_sample = (mono_sample * src->volume) / 100;
                    }
                    
                    // Mix to both LEFT and RIGHT output channels (saturating add)
                    // Left channel
                    int32_t mixed_l = i2s_buffer[j * 2] + mono_sample;
                    if (mixed_l > 32767) mixed_l = 32767;
                    if (mixed_l < -32768) mixed_l = -32768;
                    i2s_buffer[j * 2] = (int16_t)mixed_l;
                    
                    // Right channel (same mono sample)
                    int32_t mixed_r = i2s_buffer[j * 2 + 1] + mono_sample;
                    if (mixed_r > 32767) mixed_r = 32767;
                    if (mixed_r < -32768) mixed_r = -32768;
                    i2s_buffer[j * 2 + 1] = (int16_t)mixed_r;
                }
                
                // Track output samples (stereo pairs * 2 for L+R)
                int output_samples = stereo_samples * 2;
                if (output_samples > max_samples) {
                    max_samples = output_samples;
                }
            }
            
            active_sources++;
        }
        
        xSemaphoreGive(g_mixer.mutex);
        
        // Apply master volume scaling using audio_volume module
        uint8_t master_vol = audio_mixer_get_master_volume();
        if (master_vol < 100 && max_samples > 0) {
            // Only scale the samples we actually mixed (not beyond buffer!)
            audio_volume_apply_fast(i2s_buffer, 
                                   (max_samples < (512 * 2)) ? max_samples : (512 * 2),
                                   master_vol);
        }
        
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

/**
 * @brief Set queue ID for a source (CAN protocol integration)
 */
void audio_mixer_set_queue_id(audio_source_handle_t handle, uint8_t queue_id, uint16_t sound_index) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return;
    }
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    if (g_mixer.sources[handle].active) {
        g_mixer.sources[handle].queue_id = queue_id;
        g_mixer.sources[handle].sound_index = sound_index;
        ESP_LOGI(TAG, "Source %d: queue_id=%d, sound_index=%d", handle, queue_id, sound_index);
    }
    
    xSemaphoreGive(g_mixer.mutex);
}

/**
 * @brief Get queue ID for a source
 */
uint8_t audio_mixer_get_queue_id(audio_source_handle_t handle) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return 0;
    }
    
    uint8_t queue_id = 0;
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    if (g_mixer.sources[handle].active) {
        queue_id = g_mixer.sources[handle].queue_id;
    }
    xSemaphoreGive(g_mixer.mutex);
    
    return queue_id;
}

/**
 * @brief Get sound index for a source
 */
uint16_t audio_mixer_get_sound_index(audio_source_handle_t handle) {
    if (handle < 0 || handle >= MAX_AUDIO_SOURCES) {
        return 0xFFFF;
    }
    
    uint16_t sound_index = 0xFFFF;
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    if (g_mixer.sources[handle].active) {
        sound_index = g_mixer.sources[handle].sound_index;
    }
    xSemaphoreGive(g_mixer.mutex);
    
    return sound_index;
}

/**
 * @brief Stop source by queue ID
 */
esp_err_t audio_mixer_stop_by_queue_id(uint8_t queue_id) {
    if (queue_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t result = ESP_ERR_NOT_FOUND;
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (g_mixer.sources[i].active && g_mixer.sources[i].queue_id == queue_id) {
            ESP_LOGI(TAG, "Stopping source %d by queue_id %d", i, queue_id);
            g_mixer.sources[i].stopping = true;
            g_mixer.sources[i].state = SOURCE_STATE_STOPPING;
            result = ESP_OK;
            break;
        }
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "No active source found with queue_id %d", queue_id);
    }
    
    return result;
}

/**
 * @brief Get source handle by queue ID
 */
audio_source_handle_t audio_mixer_get_handle_by_queue_id(uint8_t queue_id) {
    if (queue_id == 0) {
        return INVALID_SOURCE_HANDLE;
    }
    
    audio_source_handle_t handle = INVALID_SOURCE_HANDLE;
    
    xSemaphoreTake(g_mixer.mutex, portMAX_DELAY);
    
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (g_mixer.sources[i].active && g_mixer.sources[i].queue_id == queue_id) {
            handle = i;
            break;
        }
    }
    
    xSemaphoreGive(g_mixer.mutex);
    
    return handle;
}
