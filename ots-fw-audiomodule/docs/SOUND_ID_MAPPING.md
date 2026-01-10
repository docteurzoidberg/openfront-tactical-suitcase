# Sound ID Mapping and Naming Convention

## CAN Protocol Sound ID Ranges

### SD Card Sounds (0-9999)

Sound IDs 0-9999 are reserved for game sounds stored on SD card at `/sdcard/sounds/XXXX.wav`.

**SD Card Priority**: If SD card is present and file exists, it will be played with high quality.

**Embedded Fallback**: Sounds 0-7 and 0100 have low-quality embedded versions (8-bit 22.05kHz mono) as fallback.

| Sound ID | Filename | Embedded? | Description | Naming Pattern |
|----------|----------|-----------|-------------|----------------|
| 0 | 0000.wav | ✓ Yes | Game start sound | `game_sound_0000_22050_8bit` |
| 1 | 0001.wav | ✓ Yes | Victory sound | `game_sound_0001_22050_8bit` |
| 2 | 0002.wav | ✓ Yes | Defeat sound | `game_sound_0002_22050_8bit` |
| 3 | 0003.wav | ✓ Yes | Player death sound | `game_sound_0003_22050_8bit` |
| 4 | 0004.wav | ✓ Yes | Nuclear alert sound | `game_sound_0004_22050_8bit` |
| 5 | 0005.wav | ✓ Yes | Land invasion alert | `game_sound_0005_22050_8bit` |
| 6 | 0006.wav | ✓ Yes | Naval invasion alert | `game_sound_0006_22050_8bit` |
| 7 | 0007.wav | ✓ Yes | Nuke launch sound | `game_sound_0007_22050_8bit` |
| 100 | 0100.wav | ✓ Yes | Audio module ready (boot) | `game_sound_0100_22050_8bit` |
| 8-9999 | XXXX.wav | ✗ No | Custom sounds | N/A |

### Embedded Test Sounds (10000-10199)

Sound IDs 10000-10199 are reserved for embedded test tones and special sounds stored in flash.

**No SD Card Fallback**: These sounds are always played from embedded flash memory.

| Sound ID | CAN ID | Description | Naming Pattern | Format |
|----------|--------|-------------|----------------|--------|
| 10000 | 0x2710 | Test tone 1 (440Hz, 1 second) | `game_sound_10000_22050_8bit` | 8-bit 22.05kHz mono |
| 10001 | 0x2711 | Test tone 2 (880Hz, 2 seconds) | `game_sound_10001_22050_8bit` | 8-bit 22.05kHz mono |
| 10002 | 0x2712 | Test tone 3 (220Hz, 5 seconds) | `game_sound_10002_22050_8bit` | 8-bit 22.05kHz mono |
| 10100 | 0x2774 | Quack sound (0.1 seconds) | `game_sound_10100_22050_8bit` | 8-bit 22.05kHz mono |

## Naming Convention

### Consistent Pattern for All Embedded Sounds

**Format**: `game_sound_XXXXX_22050_8bit`

Where:
- `XXXXX` = 5-digit CAN sound ID (with leading zeros)
- `22050` = Sample rate in Hz (22.05kHz)
- `8bit` = Bit depth (8-bit unsigned PCM)

### File Structure

Each embedded sound consists of 3 components:

**1. Header File** (`include/embedded/game_sound_XXXXX_22050_8bit.h`):
```c
#pragma once
#include <stdint.h>
#include <stddef.h>

extern const uint8_t game_sound_XXXXX_22050_8bit_wav[];
extern const size_t game_sound_XXXXX_22050_8bit_wav_len;
```

**2. Source File** (`src/embedded/game_sound_XXXXX_22050_8bit.c`):
```c
#include "embedded/game_sound_XXXXX_22050_8bit.h"

const uint8_t game_sound_XXXXX_22050_8bit_wav[] = {
    // WAV file binary data (xxd format)
};

const size_t game_sound_XXXXX_22050_8bit_wav_len = sizeof(game_sound_XXXXX_22050_8bit_wav);
```

**3. Source WAV File** (`sounds/XXXXX.wav`):
- Input format: Any WAV format (will be converted)
- Conversion: Automatic to 8-bit 22.05kHz mono by embed script

## Code References

### Test Tones (audio_tone_player.c)

```c
#include "embedded/game_sound_10000_22050_8bit.h"  // Test tone 1
#include "embedded/game_sound_10001_22050_8bit.h"  // Test tone 2
#include "embedded/game_sound_10002_22050_8bit.h"  // Test tone 3

// Access via tone_player_play(TONE_ID_1, volume, &handle)
// Mapped to CAN IDs: 10000, 10001, 10002
```

### Quack Sound (audio_player.c)

```c
#include "embedded/game_sound_10100_22050_8bit.h"  // Quack (CAN ID 10100)

// Access via audio_player_play_embedded_wav(&handle)
// Mapped to CAN ID: 10100
```

### Game Sounds (audio_player.c - TODO)

```c
#include "embedded/game_sound_0000_22050_8bit.h"  // game_start
#include "embedded/game_sound_0001_22050_8bit.h"  // game_victory
// ... etc

// Access via audio_player_play_sound_by_index(0-7, ...)
// With SD card priority and embedded fallback
```

## CAN Protocol Integration

### PLAY_SOUND Command (0x420)

```
Byte 0-1: Sound Index (16-bit little-endian)
Byte 2:   Flags (loop, interrupt)
Byte 3:   Volume (0-100)
Bytes 4-7: Reserved

Examples:
- Test tone 1: [0x10, 0x27, 0x00, 0x64, ...]  # 0x2710 = 10000
- Test tone 2: [0x11, 0x27, 0x00, 0x64, ...]  # 0x2711 = 10001
- Quack:       [0x74, 0x27, 0x00, 0x64, ...]  # 0x2774 = 10100
- Game start:  [0x00, 0x00, 0x00, 0x64, ...]  # 0x0000 = 0
```

## Firmware Flash Usage

**Current Status** (as of 2026-01-09):
- Total firmware: 639,775 bytes (15.3% of 4MB flash)
- Embedded sounds: ~3.1MB source (compressed to ~1MB in firmware)
- Available space: ~3.5MB for additional embedded audio

**Format Impact**:
- 8-bit 22.05kHz mono: ~22KB/second
- 16-bit 44.1kHz stereo: ~176KB/second (8× larger)
- Current 12 embedded sounds: Total ~400KB embedded data

## Future Extensions

### Reserved ID Ranges

- **10200-19999**: Reserved for future embedded sounds
- **20000-65535**: Reserved for custom/user sounds

### Adding New Embedded Sounds

1. Create WAV file: `sounds/XXXXX.wav` (any format)
2. Run embed script: `./tools/embed_sounds.sh`
3. Include header in source: `#include "embedded/game_sound_XXXXX_22050_8bit.h"`
4. Access variables: `game_sound_XXXXX_22050_8bit_wav`, `game_sound_XXXXX_22050_8bit_wav_len`

## See Also

- **Embed Script**: [tools/embed_sounds.sh](../tools/embed_sounds.sh)
- **Script Documentation**: [prompts/tools/EMBED_SOUNDS_SCRIPT.md](../prompts/tools/EMBED_SOUNDS_SCRIPT.md)
- **CAN Protocol**: [prompts/CANBUS_MESSAGE_SPEC.md](../../prompts/CANBUS_MESSAGE_SPEC.md)
- **Quick Start**: [QUICKSTART_EMBEDDING.md](../QUICKSTART_EMBEDDING.md)
