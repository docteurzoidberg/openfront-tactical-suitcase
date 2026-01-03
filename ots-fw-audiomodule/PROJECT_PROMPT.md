# OTS Sound Module Firmware - Project Documentation

## Project Overview

**Sound Module firmware** for the **OpenFront Tactical Suitcase (OTS)** project. Runs on ESP32-A1S Audio Development Kit and provides CAN-controlled audio playback for game events.

**Architecture:** Plain ESP-IDF (no ESP-ADF dependency) for maximum compatibility and control.

**Hardware spec:** `../ots-hardware/modules/sound-module.md`  
**CAN Protocol:** `../prompts/protocol-context.md` (CAN IDs 0x420-0x423)

## Hardware Platform

**Board:** ESP32-A1S Audio Development Kit (AI-Thinker ESP32 Audio Kit v2.2)
- **MCU:** ESP32-WROVER-B (4MB Flash, 8MB PSRAM)
- **Audio Codec:** AC101 (I2S interface)
- **Storage:** MicroSD card slot (SPI mode)
- **CAN Interface:** GPIO 21 (TX), GPIO 22 (RX) via external transceiver

### ESP32-A1S Pin Configuration

**I2S Audio (AC101 Codec):**
- BCK (Bit Clock): GPIO 27
- WS (Word Select): GPIO 25
- DOUT (Data Out to codec): GPIO 26
- DIN (Data In from codec): GPIO 35
- MCLK (Master Clock): GPIO 0

**I2C Control (AC101 Config):**
- SDA: GPIO 33
- SCL: GPIO 32
- Address: 0x1A

**SD Card (SPI Mode):**
- CS: GPIO 13
- MISO: GPIO 2
- MOSI: GPIO 15
- SCK: GPIO 14

**CAN Bus (TWAI):**
- TX: GPIO 21
- RX: GPIO 22
- **Requires external CAN transceiver** (TJA1050 or SN65HVD230)

## Technology Stack

### Core Framework
- **ESP-IDF v5.4.2** (native Espressif framework)
- **Build System:** PlatformIO + CMake
- **RTOS:** FreeRTOS (built into ESP-IDF)
- **Language:** C (C99 standard)

### Audio Pipeline (Custom - No ESP-ADF)
- **Decoder:** minimp3 (lightweight MP3 decoder) or libhelix
- **File I/O:** ESP-VFS + FAT32 on SD card
- **I2S Streaming:** ESP-IDF I2S driver
- **Codec Control:** AC101 driver via I2C
- **NO ESP-ADF dependency** - full control, lighter binary

### Communication
- **Primary:** CAN bus (TWAI driver) at 500 kbps
- **Debug:** UART console (115200 baud)

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    OTS Sound Module                          │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐      ┌──────────────┐      ┌───────────┐ │
│  │ CAN RX Task  │─────▶│ Sound Queue  │─────▶│ MP3 Player│ │
│  │ (500kbps)    │      │ (FreeRTOS)   │      │ Task      │ │
│  └──────────────┘      └──────────────┘      └─────┬─────┘ │
│                                                      │       │
│  ┌──────────────┐      ┌──────────────┐           │       │
│  │ Status Task  │◀─────│ SD Card      │◀──────────┘       │
│  │ (CAN TX)     │      │ /sounds/     │                    │
│  └──────────────┘      └──────────────┘                    │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐      ┌──────────────┐                    │
│  │ I2S Stream   │─────▶│ AC101 Codec  │─────▶ 🔊 Speakers │
│  │ DMA Buffer   │      │ (I2C+I2S)    │                    │
│  └──────────────┘      └──────────────┘                    │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### Audio Playback Flow

1. **CAN Command Received** (0x420 PLAY_SOUND)
2. **Sound Index Extracted** (e.g., 1 = `/sounds/0001.mp3`)
3. **File Opened** from SD card via FatFS
4. **MP3 Decoded** frame by frame using minimp3
5. **PCM Streamed** to I2S DMA buffer
6. **AC101 Plays** analog audio output

## CAN Protocol Implementation

### CAN IDs (Standard 11-bit)

| ID | Direction | Purpose |
|----|-----------|---------|
| 0x420 | Main → Sound | PLAY_SOUND command |
| 0x421 | Main → Sound | STOP_SOUND command |
| 0x422 | Sound → Main | STATUS report (periodic) |
| 0x423 | Sound → Main | ACK response (optional) |

### PLAY_SOUND (0x420)

**Frame Format:**
```
Byte 0: Command (0x01)
Byte 1: Flags (bit0=interrupt, bit1=highPriority, bit2=loop)
Byte 2-3: Sound Index (u16 little-endian)
Byte 4: Volume Override (0xFF = use potentiometer)
Byte 5: Reserved
Byte 6-7: Request ID (u16 little-endian)
```

**Example:** Play sound #1 with high priority
```
ID=0x420 DLC=8 DATA=[01 02 01 00 FF 00 00 00]
         ^^^^        ^^    ^^^^^
         CMD         Flags Index=1
```

### STOP_SOUND (0x421)

```
Byte 0: Command (0x02)
Byte 1: Flags (bit0=stopAll)
Byte 2-3: Sound Index (0x0000 = stop current)
Byte 4-7: Reserved
```

### STATUS (0x422)

Sent periodically (1Hz) or on state change:
```
Byte 0: State (0x00=idle, 0x01=playing, 0x02=paused, 0xFF=error)
Byte 1: Current sound index (u8, high byte)
Byte 2: Current sound index (u8, low byte)
Byte 3: Volume (0-100)
Byte 4: Error code (if state=0xFF)
Byte 5-7: Reserved
```

## SD Card Structure

### Directory Layout
```
/sounds/
  ├── 0001.mp3  (game_start)
  ├── 0002.mp3  (game_player_death)
  ├── 0003.mp3  (game_victory)
  ├── 0004.mp3  (game_defeat)
  ├── 0010.mp3  (alert_atom)
  ├── 0011.mp3  (alert_hydro)
  ├── 0012.mp3  (alert_mirv)
  ├── 0013.mp3  (alert_land)
  ├── 0014.mp3  (alert_naval)
  └── ...
```

### File Naming Convention
- **Format:** `NNNN.mp3` (4-digit zero-padded)
- **Fallback:** `NNNN.wav` (if MP3 not available)
- **Index Range:** 0001-9999
- **Audio Specs:** MP3 320kbps, 44.1kHz stereo recommended

### Sound Index Mapping

See `../prompts/protocol-context.md` for canonical soundId → soundIndex mapping table.

## Project Structure

```
ots-fw-audiomodule/
├── platformio.ini              # PlatformIO configuration
├── CMakeLists.txt              # Root CMake config
├── sdkconfig.esp32-a1s-espidf  # ESP-IDF SDK config
├── PROJECT_PROMPT.md           # This file
├── README.md                   # Quick start guide
├── prompts/                    # Development prompts
└── src/
    ├── CMakeLists.txt
    ├── main.c                  # Application entry point
    ├── can_protocol.c/h        # CAN message handling (future)
    ├── audio_player.c/h        # MP3 playback engine (future)
    ├── ac101_driver.c/h        # AC101 codec control (future)
    └── sd_manager.c/h          # SD card file operations (future)
```

## Build & Flash

### Using PlatformIO (Recommended)

```bash
cd ots-fw-audiomodule

# Build firmware
pio run -e esp32-a1s-espidf

# Flash to device
pio run -e esp32-a1s-espidf -t upload

# Monitor serial output
pio device monitor --port /dev/ttyUSB0

# Flash + Monitor
pio run -e esp32-a1s-espidf -t upload && pio device monitor
```

### Configuration

Edit `platformio.ini` to adjust:
- Board type (default: `esp-wrover-kit`)
- Monitor speed (default: 115200)
- Build flags (pin definitions, features)

## Development Roadmap

### Phase 1: Audio Foundation ✅ (Current)
- [x] ESP-IDF base project setup
- [x] I2S + AC101 codec initialization
- [x] SD card mount (FAT32)
- [x] WAV file playback
- [x] UART command interface (basic testing)

### Phase 2: MP3 Support 🔄 (Next)
- [ ] Integrate minimp3 decoder library
- [ ] Frame-by-frame MP3 decoding
- [ ] I2S DMA buffer management
- [ ] Memory optimization (PSRAM usage)
- [ ] Gapless playback support

### Phase 3: CAN Bus Integration ⏳
- [ ] TWAI driver initialization (500 kbps)
- [ ] CAN protocol parser (0x420-0x423)
- [ ] FreeRTOS queue for sound commands
- [ ] STATUS reporting (0x422)
- [ ] ACK responses (0x423)

### Phase 4: Advanced Features ⏳
- [ ] Interrupt/priority handling
- [ ] Volume control via potentiometer/CAN
- [ ] Loop playback mode
- [ ] Sound queue (multiple requests)
- [ ] Error recovery and retry logic

### Phase 5: Integration & Testing ⏳
- [ ] Device-to-device CAN testing with ots-fw-main
- [ ] End-to-end game event testing
- [ ] Performance optimization
- [ ] Production firmware release

## Current Implementation (Legacy/Testing)

**UART Protocol (115200 baud):**
Commands are text strings terminated by newline (`\n`).

| Command | File Played | Description |
|---------|-------------|-------------|
| `1` | `track1.wav` | Play track 1 |
| `2` | `track2.wav` | Play track 2 |
| `HELLO` | `hello.wav` | Play hello message |
| `PING` | `ping.wav` | Play ping sound |

**Usage:**
```
HELLO\n      → Plays /sdcard/hello.wav
1\n          → Plays /sdcard/track1.wav
```

**Note:** UART protocol is for testing only. Production will use CAN bus.

---

**Last Updated:** January 3, 2026  
**Framework:** ESP-IDF v5.4.2 (plain, no ESP-ADF)  
**Status:** Phase 1 complete, Phase 2 in progress

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
