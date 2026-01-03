# Sound Module Implementation - Main Firmware Side

## Summary

Implemented the sound module on the main firmware side (ots-fw-main) with mocked CAN bus communication. The module follows the established hardware module pattern and integrates with the event dispatcher.

## Files Created

### Headers
- **`include/can_driver.h`** - CAN bus driver interface with protocol definitions
  - Frame structures and message IDs (0x420-0x423)
  - Helper functions for building PLAY_SOUND and STOP_SOUND frames
  - Mock implementation documented (future: ESP-IDF TWAI driver)

- **`include/sound_module.h`** - Sound module interface
  - `hardware_module_t` implementation
  - Sound catalog enum (sound indices 1-100)
  - Public API: `sound_module_play()`, `sound_module_stop()`

### Source Files
- **`src/can_driver.c`** - Mock CAN driver implementation
  - Logs all CAN frames to serial output
  - Decodes and displays PLAY_SOUND and STOP_SOUND messages
  - Helper functions for frame construction

- **`src/sound_module.c`** - Sound module implementation
  - Handles SOUND_PLAY events from event dispatcher
  - Parses JSON event data for soundIndex, interrupt, priority
  - Sends CAN messages via mock driver
  - Tracks statistics (sounds played, failed)
  - Fallback sound mapping from game event types

### Documentation
- **`docs/CAN_DRIVER_FUTURE.md`** - Migration plan for shared CAN component
  - Extraction to `components/can_driver/`
  - Physical TWAI implementation plan
  - Pin assignments for both firmwares
  - Testing strategy

## Integration

### CMakeLists.txt
Added to production source list:
- `sound_module.c`
- `can_driver.c`

### main.c
- Added `#include "sound_module.h"`
- Registered module: `module_manager_register(sound_module_get())`
- Module initialized independently of I/O expanders (uses CAN, not MCP23017)

## Features

### Mock Mode (Current)
- ✅ CAN messages logged to serial output with decoded parameters
- ✅ Sound module handles SOUND_PLAY events
- ✅ JSON parsing for soundIndex, interrupt, priority flags
- ✅ Fallback event-to-sound-index mapping
- ✅ Statistics tracking (sounds played/failed)
- ✅ Module status reporting via diagnostics

### Physical CAN (Future)
- ⏳ ESP-IDF TWAI driver integration (500kbps)
- ⏳ Hardware CAN transceiver support (SN65HVD230/MCP2551)
- ⏳ Receive CAN STATUS/ACK messages from audio module
- ⏳ Shared component extraction for ots-fw-audiomodule

## Protocol

### CAN Message IDs
- `0x420` PLAY_SOUND (main → audio)
- `0x421` STOP_SOUND (main → audio)
- `0x422` SOUND_STATUS (audio → main)
- `0x423` SOUND_ACK (audio → main)

### PLAY_SOUND Frame (8 bytes)
```
Byte 0: cmd = 0x01
Byte 1: flags (interrupt, high_priority, loop)
Byte 2-3: soundIndex (u16, little-endian)
Byte 4: volumeOverride (0xFF = use pot)
Byte 5: reserved
Byte 6-7: requestId (u16, little-endian)
```

### Sound Catalog (Preliminary)
```c
SOUND_INDEX_GAME_START = 1
SOUND_INDEX_ALERT_ATOM = 10
SOUND_INDEX_ALERT_HYDRO = 11
SOUND_INDEX_ALERT_MIRV = 12
SOUND_INDEX_ALERT_LAND = 13
SOUND_INDEX_ALERT_NAVAL = 14
SOUND_INDEX_NUKE_LAUNCH = 20
SOUND_INDEX_NUKE_EXPLODE = 21
SOUND_INDEX_NUKE_INTERCEPT = 22
SOUND_INDEX_VICTORY = 30
SOUND_INDEX_DEFEAT = 31
SOUND_INDEX_TEST_BEEP = 100
```

Maps to SD card files on audio module: `/sounds/0001.mp3`, etc.

## Testing

### Serial Output Example
```
[SOUND_MODULE] Initializing sound module...
[CAN_DRIVER] CAN driver initialized (MOCK MODE)
[CAN_DRIVER] Physical CAN bus not implemented yet - messages will be logged only
[SOUND_MODULE] Sound module initialized successfully
[SOUND_MODULE] Received SOUND_PLAY event
[SOUND_MODULE] Sent PLAY_SOUND: index=10 interrupt=0 priority=1 reqID=1
[CAN_DRIVER] CAN TX: ID=0x420 DLC=8 DATA=[01 02 0A 00 FF 00 01 00]
[CAN_DRIVER]   → PLAY_SOUND: index=10 flags=0x02 volume=255 reqID=1
[CAN_DRIVER]      [HIGH_PRIORITY]
```

### Event Flow
1. Userscript sends SOUND_PLAY event via WebSocket
2. Event dispatcher routes to sound module
3. Sound module parses event data
4. CAN frame constructed and sent
5. Mock driver logs frame details

## Next Steps

### For Audio Firmware (ots-fw-audiomodule)
1. Implement CAN receive handler for PLAY_SOUND (0x420)
2. Add SD card audio playback on command
3. Send STATUS heartbeat messages (0x422) at 1 Hz
4. Send ACK messages (0x423) on successful playback

### For Main Firmware (ots-fw-main)
1. Complete sound catalog in protocol-context.md
2. Add CAN receive handler for STATUS/ACK messages
3. Display audio module status in diagnostics
4. Implement sound queue/priority system

### For Shared Component
1. Extract to `components/can_driver/`
2. Implement physical TWAI driver
3. Add compile-time mock/physical mode selection
4. Document pin assignments and hardware setup

## References

- Sound Module Spec: `/ots-hardware/modules/sound-module.md`
- Audio Firmware: `/ots-fw-audiomodule/`
- Protocol Context: `/prompts/protocol-context.md`
- CAN Migration Plan: `/ots-fw-main/docs/CAN_DRIVER_FUTURE.md`
