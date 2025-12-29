# OTS Sound Module Firmware (Audio) - Project Prompt

## Project Overview

This is the **Sound Module firmware** (formerly referred to as the "Audio Module") for the **OpenFront Tactical Suitcase (OTS)** project. It runs on an ESP32-A1S Audio Development Kit and provides audio playback capabilities as an external module controlled by the main controller board.

This module is designed to be controlled primarily over **CAN bus** (planned). The hardware/module specification lives at `../ots-hardware/modules/sound-module.md`.

## Hardware

**Board:** ESP32-A1S Audio Development Kit (AI Thinker ESP32 Audio Kit v2.2)
- **Chip:** ESP32-WROVER with PSRAM
- **Audio Codec:** AC101 (I2S)
- **Storage:** SD Card slot (SPI mode)
- **Interface:** UART (current), CAN Bus (planned)

### Pin Configuration (SD Card - SPI Mode)
- **CS:** GPIO 13
- **MISO:** GPIO 2
- **MOSI:** GPIO 15
- **SCK:** GPIO 14

## Architecture

### Current Implementation
- **Communication:** Serial/UART (115200 baud)
- **Command Protocol:** Simple text commands (e.g., "1", "HELLO", "PING")
- **Audio Storage:** WAV files on SD card (`/sdcard/`)
- **Audio Output:** I2S to AC101 codec
- **Command Mapping:** Static table mapping commands to WAV filenames

### Planned Features
- **CAN Bus Interface:** Replace/supplement UART with CAN bus communication
- **Integration:** Connect to main controller in `../of-fw-main`
- **Extended Commands:** More sophisticated command protocol
- **Status Reporting:** Playback status, errors, ready states

## CAN Protocol (Planned)

Recommended defaults (keep in sync with `../ots-hardware/modules/sound-module.md` and `../prompts/protocol-context.md`):

- CAN 2.0 classic, 500 kbps, standard 11-bit IDs
- 8-byte payload frames
- Little-endian multi-byte integers

### CAN IDs

- `0x420` PLAY_SOUND (main → sound)
- `0x421` STOP_SOUND (main → sound)
- `0x422` SOUND_STATUS (sound → main)
- `0x423` SOUND_ACK (sound → main, optional)

### PLAY_SOUND payload (`0x420`)

- Byte0 `cmd=0x01`
- Byte1 `flags`: bit0=interrupt, bit1=highPriority, bit2=loop
- Byte2-3 `soundIndex` (u16)
- Byte4 `volumeOverride` (u8, 0xFF=ignore/use pot)
- Byte6-7 `requestId` (u16)

### SD asset naming (recommended)

- Directory: `/sounds/`
- File: `NNNN.mp3` preferred, `NNNN.wav` fallback (example: `/sounds/0020.mp3`)

## Technology Stack

- **Framework:** ESP-IDF (Espressif IoT Development Framework)
- **Build System:** PlatformIO + CMake
- **RTOS:** FreeRTOS (included in ESP-IDF)
- **File System:** FAT32 on SD card (ESP-VFS)
- **Language:** C (C99 standard)

## Project Structure

```
ots-fw-audiomodule/
├── platformio.ini          # PlatformIO configuration
├── CMakeLists.txt          # Root CMake configuration
├── sdkconfig.esp32-a1s-espidf  # ESP-IDF SDK configuration
└── src/
    ├── CMakeLists.txt      # Source CMake configuration
    └── main.c              # Main application code
```

## Build & Flash

```bash
# Build the project
pio run

# Flash to ESP32
pio run -t upload

# Monitor serial output
pio run -t monitor

# Build + Flash + Monitor
pio run -t upload -t monitor
```

## Command Protocol (Current)

Commands are sent via UART (115200 baud) as text strings terminated by newline (`\n`).

### Command Table
| Command | File Played | Description |
|---------|-------------|-------------|
| `1` | `track1.wav` | Play track 1 |
| `2` | `track2.wav` | Play track 2 |
| `HELLO` | `hello.wav` | Play hello message |
| `PING` | `ping.wav` | Play ping sound |

### Example Usage
```
HELLO\n      → Plays /sdcard/hello.wav
1\n          → Plays /sdcard/track1.wav
```

## SD Card Setup

1. Format SD card as FAT32
2. Copy WAV files to root directory
3. Files should be one of:
    - **Preferred (if supported by firmware)**: MP3
    - Fallback: 16-bit PCM WAV (mono or stereo)

WAV recommendations:
- Sample rate: 44.1kHz or 48kHz

## Integration with OTS Main Controller

This module is designed to work as an external peripheral for the main OpenFront Tactical Suitcase controller firmware located in `../of-fw-main`.

### Communication Flow
```
Main Controller (of-fw-main)
    ↓ UART/CAN Command
Audio Module (ots-fw-audiomodule)
    ↓ Read WAV from SD Card
    ↓ Decode & Stream to I2S
AC101 Audio Codec
    ↓ Analog Output
Speakers/Headphones
```

## Documentation & Resources

### ESP32-A1S Audio Kit
- **Official ESP-ADF (Audio Development Framework):** https://docs.espressif.com/projects/esp-adf/en/latest/
- **ESP32-A1S Board Overview:** https://docs.ai-thinker.com/en/esp32-audio-kit
- **Schematic & Pinout:** https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit

### Sample Repositories
- **ESP32-A1S Examples (Ai-Thinker):** https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit
- **ESP-ADF Examples:** https://github.com/espressif/esp-adf/tree/master/examples
- **ESP-IDF Audio Examples:** https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s

### ESP-IDF Documentation
- **ESP-IDF Programming Guide:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
- **I2S Driver:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
- **SD Card / SPI Driver:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html
- **UART Driver:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html

### PlatformIO
- **PlatformIO ESP32 Platform:** https://docs.platformio.org/en/latest/platforms/espressif32.html
- **ESP-IDF Framework in PlatformIO:** https://docs.platformio.org/en/latest/frameworks/espidf.html

## Development Guidelines

### Code Style
- Follow C99 standard
- Use ESP-IDF logging macros (`ESP_LOGI`, `ESP_LOGE`, etc.)
- Keep functions modular and well-documented
- Use FreeRTOS tasks for concurrent operations

### Error Handling
- Check return values from ESP-IDF functions
- Log errors with appropriate severity levels
- Implement graceful fallbacks where possible

### Testing
- Test with various WAV file formats
- Verify SD card mount/unmount behavior
- Test command parsing edge cases
- Monitor memory usage (heap, stack)

## Future Enhancements

### Short Term
- [ ] Implement proper WAV file parsing and validation
- [ ] Add playback control (pause, stop, volume)
- [ ] Implement status LEDs/feedback
- [ ] Add error recovery mechanisms

### Medium Term
- [ ] CAN bus communication implementation
- [ ] Advanced command protocol with checksums
- [ ] Playlist/queue support
- [ ] Dynamic command table (configurable via SD card)

### Long Term
- [ ] Support for compressed audio formats (MP3, AAC)
- [ ] Multi-zone audio output
- [ ] Real-time audio streaming from main controller
- [ ] Audio mixing capabilities

## Troubleshooting

### SD Card Not Mounting
- Check wiring (CS, MISO, MOSI, SCK pins)
- Verify SD card is FAT32 formatted
- Check SD card compatibility (Class 10 recommended)
- Review serial logs for mount errors

### No Audio Output
- Verify I2S initialization
- Check AC101 codec configuration
- Ensure WAV file format is compatible
- Test with known-good audio files

### Build Errors
- Update PlatformIO platform: `pio pkg update`
- Clear build cache: `pio run -t clean`
- Verify ESP-IDF version compatibility
- Check `sdkconfig` settings

## License

Part of the OpenFront Tactical Suitcase project.

## Related Projects

- **Main Controller Firmware:** `../of-fw-main`
- **OpenFront Tactical Suitcase:** Parent project repository

---

**Last Updated:** December 4, 2025
**Target Platform:** ESP32-A1S Audio Development Kit
**Framework:** ESP-IDF via PlatformIO
