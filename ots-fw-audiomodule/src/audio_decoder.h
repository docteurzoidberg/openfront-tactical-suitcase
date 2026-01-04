/**
 * @file audio_decoder.h
 * @brief WAV file decoder task
 * 
 * Handles decoding WAV files and feeding PCM data to stream buffers.
 * Separated from audio_mixer for better code organization.
 */

#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include "wav_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decoder task parameters
 */
typedef struct {
    int slot;                          ///< Source slot number
    char filepath[128];                ///< Path to WAV file
    bool loop;                         ///< Loop playback
    StreamBufferHandle_t buffer;       ///< Output stream buffer
    volatile bool *stopping;           ///< Stopping flag (set by mixer)
    volatile bool *eof_reached;        ///< EOF flag (set by decoder)
    wav_info_t *wav_info;              ///< WAV file info (output)
    
    // Memory source fields
    const uint8_t *memory_data;        ///< Pointer to memory buffer (NULL for file source)
    size_t memory_size;                ///< Size of memory buffer
} decoder_params_t;

/**
 * @brief Decoder task entry point
 * 
 * @param arg Pointer to decoder_params_t structure
 */
void audio_decoder_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_DECODER_H
