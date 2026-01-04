/**
 * @file silence_padding.h
 * @brief Silence buffer for pre-filling I2S to avoid startup clicks
 */

#ifndef SILENCE_PADDING_H
#define SILENCE_PADDING_H

#include <stdint.h>
#include <stddef.h>

// 100ms of silence at 44.1kHz, 16-bit stereo = 17640 bytes
// Formula: sample_rate * channels * bytes_per_sample * duration_seconds
// 44100 * 2 * 2 * 0.1 = 17640
#define SILENCE_BUFFER_SIZE 17640

extern const uint8_t silence_buffer[SILENCE_BUFFER_SIZE];

#endif // SILENCE_PADDING_H
