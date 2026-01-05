# OTS Sound Module Firmware - Project Documentation

## Project Overview

**Sound Module firmware** for the **OpenFront Tactical Suitcase (OTS)** project. Runs on ESP32-A1S Audio Development Kit and provides CAN-controlled audio playback for game events.

**Architecture:** Plain ESP-IDF (no ESP-ADF dependency) for maximum compatibility and control.

**Hardware spec:** `../ots-hardware/modules/sound-module.md`  
**CAN Protocol:** `../prompts/CANBUS_MESSAGE_SPEC.md` (CAN IDs 0x410-0x42F)

## Hardware Platform

**Board:** ESP32-A1S Audio Development Kit (AI-Thinker ESP32 Audio Kit v2.2)
- **MCU:** ESP32-WROVER-B (4MB Flash, 8MB PSRAM)
- **Audio Codec:** ES8388 (I2S interface) - NOT AC101!
- **Storage:** MicroSD card slot (SPI mode)
- **CAN Interface:** GPIO 21 (TX), GPIO 22 (RX) via external transceiver

### ESP32-A1S Pin Configuration (VERIFIED WORKING)

**I2S Audio (ES8388 Codec):**
- **MCLK (Master Clock): GPIO 0** ⚠️ **CRITICAL - MUST BE ENABLED!**
  - ES8388 REQUIRES MCLK to generate internal audio clocks
  - Setting `.mclk = I2S_GPIO_UNUSED` causes silent playback
  - This is the #1 cause of "no audio" issues
- BCK (Bit Clock): GPIO 27 (NOT GPIO 5!)
- WS (Word Select): GPIO 25
- DOUT (Data Out to codec): GPIO 26
- DIN (Data In from codec): GPIO 35

**I2C Control (ES8388 Config):**
- SDA: GPIO 33
- SCL: GPIO 32
- Address: 0x10 (not 0x1A!)

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
- **Audio Format:** WAV only (16-bit PCM)
- **File I/O:** ESP-VFS + FAT32 on SD card
- **I2S Streaming:** ESP-IDF I2S driver
- **Codec Control:** AC101 driver via I2C
- **NO ESP-ADF dependency** - full control, lighter binary

### Communication
- **Primary:** CAN bus (TWAI driver) at 500 kbps
- **Debug:** UART console (115200 baud)

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    OTS Sound Module (Modular)                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌──────────────┐      ┌─────────────────┐      ┌────────────────┐ │
│  │ CAN Handler  │─────▶│  Audio Mixer    │◀─────│ Serial Cmds    │ │
│  │ (500kbps)    │      │  (Multi-source) │      │ (UART debug)   │ │
│  └──────────────┘      └────────┬────────┘      └────────────────┘ │
│                                  │                                    │
│  ┌──────────────────────────────┼──────────────────────────┐       │
│  │          Per-Source Pipeline │                          │       │
│  │  ┌───────────────┐  ┌────────▼─────────┐  ┌─────────┐ │       │
│  │  │ WAV Utils     │◀─│ Audio Decoder    │◀─│ SD Card │ │       │
│  │  │ (Shared)      │  │ (FreeRTOS Task)  │  │/sounds/ │ │       │
│  │  └───────────────┘  └────────┬─────────┘  └─────────┘ │       │
│  │                              │                          │       │
│  │                              ▼ PCM Stream              │       │
│  │                     ┌─────────────────┐                │       │
│  │                     │ Stream Buffer   │                │       │
│  │                     └─────────────────┘                │       │
│  └──────────────────────────────┬──────────────────────────┘       │
│                                  │ Volume Mixing                    │
│                                  ▼                                  │
│  ┌──────────────┐      ┌──────────────┐                            │
│  │ I2S Driver   │─────▶│ AC101 Codec  │─────▶ 🔊 Speakers         │
│  │ (DMA Buffer) │      │ (I2C+I2S)    │                            │
│  └──────────────┘      └──────────────┘                            │
│                                                                      │
│  Hardware Abstraction Layer: i2s.c, ac101.c, i2c.c, sdcard.c       │
└──────────────────────────────────────────────────────────────────────┘
```

### Audio Playback Flow

1. **CAN Command Received** (0x420 PLAY_SOUND)
2. **Sound Index Extracted** (e.g., 1 = `/sounds/0001.wav`)
3. **Audio Mixer** creates new source slot
4. **Decoder Task** spawned for this source:
   - Opens file from SD card via FatFS
   - Parses WAV header using **wav_utils** (shared)
   - Streams PCM data to source's stream buffer
5. **Mixer Task** combines all active sources:
   - Reads from each source's stream buffer
   - Applies volume control
   - Mixes to single output stream
6. **I2S Driver** writes mixed PCM to DMA buffer
7. **AC101 Codec** outputs analog audio

## Code Organization

### Core Audio Modules (Refactored)

The audio system has been refactored into focused, single-responsibility modules:

**wav_utils.c/h** (116 lines) - Shared WAV Parsing
- Single source of truth for WAV file parsing
- Unified type definitions (`wav_info_t`)
- Endian conversion helpers (`wav_read_le16/32`)
- Robust RIFF/WAVE parser with chunk navigation
- **Used by:** audio_player, audio_decoder

**audio_decoder.c/h** (80 lines) - WAV Decoder Task
- FreeRTOS task that reads WAV files
- Streams PCM data to stream buffer
- Handles looping and EOF detection
- **Uses:** wav_utils for parsing
- **Separation:** Isolated from mixing logic

**audio_mixer.c/h** (370 lines) - Multi-Source Mixing
- Manages up to 8 simultaneous audio sources
- Volume control per source
- Combines sources into single output stream
- Writes mixed audio to I2S
- **Uses:** audio_decoder tasks for each source

**audio_player.c/h** (87 lines) - Simple WAV Playback
- High-level API for single-file playback
- Blocking playback (simpler use case)
- **Uses:** wav_utils for parsing
- **Legacy:** Kept for backward compatibility

### Refactoring Results

**Phase 1: WAV Utils Extraction**
- Eliminated 150 lines of duplicate code
- Unified wav_info_t type (was defined in 3 places)
- Single parsing implementation (was duplicated in 2 files)
- audio_player.c: 212 → 87 lines (-59%)

**Phase 2: Decoder Separation**
- Split audio_mixer.c: 428 → 370 lines
- Extracted decoder_task → audio_decoder.c (80 lines)
- Clear separation: mixing vs decoding
- Better testability and maintainability

**Total Codebase Stats:**
- Main application: 121 lines (83% reduction from 717)
- Audio modules: 713 lines (well-organized)
- Hardware layer: ~500 lines (abstracted)
- Total: ~2,400 lines (down from ~2,600)

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
  ├── 0001.wav  (game_start)
  ├── 0002.wav  (game_player_death)
  ├── 0003.wav  (game_victory)
  ├── 0004.wav  (game_defeat)
  ├── 0010.wav  (alert_atom)
  ├── 0011.wav  (alert_hydro)
  ├── 0012.wav  (alert_mirv)
  ├── 0013.wav  (alert_land)
  ├── 0014.wav  (alert_naval)
  └── ...
```

### File Naming Convention
- **Format:** `NNNN.wav` (4-digit zero-padded)
- **Index Range:** 0001-9999
- **Audio Specs:** 16-bit PCM WAV, 44.1kHz stereo/mono

### Sound Index Mapping

See `../prompts/WEBSOCKET_MESSAGE_SPEC.md` for canonical soundId → soundIndex mapping table.

## Project Structure

```
ots-fw-audiomodule/
├── platformio.ini              # PlatformIO configuration
├── CMakeLists.txt              # Root CMake config
├── sdkconfig.esp32-a1s-espidf  # ESP-IDF SDK config
├── PROJECT_PROMPT.md           # This file (architecture & dev guide)
├── CONSOLE_COMMANDS.md         # User documentation (MUST MAINTAIN!)
├── README.md                   # Quick start guide
├── prompts/                    # Development prompts
└── src/
    ├── CMakeLists.txt
    ├── main.c                  # Application entry point (121 lines)
    │
    ├── audio_mixer.c/h         # Multi-source mixing & management (370 lines)
    ├── audio_decoder.c/h       # WAV decoder task (80 lines)
    ├── audio_player.c/h        # Simple WAV playback (87 lines)
    ├── wav_utils.c/h           # Shared WAV parsing (116 lines)
    │
    ├── audio_console.c/h       # Interactive console (16 commands) ⚠️ UPDATE DOCS!
    ├── can_handler.c/h         # CAN message handling (159 lines)
    │
    ├── board_config.h          # ESP32-A1S pin definitions
    │
    └── hardware/               # Hardware abstraction layer
        ├── i2s.c/h             # I2S audio output
        ├── es8388.c/h          # ES8388 codec driver
        ├── i2c.c/h             # I2C bus management
        ├── gpio.c/h            # GPIO initialization
        └── sdcard.c/h          # SD card FAT32 mount
```

**⚠️ Documentation Maintenance:**
- When modifying `src/audio_console.c`, update `CONSOLE_COMMANDS.md`
- When changing command behavior, update documentation examples
- When adding features to mixer/player, update architecture docs

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

### Phase 1: Audio Foundation ✅ (Complete)
- [x] ESP-IDF base project setup
- [x] I2S + AC101 codec initialization
- [x] SD card mount (FAT32)
- [x] WAV file playback
- [x] UART command interface (basic testing)

### Phase 2: CAN Bus Integration 🔄 (Next)
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

**Last Updated:** January 4, 2026  
**Framework:** ESP-IDF v5.4.2 (plain, no ESP-ADF)  
**Audio Format:** WAV only (16-bit PCM)  
**Code Status:** Refactored into modular architecture  
**Build Status:** ✅ 343KB flash (8.2%), 30KB RAM (9.2%)  
**Phase Status:** Phase 1 complete + refactored, Phase 2 (CAN) in progress

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

### Project Documentation

#### **CONSOLE_COMMANDS.md** - User Command Reference
**⚠️ IMPORTANT: Keep this documentation up-to-date!**

Complete user documentation for all serial console commands. This file MUST be updated whenever:
- ✏️ Adding new commands to `audio_console.c`
- ✏️ Modifying command behavior or parameters
- ✏️ Removing or deprecating commands
- ✏️ Changing command syntax or output format

**Maintenance checklist when modifying console commands:**
1. Update command implementation in `src/audio_console.c`
2. Update command registration in console command array
3. Update `CONSOLE_COMMANDS.md` with:
   - Command syntax and parameters
   - Usage examples with actual output
   - Error conditions and handling
   - Any behavioral changes
4. Update version history section in `CONSOLE_COMMANDS.md`
5. Test the command and verify documentation accuracy

**Current commands (16 total):**
- WAV playback: `play`, `1`, `2`, `hello`, `ping`
- Tone generation: `tone1`, `tone2`, `tone3`
- Volume control: `volume`
- Mixer control: `status`, `stop`, `mix`, `test`
- System info: `ls`, `sysinfo`, `info`

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
- **After adding/modifying console commands:**
  - Test the new/modified command thoroughly
  - Verify all parameter combinations
  - Check error handling paths
  - Update `CONSOLE_COMMANDS.md` with examples
  - Verify documentation matches actual behavior

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

## CAN Protocol Documentation (CRITICAL)

**CAN Bus Protocol:**
- **Specification**: [`/prompts/CANBUS_MESSAGE_SPEC.md`](../prompts/CANBUS_MESSAGE_SPEC.md) - Single source of truth (authoritative)
- **Developer Guide**: [`/doc/developer/canbus-protocol.md`](../doc/developer/canbus-protocol.md) - Implementation patterns and debugging
- **Component APIs**: `/ots-fw-shared/components/*/COMPONENT_PROMPT.md` files for hardware abstraction

🤖 **AI Guidelines: CAN Protocol Changes**

When user requests CAN protocol changes (new messages, CAN IDs, or data fields):

1. **Update BOTH files in this order:**
   - First: `prompts/CANBUS_MESSAGE_SPEC.md` (add message definition with byte layout)
   - Second: `doc/developer/canbus-protocol.md` (add C implementation examples)

2. **Keep them synchronized:**
   - Spec shows WHAT messages look like (CAN ID, DLC, byte layout, data fields)
   - Dev doc shows HOW to implement in C (code examples, patterns, debugging)
   - Both must reflect the same CAN IDs and message formats

3. **Audio module-specific updates:**
   - Update shared component (`can_audiomodule`) if protocol-level changes
   - Update handlers in `src/can_audio_handler.c` for behavior changes
   - Test changes in BOTH audio module AND main controller firmware
   - Document sound playback behavior in spec

4. **Verification checklist:**
   - [ ] Message exists in spec with byte layout diagram
   - [ ] Message has C implementation example in dev doc
   - [ ] Both files mention same CAN IDs
   - [ ] Audio behavior documented in both files

**When modifying the CAN protocol** (adding messages, changing formats, etc.):
1. ⚠️ **UPDATE `/prompts/CANBUS_MESSAGE_SPEC.md` FIRST**
2. Update `/doc/developer/canbus-protocol.md` with implementation examples
3. Update the shared component: `/ots-fw-shared/components/can_audiomodule/`
4. Update handlers in `src/can_audio_handler.c` if needed
5. Test changes in both audio module AND main controller firmware

## Related Projects

- **Main Controller Firmware:** `../ots-fw-main`
- **OpenFront Tactical Suitcase:** Parent project repository

---

**Last Updated:** January 4, 2026  
**Target Platform:** ESP32-A1S Audio Development Kit  
**Framework:** ESP-IDF v5.4.2 via PlatformIO  
**Architecture:** Modular design with hardware abstraction  
**Code Quality:** Refactored, single-responsibility modules
