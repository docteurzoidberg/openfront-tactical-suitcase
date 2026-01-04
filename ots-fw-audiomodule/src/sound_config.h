/**
 * @file sound_config.h
 * @brief Sound Configuration - Command table and sound mappings
 */

#pragma once

#include "serial_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serial command to WAV file mapping table
 */
static const serial_command_entry_t sound_command_table[] = {
    { "1",      "track1.wav" },
    { "2",      "track2.wav" },
    { "HELLO",  "hello.wav"  },
    { "PING",   "ping.wav"   },
};

/**
 * @brief Number of entries in command table
 */
#define SOUND_COMMAND_TABLE_LEN (sizeof(sound_command_table) / sizeof(sound_command_table[0]))

#ifdef __cplusplus
}
#endif
