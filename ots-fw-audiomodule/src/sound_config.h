/**
 * @file sound_config.h
 * @brief Sound registry configuration for CAN-controlled playback
 */

#ifndef SOUND_CONFIG_H
#define SOUND_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Sound registry entry
 */
typedef struct {
    uint16_t index;              // CAN sound index (0-65535)
    const char *filename;        // WAV filename on SD card
    const char *description;     // Human-readable description
    uint8_t default_volume;      // Default volume (0-100)
    bool loopable;               // Can be looped
} sound_entry_t;

/**
 * @brief Global sound registry
 * 
 * Maps CAN sound indices to WAV files on SD card
 */
extern const sound_entry_t g_sound_registry[];
extern const size_t g_sound_registry_size;

/**
 * @brief Lookup sound entry by CAN index
 * 
 * @param index CAN sound index
 * @return Pointer to sound entry, or NULL if not found
 */
const sound_entry_t* sound_config_lookup(uint16_t index);

#endif // SOUND_CONFIG_H
