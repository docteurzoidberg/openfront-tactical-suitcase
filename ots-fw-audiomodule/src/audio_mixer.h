#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include "wav_utils.h"  // For wav_info_t

// Maximum number of simultaneous audio sources
#define MAX_AUDIO_SOURCES 4

// Audio format
#define MIXER_SAMPLE_RATE 44100
#define MIXER_CHANNELS 2
#define MIXER_BITS_PER_SAMPLE 16

// Ring buffer size per source (in samples, stereo = 2 values per sample)
#define SOURCE_BUFFER_SAMPLES 4096  // ~46ms at 44.1kHz stereo

/**
 * @brief Audio source handle
 */
typedef int audio_source_handle_t;
#define INVALID_SOURCE_HANDLE -1

/**
 * @brief Audio source state
 */
typedef enum {
    SOURCE_STATE_IDLE = 0,
    SOURCE_STATE_PLAYING,
    SOURCE_STATE_PAUSED,
    SOURCE_STATE_STOPPING,
    SOURCE_STATE_DRAINING,  // Buffer empty, waiting for I2S to finish
    SOURCE_STATE_STOPPED
} audio_source_state_t;

/**
 * @brief Initialize audio mixer
 * 
 * Creates mixer task that combines all active sources and outputs to I2S.
 * Note: Audio hardware (I2S/codec) must be initialized separately.
 * Call audio_mixer_set_hardware_ready(true) after successful hardware init.
 * 
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_init(void);

/**
 * @brief Set hardware ready state
 * 
 * Should be called after I2S and codec are successfully initialized.
 * When false, mixer task will skip I2S writes to prevent crashes.
 * 
 * @param ready true if hardware is working, false otherwise
 */
void audio_mixer_set_hardware_ready(bool ready);

/**
 * @brief Create a new audio source
 * 
 * @param filepath Path to WAV file on SD card
 * @param volume Volume 0-100 (100 = full volume)
 * @param loop true to loop playback, false for one-shot
 * @param interrupt true to stop all other sources
 * @param handle Output: source handle for control
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_create_source(const char *filepath, uint8_t volume, 
                                     bool loop, bool interrupt,
                                     audio_source_handle_t *handle);

/**
 * @brief Create an audio source from memory buffer
 * 
 * @param pcm_data Pointer to PCM audio data
 * @param pcm_size Size of PCM data in bytes
 * @param wav_info WAV format info (for 8-bit to 16-bit conversion)
 * @param volume Volume 0-100 (100 = full volume)
 * @param loop true to loop playback, false for one-shot
 * @param interrupt true to stop all other sources
 * @param handle Output: source handle for control
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_create_source_from_memory(const uint8_t *pcm_data, size_t pcm_size,
                                                 const wav_info_t *wav_info,
                                                 uint8_t volume, bool loop, bool interrupt,
                                                 audio_source_handle_t *handle);

/**
 * @brief Stop an audio source
 * 
 * @param handle Source handle to stop
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_stop_source(audio_source_handle_t handle);

/**
 * @brief Stop all audio sources
 * 
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_stop_all(void);

/**
 * @brief Set source volume
 * 
 * @param handle Source handle
 * @param volume Volume 0-100
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_set_volume(audio_source_handle_t handle, uint8_t volume);

/**
 * @brief Get number of active sources
 * 
 * @return Number of sources currently playing
 */
int audio_mixer_get_active_count(void);

/**
 * @brief Check if a source is still playing
 * 
 * @param handle Source handle
 * @return true if playing, false otherwise
 */
bool audio_mixer_is_playing(audio_source_handle_t handle);

/**
 * @brief Set master volume for mixer
 * 
 * @param volume Volume 0-100 (applied to all sources)
 */
void audio_mixer_set_master_volume(uint8_t volume);

/**
 * @brief Get current master volume
 * 
 * @return Master volume 0-100
 */
uint8_t audio_mixer_get_master_volume(void);
/**
 * @brief Get information about a specific source
 * 
 * @param handle Source handle
 * @param filepath Output buffer for file path (can be NULL)
 * @param filepath_size Size of filepath buffer
 * @param volume Output pointer for volume (can be NULL)
 * @param state Output pointer for state (can be NULL)
 * @return ESP_OK if source exists, ESP_ERR_NOT_FOUND otherwise
 */
esp_err_t audio_mixer_get_source_info(audio_source_handle_t handle,
                                       char *filepath, size_t filepath_size,
                                       uint8_t *volume,
                                       int *state);

/**
 * @brief Pause a playing source
 * 
 * @param handle Source handle
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_pause_source(audio_source_handle_t handle);

/**
 * @brief Resume a paused source
 * 
 * @param handle Source handle
 * @return ESP_OK on success, error otherwise
 */
esp_err_t audio_mixer_resume_source(audio_source_handle_t handle);

/**
 * @brief Set queue ID for a source (CAN protocol integration)
 * 
 * @param handle Source handle
 * @param queue_id CAN queue ID (1-255, 0=invalid)
 * @param sound_index Original sound index from play request
 */
void audio_mixer_set_queue_id(audio_source_handle_t handle, uint8_t queue_id, uint16_t sound_index);

/**
 * @brief Get queue ID for a source
 * 
 * @param handle Source handle
 * @return Queue ID (0 if not set or invalid handle)
 */
uint8_t audio_mixer_get_queue_id(audio_source_handle_t handle);

/**
 * @brief Get sound index for a source
 * 
 * @param handle Source handle
 * @return Sound index (0xFFFF if not set or invalid handle)
 */
uint16_t audio_mixer_get_sound_index(audio_source_handle_t handle);

/**
 * @brief Stop source by queue ID
 * 
 * @param queue_id CAN queue ID to stop
 * @return ESP_OK if stopped, ESP_ERR_NOT_FOUND if no matching source
 */
esp_err_t audio_mixer_stop_by_queue_id(uint8_t queue_id);

/**
 * @brief Get source handle by queue ID
 * 
 * @param queue_id CAN queue ID
 * @return Source handle or INVALID_SOURCE_HANDLE if not found
 */
audio_source_handle_t audio_mixer_get_handle_by_queue_id(uint8_t queue_id);

#endif // AUDIO_MIXER_H
