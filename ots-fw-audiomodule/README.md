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
- ✅ I2S audio output via AC101 codec
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

Current UART commands (115200 baud):
```
1       → Play track1.wav
2       → Play track2.wav
HELLO   → Play hello.wav
PING    → Play ping.wav
```

## Documentation

- **Full Documentation**: [PROJECT_PROMPT.md](PROJECT_PROMPT.md)
- **Hardware Spec**: `../ots-hardware/modules/sound-module.md`
- **CAN Protocol**: `../prompts/protocol-context.md`

## Project Status

**Phase 1:** ✅ Audio foundation complete (WAV playback)  
**Phase 2:** 🔄 CAN bus communication (in progress)  
**Phase 3:** ⏳ Advanced features (queue, priority, volume control)  
**Phase 4:** ⏳ Integration testing with main controller

---

**Framework:** ESP-IDF v5.4.2  
**Architecture:** Plain ESP-IDF (no ESP-ADF dependency)  
**Audio Format:** WAV only (16-bit PCM)  
**Last Updated:** January 3, 2026
