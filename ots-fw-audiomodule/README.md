# OTS Sound Module Firmware

ESP32-A1S audio playback module for the OpenFront Tactical Suitcase (OTS). Provides CAN-controlled game event sounds with WAV playback.

## Quick Start

**Hardware:** ESP32-A1S Audio Development Kit (AI-Thinker v2.2)  
**Framework:** Plain ESP-IDF v5.4.2 (no ESP-ADF)

```bash
# Build firmware
pio run -e esp32-a1s-espidf

# Flash to device
pio run -e esp32-a1s-espidf -t upload

# Monitor logs
pio device monitor
```

## Features

- ✅ WAV file playback from SD card (16-bit PCM)
- ✅ Multi-source audio mixing (up to 8 simultaneous sounds)
- ✅ I2S audio output via AC101 codec
- ✅ Modular architecture (refactored for maintainability)
- ✅ UART command interface (testing)
- 🔄 CAN bus protocol (in progress)
- ⏳ Integration with ots-fw-main controller

## Hardware Requirements

- **ESP32-A1S** Audio Development Kit
- **MicroSD card** (FAT32, with audio files in `/sounds/`)
- **External CAN transceiver** (TJA1050 or SN65HVD230) for CAN communication
- **USB cable** for programming

## SD Card Setup

1. Format SD card as FAT32
2. Create `/sounds/` directory
3. Copy WAV audio files:
   - Format: `0001.wav`, `0002.wav`, etc.
   - Specs: 16-bit PCM, 44.1kHz, stereo or mono

## Testing

### Serial Commands

Current UART commands (115200 baud):
```
1       → Play track1.wav
2       → Play track2.wav
HELLO   → Play hello.wav
PING    → Play ping.wav
```

### Boot Log Capture

To capture complete boot sequences (including early bootloader logs):

```bash
# Capture full boot (auto-detect port, 10 seconds)
./tools/capture_boot.py

# Save to file for debugging
./tools/capture_boot.py --output boot_logs.txt

# Longer capture (includes runtime logs)
./tools/capture_boot.py --duration 20
```

See [Boot Capture Tool Guide](docs/BOOT_CAPTURE_TOOL.md) for details.

## Architecture

Modular design with clear separation of concerns:

**Audio Pipeline:**
- `wav_utils` - Shared WAV parsing (116 lines)
- `audio_decoder` - Per-source WAV decoder tasks (80 lines)
- `audio_mixer` - Multi-source mixing & I2S output (370 lines)
- `audio_player` - Simple blocking playback API (87 lines)

**Hardware Abstraction:**
- `hardware/i2s` - I2S audio output
- `hardware/ac101` - AC101 codec control
- `hardware/sdcard` - SD card FAT32 mount
- `hardware/i2c`, `hardware/gpio` - Low-level drivers

**Communication:**
- `can_handler` - CAN bus message handling (159 lines)
- `serial_commands` - UART debug interface (122 lines)

**Benefits:**
- Single source of truth for WAV parsing
- Easy to test individual components
- Clean interfaces between modules
- 59% code reduction in audio_player

## Documentation

- **Full Documentation**: [PROJECT_PROMPT.md](PROJECT_PROMPT.md)
- **Hardware Spec**: `../ots-hardware/modules/sound-module.md`
- **CAN Protocol**: `../prompts/WEBSOCKET_MESSAGE_SPEC.md`

## Project Status

**Phase 1:** ✅ Audio foundation complete (WAV playback + mixing)  
**Refactoring:** ✅ Modular architecture (wav_utils, decoder separation)  
**Phase 2:** 🔄 CAN bus communication (in progress)  
**Phase 3:** ⏳ Advanced features (queue, priority, volume control)  
**Phase 4:** ⏳ Integration testing with main controller

**Build Stats:**
- Flash: 343,936 bytes (8.2% of 4MB)
- RAM: 30,040 bytes (9.2% of 327KB)
- Total code: ~2,400 lines (well-organized modules)

---

**Framework:** ESP-IDF v5.4.2  
**Architecture:** Modular design with hardware abstraction  
**Audio Format:** WAV only (16-bit PCM)  
**Last Updated:** January 4, 2026
