# Embedded Game Sounds Guide

**Version**: 1.0  
**Date**: January 8, 2026  
**Status**: Implementation Guide

## Overview

This guide explains how to create and embed low-quality 8-bit 22kHz game sound samples in the firmware. These embedded sounds serve as fallback when SD card files are not present, allowing the system to work out-of-the-box without requiring an SD card.

## Sound Priority System

The audio player uses a **two-tier priority system**:

1. **PRIORITY 1: SD Card** - Check `/sdcard/sounds/XXXX.wav` first
   - If file exists and is valid WAV → Play from SD card
   - Allows users to customize sounds with higher quality files

2. **PRIORITY 2: Embedded Fallback** - If SD card file missing/invalid
   - Play embedded 8-bit 22kHz version from firmware flash
   - Guarantees system works without SD card
   - Saves flash space (~4× smaller than 16-bit 44.1kHz)

## Required Game Sounds

### Sound Index Mapping

| ID | Filename | Description | Duration | Use Case |
|----|----------|-------------|----------|----------|
| 0 | game_start.wav | Game start sound | ~2-3s | Player joins game |
| 1 | game_victory.wav | Victory fanfare | ~3-5s | Player/team wins |
| 2 | game_defeat.wav | Defeat sound | ~2-3s | Player/team loses |
| 3 | game_death.wav | Player death | ~1-2s | Player eliminated |
| 4 | game_alert_nuke.wav | Nuclear alert | ~2-3s | Incoming nuke detected |
| 5 | game_alert_land.wav | Land invasion alert | ~2-3s | Land attack incoming |
| 6 | game_alert_naval.wav | Naval invasion alert | ~2-3s | Naval attack incoming |
| 7 | game_nuke_launch.wav | Nuke launch sound | ~2-3s | Player launches nuke |

### Format Requirements

**Embedded Versions** (saved in firmware):
- **Bit Depth**: 8-bit PCM (unsigned, 0-255)
- **Sample Rate**: 22.05 kHz
- **Channels**: Mono (1 channel)
- **Format**: WAV (RIFF WAVE)

**Storage Efficiency**:
- 8-bit 22kHz mono: 22,050 bytes/second
- 2-second sound: ~44 KB
- 3-second sound: ~66 KB
- Total for 8 sounds (~20s total): ~440 KB

**SD Card Versions** (user-replaceable):
- Any bit depth: 8-bit or 16-bit
- Any sample rate: 8kHz to 48kHz
- Any channels: Mono or stereo
- Firmware will auto-convert to 16-bit 44.1kHz

## Creating 8-bit 22kHz Sounds

### Using FFmpeg

Convert existing high-quality sounds to embedded format:

```bash
# Basic conversion (mono, 8-bit, 22kHz)
ffmpeg -i input.wav -ar 22050 -ac 1 -sample_fmt u8 output_22050_8bit.wav

# With volume normalization
ffmpeg -i input.wav -ar 22050 -ac 1 -sample_fmt u8 -filter:a "volume=1.5" output_22050_8bit.wav

# Batch convert all sounds
for file in sounds/*.wav; do
    basename=$(basename "$file" .wav)
    ffmpeg -i "$file" -ar 22050 -ac 1 -sample_fmt u8 "embedded/${basename}_22050_8bit.wav"
done
```

### Using Audacity

1. **Open** high-quality WAV file
2. **Convert to Mono**: Tracks → Mix → Mix Stereo Down to Mono
3. **Resample**: Tracks → Resample → 22050 Hz
4. **Export**: File → Export → Export Audio
   - Format: WAV (Microsoft)
   - Encoding: Unsigned 8-bit PCM
   - Filename: `game_start_22050_8bit.wav`

### Quality Tips

- **Normalize audio** before converting to 8-bit (maximize dynamic range)
- **Use compression** to reduce peaks (prevents clipping in 8-bit)
- **Test playback** - 8-bit has audible noise, ensure it's acceptable
- **Keep duration short** - 2-3 seconds is ideal for space efficiency

## Embedding Sounds in Firmware

### Step 1: Convert WAV to C Header

Use the provided Python script:

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-audiomodule
python tools/wav_to_header.py sounds/game_start_22050_8bit.wav > include/embedded/game_start_22050_8bit_wav.h
```

**Expected output** (`game_start_22050_8bit_wav.h`):

```c
// Auto-generated from game_start_22050_8bit.wav
#pragma once
#include <stdint.h>
#include <stddef.h>

extern const uint8_t game_start_22050_8bit_wav[];
extern const size_t game_start_22050_8bit_wav_len;
```

And corresponding `.c` file:

```c
#include "embedded/game_start_22050_8bit_wav.h"

const uint8_t game_start_22050_8bit_wav[] = {
    0x52, 0x49, 0x46, 0x46, 0x24, 0xAC, 0x00, 0x00,  // RIFF header
    0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,  // WAVE fmt
    // ... rest of WAV data ...
};

const size_t game_start_22050_8bit_wav_len = sizeof(game_start_22050_8bit_wav);
```

### Step 2: Add Headers to Project

Create header files for all 8 sounds:

```bash
python tools/wav_to_header.py sounds/game_start_22050_8bit.wav > include/embedded/game_start_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_victory_22050_8bit.wav > include/embedded/game_victory_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_defeat_22050_8bit.wav > include/embedded/game_defeat_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_death_22050_8bit.wav > include/embedded/game_death_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_alert_nuke_22050_8bit.wav > include/embedded/game_alert_nuke_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_alert_land_22050_8bit.wav > include/embedded/game_alert_land_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_alert_naval_22050_8bit.wav > include/embedded/game_alert_naval_22050_8bit_wav.h
python tools/wav_to_header.py sounds/game_nuke_launch_22050_8bit.wav > include/embedded/game_nuke_launch_22050_8bit_wav.h
```

### Step 3: Update audio_player.c

Uncomment and update the includes section:

```c
// Embedded game sound headers
#include "embedded/game_start_22050_8bit_wav.h"
#include "embedded/game_victory_22050_8bit_wav.h"
#include "embedded/game_defeat_22050_8bit_wav.h"
#include "embedded/game_death_22050_8bit_wav.h"
#include "embedded/game_alert_nuke_22050_8bit_wav.h"
#include "embedded/game_alert_land_22050_8bit_wav.h"
#include "embedded/game_alert_naval_22050_8bit_wav.h"
#include "embedded/game_nuke_launch_22050_8bit_wav.h"
```

Populate the `embedded_game_sounds` array:

```c
static const embedded_game_sound_t embedded_game_sounds[] = {
    { 0, game_start_22050_8bit_wav, game_start_22050_8bit_wav_len, "game_start" },
    { 1, game_victory_22050_8bit_wav, game_victory_22050_8bit_wav_len, "game_victory" },
    { 2, game_defeat_22050_8bit_wav, game_defeat_22050_8bit_wav_len, "game_defeat" },
    { 3, game_death_22050_8bit_wav, game_death_22050_8bit_wav_len, "game_death" },
    { 4, game_alert_nuke_22050_8bit_wav, game_alert_nuke_22050_8bit_wav_len, "game_alert_nuke" },
    { 5, game_alert_land_22050_8bit_wav, game_alert_land_22050_8bit_wav_len, "game_alert_land" },
    { 6, game_alert_naval_22050_8bit_wav, game_alert_naval_22050_8bit_wav_len, "game_alert_naval" },
    { 7, game_nuke_launch_22050_8bit_wav, game_nuke_launch_22050_8bit_wav_len, "game_nuke_launch" },
};
```

### Step 4: Build and Flash

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-audiomodule
pio run -e esp32-a1s-espidf -t upload
```

## Testing

### Test Without SD Card

1. **Remove SD card** (or keep it unmounted)
2. **Send CAN command** to play sound 0-7
3. **Expected logs**:
```
I (12345) AUDIO_PLAYER: Playing sound 0: /sdcard/sounds/0000.wav vol=80% loop=0 int=0
W (12346) AUDIO_PLAYER: SD card file not found
I (12347) AUDIO_PLAYER: SD card file not found, using embedded 'game_start' (8-bit 22kHz)
I (12348) AUDIO_PLAYER: Embedded WAV: 22050Hz 1ch 8bit (will be converted to 16-bit 44.1kHz)
I (12349) DECODER: Source 0: 22050Hz 1ch 8bit
I (12350) DECODER: Source 0: 8-bit to 16-bit conversion enabled
I (12351) DECODER: Source 0: Resampling from 22050Hz to 44100Hz
I (12400) AUDIO_MIXER: ✓ Source 0 started
```

4. **Expected behavior**: Sound plays via embedded 8-bit 22kHz version

### Test With SD Card

1. **Insert SD card** with `/sdcard/sounds/0000.wav` (any format)
2. **Send CAN command** to play sound 0
3. **Expected logs**:
```
I (12345) AUDIO_PLAYER: Playing sound 0: /sdcard/sounds/0000.wav vol=80% loop=0 int=0
I (12346) AUDIO_PLAYER: Using SD card file: /sdcard/sounds/0000.wav
I (12350) DECODER: Source 0: 44100Hz 2ch 16bit
I (12351) AUDIO_MIXER: ✓ Source 0 started
```

4. **Expected behavior**: Sound plays from SD card (higher quality)

### Test Mixed Sources

1. **SD card has sound 0 (high quality)**
2. **SD card missing sound 1** (will use embedded)
3. **Play both sounds simultaneously**
4. **Expected**: Sound 0 from SD card, sound 1 from embedded
5. **Verify**: Both sounds play correctly

## Flash Space Considerations

### Current Partition Layout

- **Total Flash**: 4 MB
- **App Partition**: 3.94 MB (without OTA)
- **Current Firmware**: 1.25 MB
- **Available**: 2.69 MB

### Storage Budget

| Item | Size | Notes |
|------|------|-------|
| Existing firmware | 1.25 MB | Base code |
| Embedded tones (10000-10002) | ~600 KB | Test tones + quack |
| **Embedded game sounds (0-7)** | **~440 KB** | 8-bit 22kHz, ~20s total |
| **Total used** | **~2.29 MB** | Still fits comfortably |
| **Remaining** | **~1.65 MB** | For future sounds/features |

### Optimization Options

If space becomes tight:

1. **Reduce sound duration**: 1-2s instead of 2-3s
2. **Lower sample rate**: 11.025 kHz instead of 22.05 kHz
3. **Share sounds**: Use same file for similar alerts
4. **Compress audio**: Use ADPCM (4-bit) encoding

## User Experience

### Without SD Card
- System works out-of-the-box
- All 8 game sounds available
- Lower audio quality (8-bit, some noise)
- No user action required

### With SD Card
- Users can customize sounds
- Higher quality available (16-bit 44.1kHz)
- Easy to replace: just copy WAV files
- Instant upgrade without reflashing firmware

### Mixed Configuration
- Some sounds on SD card (customized)
- Others fall back to embedded (default)
- Seamless switching per-sound
- No configuration needed

## Troubleshooting

### Sound Plays from Embedded Instead of SD Card
- **Check**: Is SD card mounted? (`ls` command should work)
- **Verify**: Does `/sdcard/sounds/XXXX.wav` exist?
- **Test**: Is WAV file valid? (use `ffmpeg -i file.wav`)

### Embedded Sound Not Playing
- **Check logs**: Did firmware compile with embedded sounds?
- **Verify**: Are header files included in `audio_player.c`?
- **Test**: Does `embedded_game_sounds[]` array have entries?

### Poor Audio Quality
- **Expected**: 8-bit has audible noise, especially at low volumes
- **Solution 1**: Replace with 16-bit version on SD card
- **Solution 2**: Use higher sample rate (sacrifice space)
- **Solution 3**: Apply noise shaping/dithering before conversion

### Firmware Size Too Large
- **Reduce sound duration**: Trim to 1-2 seconds
- **Lower sample rate**: Use 11.025 kHz
- **Remove unused sounds**: Comment out in array
- **Enable OTA partitions**: If you don't need them, use no-OTA layout

## Future Enhancements

- [ ] Add web interface to upload custom sounds
- [ ] Support for ADPCM compression (4-bit encoding)
- [ ] Dynamic loading of sounds from SD card on demand
- [ ] Sound preview command in console
- [ ] Automatic quality selection based on available space

## References

- **Audio Player**: `/ots-fw-audiomodule/src/audio_player.c`
- **Multi-Format Support**: `/ots-fw-audiomodule/docs/MULTI_FORMAT_AUDIO.md`
- **CAN Protocol**: `/prompts/CANBUS_MESSAGE_SPEC.md`
- **WAV Format**: RIFF WAVE specification

---

**Status**: ✅ Framework Implemented  
**Next Step**: Create 8-bit 22kHz WAV files and embed them  
**Estimated Time**: 1-2 hours to convert and embed all sounds
