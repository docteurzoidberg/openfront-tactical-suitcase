# Audio Module Console Commands

**Last Updated:** January 4, 2026  
**Firmware Version:** ESP32-A1S Audio Module

This document provides comprehensive documentation for all available serial console commands.

## Table of Contents

- [Getting Started](#getting-started)
- [WAV File Playback](#wav-file-playback)
- [Tone Generation](#tone-generation)
- [Volume Control](#volume-control)
- [Mixer Control](#mixer-control)
- [System Information](#system-information)
- [Advanced Usage](#advanced-usage)

---

## Getting Started

### Accessing the Console

1. Connect to the device via USB serial
2. Use any serial terminal (PuTTY, screen, minicom, etc.)
3. Settings: **115200 baud, 8N1**
4. The console starts automatically on boot

### Console Features

- **Tab completion** - Press TAB to autocomplete commands
- **Command history** - Use UP/DOWN arrows to recall previous commands
- **Command help** - Type `help` to see all available commands
- **Line editing** - Use LEFT/RIGHT arrows to edit commands

---

## WAV File Playback

### `play <filename>`

Play a WAV file from the SD card.

**Usage:**
```
play <filename>
```

**Parameters:**
- `filename` - Name of WAV file on SD card (must include .wav extension)

**Examples:**
```
play track1.wav
play /sdcard/audio/music.wav
play alarm.wav
```

**Requirements:**
- SD card must be mounted
- File must exist on SD card
- File must be valid WAV format (16-bit, mono/stereo)

**Notes:**
- Automatically interrupts currently playing sounds
- File path relative to `/sdcard/` mount point

---

### `1` (Quick Play #1)

Play the first quick-access sound file (`track1.wav`).

**Usage:**
```
1
```

**Description:**
Shortcut for `play track1.wav`. Useful for frequently used sounds.

**Example:**
```
> 1
I (1234) CONSOLE: Playing sound 1 (track1.wav)
```

---

### `2` (Quick Play #2)

Play the second quick-access sound file (`track2.wav`).

**Usage:**
```
2
```

**Description:**
Shortcut for `play track2.wav`.

---

### `hello` (Quick Play Hello)

Play the hello sound file (`hello.wav`).

**Usage:**
```
hello
```

**Description:**
Shortcut for `play hello.wav`. Typically used for greeting or confirmation sounds.

---

### `ping` (Quick Play Ping)

Play the ping sound file (`ping.wav`).

**Usage:**
```
ping
```

**Description:**
Shortcut for `play ping.wav`. Typically used for notification or alert sounds.

---

## Tone Generation

All tone commands play embedded test tones (no SD card required).

### `tone1`

Play a 1-second 440Hz tone (A4 note).

**Usage:**
```
tone1
```

**Description:**
- Frequency: 440 Hz (musical note A4)
- Duration: 1 second
- Volume: 100% (default)
- Source: Embedded in firmware

**Example:**
```
> tone1
I (1234) CONSOLE: Playing embedded tone (1s @ 440Hz)
```

---

### `tone2`

Play a 2-second 880Hz tone (A5 note).

**Usage:**
```
tone2
```

**Description:**
- Frequency: 880 Hz (musical note A5, one octave higher)
- Duration: 2 seconds
- Volume: 100% (default)
- Source: Embedded in firmware

---

### `tone3`

Play a 5-second 220Hz tone (A3 note).

**Usage:**
```
tone3
```

**Description:**
- Frequency: 220 Hz (musical note A3, one octave lower)
- Duration: 5 seconds
- Volume: 100% (default)
- Source: Embedded in firmware

---

## Volume Control

### `volume [level]`

Get or set the master volume level.

**Usage:**
```
volume              # Show current volume
volume <level>      # Set volume (0-100)
```

**Parameters:**
- `level` - Volume percentage (0-100)
  - `0` = Muted
  - `100` = Full volume

**Examples:**
```
> volume
Current master volume: 100%

> volume 50
I (1234) AUDIO_MIXER: Master volume set to 50%

> volume 0
I (1235) AUDIO_MIXER: Master volume set to 0%
```

**Notes:**
- Master volume applies to ALL audio sources
- Changes take effect immediately
- Volume persists only for current session (reset on reboot)
- Valid range: 0-100 (values >100 are clamped to 100)

---

## Mixer Control

### `status`

Show current mixer status and active audio sources.

**Usage:**
```
status
```

**Output:**
```
> status
I (1234) CONSOLE: ═══ Audio Mixer Status ═══
I (1234) CONSOLE: Active sources: 2 / 8
I (1234) CONSOLE: Source 0: ACTIVE (volume: 80%)
I (1234) CONSOLE: Source 3: ACTIVE (volume: 100%)
```

**Shows:**
- Number of active sources vs. maximum capacity
- Individual source status (active/inactive)
- Per-source volume levels

---

### `stop`

Stop all currently playing audio.

**Usage:**
```
stop
```

**Description:**
Immediately stops all active audio sources. Useful for emergency silence or resetting the mixer.

**Example:**
```
> stop
I (1234) CONSOLE: Stopping all audio
```

---

### `playing`

Show currently active audio sources.

**Usage:**
```
playing
```

**Description:**
Displays detailed information about all currently active audio sources, including:
- Source slot number
- File path
- Playback state (PLAYING, PAUSED, STOPPING)
- Per-source volume level

**Output:**
```
> playing
I (1234) CONSOLE: ═══ Active Audio Sources (2/8) ═══
  Source 0: track1.wav [PLAYING] vol=80%
  Source 3: track2.wav [PAUSED] vol=60%
```

**States:**
- **PLAYING** - Currently playing audio
- **PAUSED** - Paused (use `resume` to continue)
- **STOPPING** - In the process of stopping

**Example - No Active Sources:**
```
> playing
No audio currently playing
```

**Use Cases:**
- Check what's currently playing
- Debug audio playback issues
- Monitor mixer capacity usage
- Identify paused sources

---

### `pause`

Pause all currently playing audio.

**Usage:**
```
pause
```

**Description:**
Pauses all active audio sources. Audio can be resumed later with the `resume` command. Paused sources maintain their position in the file and their mixer slot.

**Output:**
```
> pause
I (1234) AUDIO_MIXER: Source 0 paused
I (1235) AUDIO_MIXER: Source 2 paused
I (1236) CONSOLE: ✓ Paused 2 source(s)
```

**Example - Nothing Playing:**
```
> pause
No active sources to pause
```

**Notes:**
- Paused sources still occupy mixer slots
- Use `playing` to see paused sources
- Use `stop` to completely remove sources
- Master volume changes apply to paused sources when resumed

---

### `resume`

Resume paused audio playback.

**Usage:**
```
resume
```

**Description:**
Resumes all paused audio sources. Audio continues from where it was paused.

**Output:**
```
> resume
I (1234) AUDIO_MIXER: Source 0 resumed
I (1235) AUDIO_MIXER: Source 2 resumed
I (1236) CONSOLE: ✓ Resumed 2 source(s)
```

**Example - Nothing Paused:**
```
> resume
No paused sources to resume
```

**Workflow Example:**
```
> play track1.wav
> playing
  Source 0: track1.wav [PLAYING] vol=100%
> pause
I (1234) CONSOLE: ✓ Paused 1 source(s)
> playing
  Source 0: track1.wav [PAUSED] vol=100%
> resume
I (1235) CONSOLE: ✓ Resumed 1 source(s)
> playing
  Source 0: track1.wav [PLAYING] vol=100%
```

---

### `mix`

Play multiple sounds simultaneously (mixer test).

**Usage:**
```
mix
```

**Description:**
Demonstrates the multi-source mixing capability by playing 3 sounds at once:
1. `track1.wav` at 80% volume
2. `track2.wav` at 60% volume  
3. 2-second 880Hz tone at 50% volume

**Example:**
```
> mix
I (1234) CONSOLE: Playing simultaneous sounds (mixer test)
```

**Requirements:**
- SD card must be mounted
- `track1.wav` and `track2.wav` must exist

---

### `test`

Run comprehensive audio test sequence.

**Usage:**
```
test
```

**Description:**
Runs a full test of the audio system:
1. System information display
2. Plays each quick-access sound
3. Plays all tone generators
4. Tests mixer with simultaneous playback

**Duration:** ~20-30 seconds

**Example:**
```
> test
I (1234) CONSOLE: Starting audio test sequence...
[... test output ...]
I (1250) CONSOLE: Audio test complete
```

---

## System Information

### `ls`

List all WAV files on the SD card.

**Usage:**
```
ls
```

**Output:**
```
> ls
I (1234) CONSOLE: ═══ WAV Files on SD Card ═══
  track1.wav
  track2.wav
  hello.wav
  ping.wav
  music.wav
I (1240) CONSOLE: Found 5 WAV file(s)
```

**Features:**
- Case-insensitive filtering (finds .wav, .WAV, .Wav, etc.)
- Shows count of files found
- Lists files from `/sdcard/` directory

**Error Handling:**
```
> ls
Error: SD card not mounted
```

---

### `sysinfo`

Display comprehensive system information.

**Usage:**
```
sysinfo
```

**Output:**
```
> sysinfo
I (1234) CONSOLE: ═══ System Information ═══
Memory:
  Heap free: 245632 bytes
  Heap min:  234512 bytes
  PSRAM total: 4194304 bytes
  PSRAM free:  4100234 bytes

SD Card:
  Status: Mounted

Audio Mixer:
  Active sources: 2 / 8
  Master volume:  100%
```

**Information Displayed:**
- **Memory:**
  - Free heap memory (current)
  - Minimum heap memory (lowest recorded)
  - PSRAM total size (if available)
  - PSRAM free memory (if available)
- **SD Card:**
  - Mount status
- **Audio Mixer:**
  - Active sources count
  - Master volume level

**Use Cases:**
- Diagnosing memory issues
- Checking SD card status
- Monitoring system health
- Debugging audio playback problems

---

### `info`

Display audio system configuration and capabilities.

**Usage:**
```
info
```

**Output:**
```
> info
I (1234) CONSOLE: ═══ Audio System Info ═══
I (1234) CONSOLE: Max sources: 8
I (1234) CONSOLE: Sample rate: 44100 Hz
I (1234) CONSOLE: Bits per sample: 16
I (1234) CONSOLE: Buffer size: 2048 samples
```

**Shows:**
- Maximum simultaneous audio sources
- I2S sample rate
- Audio bit depth
- Internal buffer sizes

---

## Advanced Usage

### Command Chaining

You can run multiple commands by entering them one at a time. There is no built-in command chaining syntax.

### Error Handling

Commands return appropriate error codes:
- `0` = Success
- `1` = Error (check console output for details)

**Common Errors:**
```
Error: SD card not mounted
Error: File not found: track.wav
Error: Volume must be 0-100
Error: Failed to open directory
```

### Performance Tips

1. **Memory:** Check `sysinfo` regularly to monitor heap usage
2. **Simultaneous playback:** Limit to 4-5 sources for best performance
3. **File formats:** Use 16-bit mono WAV files for optimal memory usage
4. **Volume:** Adjust master volume instead of re-encoding files

### Troubleshooting

**No sound output:**
1. Check `sysinfo` - Is SD card mounted?
2. Run `ls` - Do WAV files exist?
3. Check `status` - Are sources active?
4. Verify `volume` - Is master volume >0?

**Distorted audio:**
1. Reduce master volume: `volume 70`
2. Check heap memory: `sysinfo`
3. Stop other sources: `stop`
4. Reduce simultaneous sources

**SD card not detected:**
1. Check physical connection
2. Reboot device
3. Format SD card (FAT32)
4. Check console logs during boot

---

## Quick Reference

| Command | Description | Example |
|---------|-------------|---------|
| `play <file>` | Play WAV file | `play music.wav` |
| `1` | Play track1.wav | `1` |
| `2` | Play track2.wav | `2` |
| `hello` | Play hello.wav | `hello` |
| `ping` | Play ping.wav | `ping` |
| `tone1` | 1s @ 440Hz | `tone1` |
| `tone2` | 2s @ 880Hz | `tone2` |
| `tone3` | 5s @ 220Hz | `tone3` |
| `volume` | Show volume | `volume` |
| `volume <n>` | Set volume (0-100) | `volume 50` |
| `status` | Show mixer status | `status` |
| `stop` | Stop all audio | `stop` |
| `playing` | Show active sources | `playing` |
| `pause` | Pause playback | `pause` |
| `resume` | Resume playback | `resume` |
| `mix` | Test simultaneous play | `mix` |
| `test` | Run audio test | `test` |
| `ls` | List WAV files | `ls` |
| `sysinfo` | System information | `sysinfo` |
| `info` | Audio system info | `info` |
| `help` | Show command list | `help` |

---

## Version History

### v1.1.0 (January 4, 2026)
- **NEW:** `playing` command - Show active audio sources with state
- **NEW:** `pause` command - Pause all active playback
- **NEW:** `resume` command - Resume paused playback
- Added SOURCE_STATE_PAUSED to mixer state machine
- Enhanced mixer API with get_source_info, pause_source, resume_source
- Total commands: 19 (was 16)

### v1.0.0 (January 4, 2026)
- Initial documentation
- 16 commands documented
- Added `ls` command (SD card file listing)
- Added `sysinfo` command (system diagnostics)
- Enhanced `volume` command (get/set functionality)
- Refactored duplicate handlers
- Added magic number constants
- Fixed race conditions in mixer

---

## See Also

- `PROJECT_PROMPT.md` - Firmware architecture documentation
- `src/audio_console.c` - Console implementation source code
- `src/audio_mixer.c` - Audio mixer implementation
- Hardware documentation for ESP32-A1S board

---

**For questions or issues, check the serial console output for detailed error messages and debug logs.**
