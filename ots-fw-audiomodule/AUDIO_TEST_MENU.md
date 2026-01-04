# Audio Test Menu Usage Guide

## Overview

The audio test menu provides an interactive serial interface for testing the audio mixer without requiring SD card files or CAN bus messages. It uses three embedded test tones stored in flash memory.

## Building with Test Menu

### PlatformIO
```bash
# Build with test menu enabled
pio run -e esp32-a1s-test-menu

# Upload and monitor
pio run -e esp32-a1s-test-menu -t upload && pio device monitor
```

### ESP-IDF (idf.py)
```bash
# Add flag to CMakeLists.txt or set environment variable
export AUDIO_TEST_MENU_ENABLE=1

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

## Building WITHOUT Test Menu (Production)

```bash
# Default build (test menu disabled)
pio run -e esp32-a1s-espidf

# Saves ~4MB flash by excluding embedded tone files
```

## Test Tones

The firmware includes 3 embedded test tones:

| Tone | Duration | Frequency | Description | Size |
|------|----------|-----------|-------------|------|
| Tone 1 | 1 second | 440 Hz | A4 (Middle A) | ~88 KB |
| Tone 2 | 2 seconds | 880 Hz | A5 (High A) | ~176 KB |
| Tone 3 | 5 seconds | 220 Hz | A3 (Low A) | ~440 KB |

All tones are 16-bit mono PCM at 44100 Hz sample rate.

## Serial Commands

Open serial monitor at 115200 baud and press these keys:

### Single Tone Playback
- `1` - Play Tone 1 (1s @ 440Hz)
- `2` - Play Tone 2 (2s @ 880Hz)
- `3` - Play Tone 3 (5s @ 220Hz)

### Mixer Tests
- `m` - **Mix All**: Play all 3 tones simultaneously (tests 3-source mixing)
- `d` - **Dual Mix**: Play tone 1 + 2 together (tests 2-source mixing)
- `t` - **Triple Sequential**: Play 1→2→3 with 500ms spacing (tests source queuing)

### Volume Tests
- `v` - **Volume Sweep**: Play tone 1 at 50% → 75% → 100% (tests volume control)

### Control Commands
- `s` - **Stop All**: Immediately stop all playing sounds
- `i` - **Info**: Display mixer status (active sources, capacity)
- `h` - **Help**: Display the test menu
- `?` - Same as `h`

## Example Session

```
=== Audio Test Menu - Mixer Testing ===

Command: 1
I (12345) TEST_MENU: Playing Tone 1 [1s @ 440Hz (A4)] at 80%

Command: m
I (15000) TEST_MENU: === MIX TEST: Playing all 3 tones simultaneously ===
I (15000) TEST_MENU: Playing Tone 1 [1s @ 440Hz (A4)] at 60%
I (15100) TEST_MENU: Playing Tone 2 [2s @ 880Hz (A5)] at 60%
I (15200) TEST_MENU: Playing Tone 3 [5s @ 220Hz (A3)] at 60%
I (15200) TEST_MENU: Listen for 3 overlapping tones!

Command: s
I (20000) TEST_MENU: Stopping all playback

Command: i
I (22000) TEST_MENU: === MIXER STATUS ===
I (22000) TEST_MENU: Active sources: 0 / 8
```

## Testing Scenarios

### Basic Functionality
1. Press `1` - Verify you hear 1 second tone
2. Press `2` - Verify you hear 2 second tone (higher pitch)
3. Press `3` - Verify you hear 5 second tone (lower pitch)

### Mixer Capacity
1. Press `m` - Should hear 3 tones playing together as a chord
2. All 3 should be audible and balanced
3. No clipping or distortion

### Sequential Playback
1. Press `t` - Tones should start 500ms apart
2. All 3 should play overlapping
3. Listen for smooth transitions

### Volume Control
1. Press `v` - Should hear same tone at 3 different volumes
2. 50% → 75% → 100% progression should be clear
3. No distortion at max volume

### Emergency Stop
1. Press `3` (long 5s tone)
2. Immediately press `s`
3. Tone should stop within 100ms

## Troubleshooting

### Menu Doesn't Appear
- Check that firmware was built with `AUDIO_TEST_MENU_ENABLE` flag
- Look for log line: `I (xxx) MAIN: *** AUDIO TEST MENU ENABLED ***`
- Verify serial monitor is at 115200 baud

### No Audio When Pressing Keys
- Check codec initialization: `I (xxx) MAIN: *** CODEC INITIALIZED ***`
- Verify MCLK enabled: `I (xxx) i2s: I2S0, MCLK output by GPIO0`
- Check mixer hardware ready: `audio_mixer_set_hardware_ready(true)` called

### Tones Sound Distorted
- Lower volume in commands (change `80` to `50` in code)
- Check for mixer overload: Press `i` to see active source count
- Verify I2S configuration (MCLK on GPIO 0!)

### Build Errors
- If you see `tone_*.c` or `audio_test_menu.c` errors:
  - These files are only included when `AUDIO_TEST_MENU_ENABLE` is defined
  - Build with `-e esp32-a1s-test-menu` environment
  - Or add `-DAUDIO_TEST_MENU_ENABLE` to build_flags

## Code Organization

```
src/
├── audio_test_menu.h        # Public API (ifdef-guarded)
├── audio_test_menu.c        # Menu implementation (ifdef-guarded)
└── embedded/
    ├── tone_1s_440hz.h      # 1-second tone declaration
    ├── tone_1s_440hz.c      # 1-second tone data array
    ├── tone_1s_440hz.wav    # Source WAV (not compiled)
    ├── tone_2s_880hz.h      # 2-second tone declaration
    ├── tone_2s_880hz.c      # 2-second tone data array
    ├── tone_2s_880hz.wav    # Source WAV
    ├── tone_5s_220hz.h      # 5-second tone declaration
    ├── tone_5s_220hz.c      # 5-second tone data array
    └── tone_5s_220hz.wav    # Source WAV
```

## Memory Usage

With test menu enabled:
- Flash: +4.2 MB (tone data arrays)
- RAM: +4 KB (menu task stack)

Without test menu (production):
- Flash: Same as before
- RAM: No overhead

## Implementation Details

### How It Works

1. **Serial Polling**: FreeRTOS task polls UART every 100ms
2. **Command Dispatch**: Single-character commands mapped to functions
3. **Embedded Playback**: Tones stored as C arrays in flash
4. **Mixer Integration**: Uses standard `audio_mixer_create_source_from_memory()` API
5. **Compile-Time Feature**: Entire module compiled out when flag not set

### Adding New Test Sounds

1. Generate WAV file (16-bit mono, 44100 Hz):
   ```python
   import wave, struct, math
   with wave.open('newsound.wav', 'w') as f:
       f.setnchannels(1)
       f.setsampwidth(2)
       f.setframerate(44100)
       # ... write samples ...
   ```

2. Convert to C array:
   ```bash
   xxd -i newsound.wav > embedded/newsound.c
   ```

3. Create header file (`embedded/newsound.h`)

4. Add to `s_test_tones[]` array in `audio_test_menu.c`

5. Add keyboard shortcut in `handle_command()` switch

## See Also

- `/ots-fw-audiomodule/AUDIO_WORKING_CONFIG.md` - Audio hardware setup
- `/ots-fw-audiomodule/ESP32_A1S_AUDIOKIT_BOARD.md` - Board documentation
- `/ots-fw-audiomodule/src/audio_mixer.c` - Mixer implementation
