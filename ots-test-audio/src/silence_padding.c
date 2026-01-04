/**
 * @file silence_padding.c
 * @brief Silence buffer for pre-filling I2S to avoid startup clicks
 */

#include "silence_padding.h"

// 100ms of silence (all zeros) for 44.1kHz 16-bit stereo
// This prevents DAC startup clicks by ensuring clean initial samples
const uint8_t silence_buffer[SILENCE_BUFFER_SIZE] = {0};
