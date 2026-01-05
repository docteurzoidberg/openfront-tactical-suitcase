/**
 * @file audio_volume.h
 * @brief Audio volume processing utilities
 */

#ifndef AUDIO_VOLUME_H
#define AUDIO_VOLUME_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Apply volume scaling to audio samples
 * 
 * @param samples Audio sample buffer (16-bit signed)
 * @param count Number of samples
 * @param volume Volume level (0-100)
 */
void audio_volume_apply(int16_t *samples, size_t count, uint8_t volume);

/**
 * @brief Apply volume scaling in-place (optimized inline function)
 * 
 * @param samples Audio sample buffer (16-bit signed)
 * @param count Number of samples
 * @param volume Volume level (0-100)
 */
void audio_volume_apply_fast(int16_t *samples, size_t count, uint8_t volume);

#endif // AUDIO_VOLUME_H
