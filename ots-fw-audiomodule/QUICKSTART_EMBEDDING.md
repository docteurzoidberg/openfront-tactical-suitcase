# Quick Start: Embedding Game Sounds

This guide shows you how to embed your 8 game sound files into the firmware.

## Step 1: Prepare Sound Files

Place your game sound WAV files in the `sounds/` folder with the correct naming:

```bash
sounds/0000.wav  # Game start sound
sounds/0001.wav  # Victory sound
sounds/0002.wav  # Defeat sound
sounds/0003.wav  # Player death sound
sounds/0004.wav  # Nuke alert sound
sounds/0005.wav  # Land invasion alert
sounds/0006.wav  # Naval invasion alert
sounds/0007.wav  # Nuke launch sound
```

**Note**: Files can be ANY WAV format (16-bit 44kHz, 8-bit, etc.) - the script will convert them automatically.

## Step 2: Run Conversion Script

```bash
cd /path/to/ots-fw-audiomodule
./tools/embed_sounds.sh
```

This will:
- Convert all sounds to 8-bit 22kHz mono
- Generate C header files in `include/embedded/`
- Generate C source files in `src/embedded/`
- Show you the next steps

## Step 3: Update audio_player.c

Edit `src/audio_player.c` and uncomment/update the includes (around line 12):

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

Then update the array (around line 40):

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

## Step 4: Build and Upload

```bash
pio run -e esp32-a1s-espidf -t upload && pio device monitor
```

## Step 5: Test

### Test Without SD Card (Embedded Sounds Only)

```bash
# In cantest device monitor, press 'p' then enter sound ID
p
Sound index: 0
# Should play embedded game_start sound
```

### Test With SD Card Priority

1. Create SD card with high-quality sounds:
   ```bash
   mkdir -p /path/to/sdcard/sounds
   cp my_hq_game_start.wav /path/to/sdcard/sounds/0000.wav
   ```

2. Insert SD card into audio module and reset

3. Test sound 0:
   - SD card present → plays high-quality SD card version
   - SD card missing → plays embedded version

## Expected Firmware Size

| Configuration | Size | Percentage of 3.94MB |
|--------------|------|----------------------|
| Base firmware | 1.25 MB | 31.7% |
| + 8 sounds @ 2s | 1.6 MB | 40.6% |
| + 8 sounds @ 5s | 2.1 MB | 53.3% |
| Available space | 2.69 MB | 68.3% |

**Current placeholder sounds**: 8 × 2 seconds = ~350KB total

## Troubleshooting

### Script Errors

**"ffmpeg not found"**:
```bash
sudo apt install ffmpeg
```

**"xxd not found"**:
```bash
sudo apt install vim-common
```

### Compilation Errors

**"file not found"**: Check includes in `audio_player.c` use correct path:
```c
#include "embedded/game_sound_0000_22050_8bit.h"  // ✓ Correct
```

**"undefined reference"**: Make sure PlatformIO is detecting src/embedded/*.c files (it should auto-detect).

### Sound Quality

**Sounds too quiet**: Increase volume in source WAV files before conversion.

**Sounds distorted**: Check source files aren't clipping (peaks should be < 0dB).

**Want higher quality**: Edit `tools/embed_sounds.sh` line 15:
```bash
EMBED_SAMPLE_RATE=44100  # Double quality, double size
```

## Advanced: Custom Sound Bank on SD Card

Users can create custom sound packs by placing WAV files on SD card:

```
/sdcard/sounds/
  0000.wav  # Custom game start (plays instead of embedded)
  0001.wav  # Custom victory
  # Missing files (2-7) will use embedded versions
  0100.wav  # Custom extra sound (not embedded)
```

This allows:
- ✅ Device works without SD card (embedded fallback)
- ✅ Users can customize sounds
- ✅ Support for high-quality audio (16-bit 44kHz)
- ✅ Unlimited SD card sounds (0-9999)

## More Information

- **Script Documentation**: `prompts/tools/EMBED_SOUNDS_SCRIPT.md`
- **Embedding Guide**: `docs/EMBEDDED_GAME_SOUNDS.md`
- **Multi-Format Audio**: `docs/MULTI_FORMAT_AUDIO.md`
- **CAN Protocol**: `/prompts/CANBUS_MESSAGE_SPEC.md`

