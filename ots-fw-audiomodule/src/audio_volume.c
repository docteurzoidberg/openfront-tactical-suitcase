/**
 * @file audio_volume.c
 * @brief Audio volume processing utilities
 */

#include "audio_volume.h"

void audio_volume_apply(int16_t *samples, size_t count, uint8_t volume)
{
    if (volume >= 100) {
        return; // No scaling needed
    }
    
    if (volume == 0) {
        // Mute - fast path
        for (size_t i = 0; i < count; i++) {
            samples[i] = 0;
        }
        return;
    }
    
    // Normal volume scaling
    for (size_t i = 0; i < count; i++) {
        samples[i] = (samples[i] * volume) / 100;
    }
}

void audio_volume_apply_fast(int16_t *samples, size_t count, uint8_t volume)
{
    // Same implementation as audio_volume_apply
    audio_volume_apply(samples, count, volume);
}
