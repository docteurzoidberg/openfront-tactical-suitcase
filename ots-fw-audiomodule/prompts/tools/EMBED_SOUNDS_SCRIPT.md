# Embed Sounds Script Documentation

**Script**: `tools/embed_sounds.sh`  
**Purpose**: Automate conversion of WAV files to embedded C arrays for flash storage  
**Version**: 1.0  
**Date**: January 2026

---

## Overview

The `embed_sounds.sh` script automates the entire process of embedding audio files into the firmware. It converts WAV files to an optimized 8-bit 22kHz mono format and generates C header/source files ready for compilation.

### What It Does

1. **Scans** the `sounds/` folder for WAV files matching pattern `XXXX.wav` (0000-9999)
2. **Converts** each file to 8-bit 22.05kHz mono format using ffmpeg
3. **Generates** C header (.h) and implementation (.c) files using xxd
4. **Places** output in `include/embedded/` and `src/embedded/` directories
5. **Reports** processing results and provides next-step instructions

### Why Use This Script

**Manual Process Problems**:
- 5+ steps per sound file (convert, xxd, edit header, edit source, rebuild)
- Error-prone text editing (array names, sizes, includes)
- Time-consuming with multiple files
- Inconsistent naming and formatting

**Automated Benefits**:
- ✅ One command to process all sounds
- ✅ Consistent naming and format
- ✅ No manual editing required
- ✅ Fast iteration (add/update sounds easily)
- ✅ Proper C syntax generation
- ✅ Automatic file size calculation

---

## Usage

### Basic Usage (Process All Sounds)

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-audiomodule
./tools/embed_sounds.sh
```

This scans `sounds/` directory and processes all WAV files matching `XXXX.wav` pattern.

### Process Specific Sounds

```bash
# Process only sounds 0, 1, and 2
./tools/embed_sounds.sh 0000 0001 0002

# Or use decimal numbers (auto-converts to 4-digit format)
./tools/embed_sounds.sh 0 1 2
```

---

## Input Requirements

### File Location

Place WAV files in: `sounds/XXXX.wav` where XXXX is 0000-9999

Examples:
- `sounds/0000.wav` → Game start sound
- `sounds/0001.wav` → Game victory sound
- `sounds/0007.wav` → Nuke launch sound
- `sounds/0042.wav` → Custom sound 42

### File Format

**Any standard WAV format is accepted**:
- Bit depth: 8-bit, 16-bit, 24-bit, 32-bit
- Sample rate: 8kHz, 22kHz, 44.1kHz, 48kHz, etc.
- Channels: Mono or stereo

The script automatically converts to optimized embedded format:
- **Output format**: 8-bit unsigned PCM, 22050 Hz, mono
- **Rationale**: ~4× smaller than 16-bit 44.1kHz stereo

---

## Output Files

### Directory Structure

```
include/embedded/
  game_sound_0000_22050_8bit.h
  game_sound_0001_22050_8bit.h
  ...

src/embedded/
  game_sound_0000_22050_8bit.c
  game_sound_0001_22050_8bit.c
  ...
```

### Generated Header File Example

```c
/**
 * @file game_sound_0000_22050_8bit.h
 * @brief Embedded sound: game_start (ID 0000)
 * 
 * Auto-generated from sounds/0000.wav
 * Format: 8-bit PCM, 22050 Hz, Mono
 * Size: 44144 bytes
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t game_sound_0000_22050_8bit_wav[];
extern const size_t game_sound_0000_22050_8bit_wav_len;

#ifdef __cplusplus
}
#endif
```

### Generated Source File Example

```c
/**
 * @file game_sound_0000_22050_8bit.c
 * @brief Embedded sound data: game_start (ID 0000)
 * 
 * Auto-generated from sounds/0000.wav
 * Format: 8-bit PCM, 22050 Hz, Mono
 * Size: 44144 bytes
 */

#include "embedded/game_sound_0000_22050_8bit.h"

const uint8_t game_sound_0000_22050_8bit_wav[] = {
  0x52, 0x49, 0x46, 0x46, 0x90, 0xac, 0x00, 0x00,
  0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,
  // ... array data ...
};

const size_t game_sound_0000_22050_8bit_wav_len = sizeof(game_sound_0000_22050_8bit_wav);
```

---

## Integration with Firmware

After running the script, you must integrate the generated files into the firmware:

### Step 1: Add Includes to `src/audio_player.c`

Find the include section (around line 12) and uncomment/add:

```c
// Embedded game sounds (8-bit 22kHz for space efficiency)
#include "embedded/game_sound_0000_22050_8bit.h"  // Game start
#include "embedded/game_sound_0001_22050_8bit.h"  // Victory
#include "embedded/game_sound_0002_22050_8bit.h"  // Defeat
#include "embedded/game_sound_0003_22050_8bit.h"  // Player death
#include "embedded/game_sound_0004_22050_8bit.h"  // Nuke alert
#include "embedded/game_sound_0005_22050_8bit.h"  // Land invasion
#include "embedded/game_sound_0006_22050_8bit.h"  // Naval invasion
#include "embedded/game_sound_0007_22050_8bit.h"  // Nuke launch
```

### Step 2: Populate `embedded_game_sounds[]` Array

Find the array definition (around line 40) and uncomment/add:

```c
static const embedded_game_sound_t embedded_game_sounds[] = {
    { 0, game_sound_0000_22050_8bit_wav, game_sound_0000_22050_8bit_wav_len, "game_start" },
    { 1, game_sound_0001_22050_8bit_wav, game_sound_0001_22050_8bit_wav_len, "game_victory" },
    { 2, game_sound_0002_22050_8bit_wav, game_sound_0002_22050_8bit_wav_len, "game_defeat" },
    { 3, game_sound_0003_22050_8bit_wav, game_sound_0003_22050_8bit_wav_len, "game_death" },
    { 4, game_sound_0004_22050_8bit_wav, game_sound_0004_22050_8bit_wav_len, "alert_nuke" },
    { 5, game_sound_0005_22050_8bit_wav, game_sound_0005_22050_8bit_wav_len, "alert_land" },
    { 6, game_sound_0006_22050_8bit_wav, game_sound_0006_22050_8bit_wav_len, "alert_naval" },
    { 7, game_sound_0007_22050_8bit_wav, game_sound_0007_22050_8bit_wav_len, "nuke_launch" },
};
```

### Step 3: Update `NUM_EMBEDDED_GAME_SOUNDS` Macro

```c
#define NUM_EMBEDDED_GAME_SOUNDS (sizeof(embedded_game_sounds) / sizeof(embedded_game_sound_t))
```

This automatically calculates the array size.

### Step 4: Add to CMake Build

If using CMake (not PlatformIO), add to `src/CMakeLists.txt`:

```cmake
set(SRCS
    # ... existing files ...
    "embedded/game_sound_0000_22050_8bit.c"
    "embedded/game_sound_0001_22050_8bit.c"
    "embedded/game_sound_0002_22050_8bit.c"
    # ... etc ...
)
```

**PlatformIO**: No changes needed, auto-detects `.c` files in `src/` subdirectories.

### Step 5: Rebuild Firmware

```bash
pio run -e esp32-a1s-espidf
```

---

## Example Workflow

### Scenario: Add 8 Game Sounds

```bash
# Step 1: Prepare source WAV files (any format)
mkdir -p sounds
cp ~/audio/game_start.wav sounds/0000.wav
cp ~/audio/game_victory.wav sounds/0001.wav
cp ~/audio/game_defeat.wav sounds/0002.wav
cp ~/audio/game_death.wav sounds/0003.wav
cp ~/audio/alert_nuke.wav sounds/0004.wav
cp ~/audio/alert_land.wav sounds/0005.wav
cp ~/audio/alert_naval.wav sounds/0006.wav
cp ~/audio/nuke_launch.wav sounds/0007.wav

# Step 2: Run conversion script
./tools/embed_sounds.sh

# Output:
# ================================================
#    OTS Audio Module - Embed Sounds Script
# ================================================
# 
# Scanning for WAV files in: /path/to/sounds
# 
# Processing sound 0000 (game_start)...
#   Input:  sounds/0000.wav
#   Converting to 8-bit 22kHz mono... ✓
#   Generating header... ✓
#   Generating source... ✓
#   Output: include/embedded/game_sound_0000_22050_8bit.h
#   Output: src/embedded/game_sound_0000_22050_8bit.c
#   Size:   44144 bytes
# 
# ... (repeat for each sound) ...
# 
# ================================================
# ✓ Processed: 8 sounds
# ================================================

# Step 3: Edit src/audio_player.c (add includes and array entries)
nano src/audio_player.c

# Step 4: Rebuild and flash
pio run -e esp32-a1s-espidf -t upload

# Step 5: Test with CAN commands
cd ../ots-fw-cantest
pio device monitor
# In CLI: p 0    # Test game_start sound
```

---

## Sound Index Mapping

The script automatically assigns friendly names to game sound IDs:

| Sound ID | Filename | Friendly Name | Game Event |
|----------|----------|---------------|------------|
| 0000 | `sounds/0000.wav` | `game_start` | Game starts |
| 0001 | `sounds/0001.wav` | `game_victory` | Player wins |
| 0002 | `sounds/0002.wav` | `game_defeat` | Player loses |
| 0003 | `sounds/0003.wav` | `game_death` | Player dies |
| 0004 | `sounds/0004.wav` | `game_alert_nuke` | Incoming nuke alert |
| 0005 | `sounds/0005.wav` | `game_alert_land` | Land invasion alert |
| 0006 | `sounds/0006.wav` | `game_alert_naval` | Naval invasion alert |
| 0007 | `sounds/0007.wav` | `game_nuke_launch` | Nuke launched |
| 0008-9999 | `sounds/XXXX.wav` | `game_sound_XXXX` | Custom sounds |

---

## File Size Estimation

### Per-Sound Size (8-bit 22kHz mono)

- **1 second**: ~22KB
- **2 seconds**: ~44KB
- **3 seconds**: ~66KB
- **5 seconds**: ~110KB
- **10 seconds**: ~220KB

### Total Flash Budget

With optimal partition (3.94MB app space):
- Current firmware: ~1.25MB
- Available for audio: ~2.69MB
- **8 sounds @ 2s each**: ~352KB (13% of available space)
- **8 sounds @ 5s each**: ~880KB (33% of available space)

### Comparison: 8-bit vs 16-bit

| Duration | 8-bit 22kHz | 16-bit 44kHz | Space Savings |
|----------|-------------|--------------|---------------|
| 2 seconds | 44KB | 176KB | **4.0×** |
| 5 seconds | 110KB | 440KB | **4.0×** |
| 10 seconds | 220KB | 880KB | **4.0×** |

---

## Troubleshooting

### Error: "ffmpeg not found"

```bash
sudo apt install ffmpeg
```

### Error: "xxd not found"

```bash
sudo apt install vim-common  # Contains xxd
```

### Error: "sounds/ directory not found"

```bash
mkdir -p sounds
# Add WAV files to sounds/ directory
```

### No WAV Files Found

Check file naming:
- ✅ Correct: `0000.wav`, `0001.wav`, `0042.wav`
- ❌ Incorrect: `sound0.wav`, `game_start.wav`, `42.wav`

Files must match pattern: `XXXX.wav` where X is 0-9.

### Compilation Error: "file not found"

Verify include paths in `audio_player.c`:
```c
#include "embedded/game_sound_0000_22050_8bit.h"  // ✓ Correct
#include "game_sound_0000_22050_8bit.h"           // ✗ Wrong
```

### Linker Error: "undefined reference"

Make sure `.c` files are included in build:
- **PlatformIO**: Auto-detects `src/embedded/*.c`
- **CMake**: Must manually add to `CMakeLists.txt`

### Firmware Too Large

Options:
1. Reduce sound duration (trim silence)
2. Use lower sample rate (11kHz instead of 22kHz)
3. Remove non-essential sounds
4. Optimize other firmware components

Current capacity: ~2.7MB for embedded audio (enough for ~120 seconds @ 8-bit 22kHz).

---

## Advanced Usage

### Custom Sample Rate

Edit script (line 15):
```bash
EMBED_SAMPLE_RATE=11025  # Half the quality, half the size
```

### Custom Output Directory

Edit script (lines 17-18):
```bash
EMBEDDED_INC_DIR="$PROJECT_ROOT/custom/headers"
EMBEDDED_SRC_DIR="$PROJECT_ROOT/custom/sources"
```

### Batch Processing with Parallel Jobs

```bash
# Process sounds in parallel (4 jobs)
ls sounds/[0-9][0-9][0-9][0-9].wav | \
  xargs -n 1 -P 4 basename -s .wav | \
  xargs ./tools/embed_sounds.sh
```

---

## SD Card Priority System

**Important**: Embedded sounds are **fallback only**.

### Priority Flow

1. **Try SD Card**: Check `/sdcard/sounds/XXXX.wav` first
2. **Try Embedded**: If SD card missing/invalid, use embedded version
3. **Error**: If both missing, return error

### Mixed Configuration

You can mix SD and embedded sounds:
- SD card has: 0, 1, 2 (high-quality 16-bit 44kHz)
- Embedded has: 0, 1, 2, 3, 4, 5, 6, 7 (low-quality 8-bit 22kHz)
- Result:
  - Sounds 0-2 play from SD card (better quality)
  - Sounds 3-7 play from embedded (fallback)
  - User can upgrade sound 3 later by adding to SD card

This allows:
- ✅ Device works without SD card
- ✅ Users can upgrade sound quality selectively
- ✅ Custom sound packs on SD card
- ✅ Graceful degradation if SD card fails

---

## Script Output Example

```
================================================
   OTS Audio Module - Embed Sounds Script
================================================

Scanning for WAV files in: /path/to/sounds

Processing sound 0000 (game_start)...
  Input:  sounds/0000.wav
  Converting to 8-bit 22kHz mono... ✓
  Generating header... ✓
  Generating source... ✓
  Output: include/embedded/game_sound_0000_22050_8bit.h
  Output: src/embedded/game_sound_0000_22050_8bit.c
  Size:   44144 bytes

Processing sound 0001 (game_victory)...
  Input:  sounds/0001.wav
  Converting to 8-bit 22kHz mono... ✓
  Generating header... ✓
  Generating source... ✓
  Output: include/embedded/game_sound_0001_22050_8bit.h
  Output: src/embedded/game_sound_0001_22050_8bit.c
  Size:   44144 bytes

... (6 more sounds) ...

================================================
✓ Processed: 8 sounds

Next steps:
1. Add includes to src/audio_player.c:
   #include "embedded/game_sound_XXXX_22050_8bit.h"

2. Add entries to embedded_game_sounds[] array:
   { XXXX, game_sound_XXXX_22050_8bit_wav, game_sound_XXXX_22050_8bit_wav_len, "name" }

3. Rebuild firmware:
   pio run -e esp32-a1s-espidf
================================================
```

---

## Related Documentation

- **Multi-Format Audio**: `docs/MULTI_FORMAT_AUDIO.md`
- **Embedded Game Sounds Guide**: `docs/EMBEDDED_GAME_SOUNDS.md`
- **CAN Protocol Spec**: `/prompts/CANBUS_MESSAGE_SPEC.md`
- **Audio Player Implementation**: `src/audio_player.c`

---

## Script Maintenance

### Version History

- **v1.0** (January 2026): Initial release
  - Auto-detection of WAV files
  - 8-bit 22kHz conversion
  - C header/source generation
  - Friendly name mapping

### Future Enhancements

- [ ] Auto-update `audio_player.c` includes and array
- [ ] Support for other audio formats (MP3, OGG)
- [ ] Quality presets (low/medium/high)
- [ ] Verify embedded data integrity (checksum)
- [ ] Generate documentation markdown table

---

**Questions or Issues?**

- Check troubleshooting section above
- Review generated files for syntax errors
- Test with `pio run -e esp32-a1s-espidf` before uploading
- Verify sound playback via CAN test commands

