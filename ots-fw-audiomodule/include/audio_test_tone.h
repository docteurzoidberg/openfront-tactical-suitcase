/**
 * @file audio_test_tone.h
 * @brief Generate test tone for audio debugging
 */

#ifndef AUDIO_TEST_TONE_H
#define AUDIO_TEST_TONE_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Generate a 440Hz sine wave test tone
 */
void audio_test_tone_generate(void);

/**
 * @brief Get pointer to test tone PCM data
 * @return Pointer to 16-bit stereo PCM data
 */
const uint8_t* audio_test_tone_get_data(void);

/**
 * @brief Get size of test tone data in bytes
 * @return Size in bytes
 */
size_t audio_test_tone_get_size(void);

#endif // AUDIO_TEST_TONE_H
