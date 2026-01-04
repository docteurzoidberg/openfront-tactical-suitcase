# OTS Test Audio - Pure ESP-IDF

Simple audio playback test firmware for ESP32-A1S audio board.

## Features

- **Pure ESP-IDF**: No ADF dependencies
- **ES8388 codec driver**: Based on ESP-ADF implementation
- **I2S audio output**: Native ESP-IDF driver (new API)
- **Embedded WAV playback**: Play test tone from flash

## Hardware

- ESP32-A1S Audio Development Kit
- ES8388 codec
- 8MB PSRAM
- Line-out/headphone jack

## Pin Configuration

- **I2S**: BCK=GPIO5, WS=GPIO25, DOUT=GPIO26
- **I2C**: SDA=GPIO33, SCL=GPIO32
- **Power Amp**: GPIO21

## Building

### Prerequisites

PlatformIO with ESP-IDF framework installed.

### Build and Upload

```bash
cd ots-test-audio
pio run -e esp32-a1s -t upload
pio device monitor
```

## Test Audio File

Place your test WAV file at `data/test_tone.wav`:
- **Format**: WAV PCM
- **Sample rate**: 44100 Hz (or any)
- **Bits**: 16-bit
- **Channels**: Stereo

## Expected Output

```
[ 1 ] Initialize ES8388 codec
ES8388: Power amplifier enabled (GPIO 21)
ES8388: I2C initialized
ES8388: Initializing ES8388 codec...
ES8388: ES8388 codec initialized successfully
[ 2 ] Initialize I2S
I2S: Initializing I2S @ 44100 Hz
I2S: I2S initialized successfully
I2S:   BCK: GPIO 5
I2S:   WS:  GPIO 25
I2S:   DO:  GPIO 26
[ 3 ] Set volume to 70%
ES8388: Volume set to 70% (reg=0x0a)
[ 4 ] Start DAC output
ES8388: Starting DAC output
[ 5 ] Playing audio (352800 bytes PCM)...
  Progress: 10% (35280 / 352800 bytes)
  Progress: 20% (70560 / 352800 bytes)
  ...
[ 6 ] Playback complete
  Total written: 352800 bytes
Test complete - system idle
```

## Troubleshooting

**No audio:**
- Check GPIO21 (power amp enable)
- Verify I2S pins
- Check I2C communication (ES8388 at 0x10)

**Distortion/clicks:**
- Verify WAV format (16-bit stereo PCM)
- Check sample rate matches
- Try lower volume (50-70%)

**Build errors:**
- Ensure ESP-IDF is properly installed
- Check `data/test_tone.wav` exists
