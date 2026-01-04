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

#ifdef __cplusplus
}
#endif
