/**
 * @file wav_utils.h
 * @brief WAV File Utilities - Shared parsing and helper functions
 * 
 * Common utilities for parsing WAV/RIFF files used across audio modules.
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV file information structure
 */
typedef struct {
    uint32_t sample_rate;      // Sample rate in Hz
    uint16_t num_channels;     // Number of channels (1=mono, 2=stereo)
    uint16_t bits_per_sample;  // Bits per sample (typically 16)
    uint32_t data_offset;      // Offset to audio data in file
    uint32_t data_size;        // Size of audio data in bytes
} wav_info_t;

/**
 * @brief Read 16-bit little-endian value
 * @param p Pointer to 2-byte data
 * @return 16-bit value
 */
uint16_t wav_read_le16(const uint8_t *p);

/**
 * @brief Read 32-bit little-endian value
 * @param p Pointer to 4-byte data
 * @return 32-bit value
 */
uint32_t wav_read_le32(const uint8_t *p);

/**
 * @brief Parse WAV file header
 * @param f File pointer (must be at start of file)
 * @param info Output structure for WAV information
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wav_parse_header(FILE *f, wav_info_t *info);

/**
 * @brief Parse WAV header from memory buffer
 * @param data Pointer to WAV data in memory
 * @param info Output structure for WAV information
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wav_parse_header_from_memory(const uint8_t *data, wav_info_t *info);

/**
 * @brief Convert 8-bit unsigned PCM to 16-bit signed PCM
 * @param in_8bit Input buffer (8-bit unsigned samples: 0-255)
 * @param out_16bit Output buffer (16-bit signed samples: -32768 to 32767)
 * @param num_samples Number of samples to convert
 * @note Output buffer must have space for num_samples * sizeof(int16_t)
 */
void wav_convert_8bit_to_16bit(const uint8_t *in_8bit, int16_t *out_16bit, size_t num_samples);

/**
 * @brief Resample audio data using linear interpolation
 * @param in_data Input buffer (16-bit signed PCM)
 * @param in_samples Number of input samples (per channel)
 * @param in_rate Input sample rate (Hz)
 * @param out_data Output buffer (16-bit signed PCM)
 * @param out_samples Number of output samples (per channel)
 * @param out_rate Output sample rate (Hz, typically 44100)
 * @param num_channels Number of channels (1=mono, 2=stereo)
 * @return Number of samples written to output buffer
 * @note Output buffer must have space for out_samples * num_channels * sizeof(int16_t)
 */
size_t wav_resample_linear(const int16_t *in_data, size_t in_samples, uint32_t in_rate,
                          int16_t *out_data, size_t out_samples, uint32_t out_rate,
                          uint16_t num_channels);

#ifdef __cplusplus
}
#endif
