/**
 * @file sound_config.c
 * @brief Sound registry configuration for CAN-controlled playback
 */

#include "sound_config.h"
#include <stddef.h>

/**
 * Sound registry mapping CAN indices to WAV files
 * 
 * Add new sounds here to make them available via CAN bus
 */
const sound_entry_t g_sound_registry[] = {
    // Test sounds
    {
        .index = 1,
        .filename = "track1.wav",
        .description = "Test track 1",
        .default_volume = 80,
        .loopable = false
    },
    {
        .index = 2,
        .filename = "track2.wav",
        .description = "Test track 2",
        .default_volume = 80,
        .loopable = false
    },
    
    // System sounds
    {
        .index = 100,
        .filename = "hello.wav",
        .description = "Hello greeting",
        .default_volume = 90,
        .loopable = false
    },
    {
        .index = 101,
        .filename = "ping.wav",
        .description = "Ping notification",
        .default_volume = 70,
        .loopable = false
    },
    
    // Game sounds (examples - add your own)
    {
        .index = 200,
        .filename = "game_start.wav",
        .description = "Game start",
        .default_volume = 100,
        .loopable = false
    },
    {
        .index = 201,
        .filename = "game_player_death.wav",
        .description = "Player death",
        .default_volume = 90,
        .loopable = false
    },
    {
        .index = 202,
        .filename = "game_victory.wav",
        .description = "Victory",
        .default_volume = 100,
        .loopable = false
    },
    {
        .index = 203,
        .filename = "game_defeat.wav",
        .description = "Defeat",
        .default_volume = 90,
        .loopable = false
    },
};

const size_t g_sound_registry_size = sizeof(g_sound_registry) / sizeof(g_sound_registry[0]);

const sound_entry_t* sound_config_lookup(uint16_t index)
{
    for (size_t i = 0; i < g_sound_registry_size; i++) {
        if (g_sound_registry[i].index == index) {
            return &g_sound_registry[i];
        }
    }
    return NULL;
}
