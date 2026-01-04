/**
 * @file audio_test_tone.c
 * @brief Generate test tone for audio debugging
 */

#include "audio_test_tone.h"
#include <math.h>
#include <string.h>

#define TEST_SAMPLE_RATE 44100
#define TEST_FREQUENCY 440  // 440Hz = A note
#define TEST_DURATION_MS 500  // 0.5 seconds
#define TEST_AMPLITUDE 8000  // ~25% of max 16-bit amplitude

// Dynamically allocated buffer for test tone
static int16_t *test_tone_buffer = NULL;
static size_t test_tone_size = 0;

void audio_test_tone_generate(void)
{
    size_t num_samples = (TEST_SAMPLE_RATE * TEST_DURATION_MS) / 1000;
    size_t buffer_size = num_samples * 2 * sizeof(int16_t);  // Stereo
    
    // Allocate buffer if not already allocated
    if (test_tone_buffer == NULL) {
        test_tone_buffer = (int16_t *)malloc(buffer_size);
        if (test_tone_buffer == NULL) {
            return;  // Allocation failed
        }
    }
    
    for (size_t i = 0; i < num_samples; i++) {
        // Generate sine wave sample
        float t = (float)i / (float)TEST_SAMPLE_RATE;
        float sine_value = sinf(2.0f * M_PI * TEST_FREQUENCY * t);
        int16_t sample = (int16_t)(sine_value * TEST_AMPLITUDE);
        
        // Stereo: same value for both channels
        test_tone_buffer[i * 2] = sample;      // Left channel
        test_tone_buffer[i * 2 + 1] = sample;  // Right channel
    }
    
    test_tone_size = num_samples * 2 * sizeof(int16_t);
}

const uint8_t* audio_test_tone_get_data(void)
{
    return (const uint8_t*)test_tone_buffer;
}

size_t audio_test_tone_get_size(void)
{
    return test_tone_size;
}
