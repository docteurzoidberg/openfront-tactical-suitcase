# Sound Module Prompt (CAN-controlled audio peripheral)

Use this prompt when implementing or modifying Sound Module integration in `ots-fw-main`.

## Implementation Status

**IMPLEMENTED** - Sound module is fully integrated with CAN protocol support:
- Source: `ots-fw-main/src/sound_module.c`
- Header: `ots-fw-main/include/sound_module.h`
- CAN Protocol: `ots-fw-main/include/can_protocol.h`
- CAN Driver: `ots-fw-shared/components/can_driver/` (shared component)

**Current Mode**: MOCK MODE (CAN messages logged, physical transmission pending TWAI driver configuration)

## What this module is

- A **separate ESP32-based board** with audio circuitry + SD card.
- The Sound Module has its **own firmware** and plays audio files from SD.
- The main controller (`ots-fw-main` on ESP32-S3) sends **CAN bus** requests to play sounds.
- The module includes:
  - 3Ω speaker
  - volume potentiometer (local)
  - hardware on/off switch (module can be totally absent from CAN)

Known hardware choice:
- **ESP32-A1S (AI Thinker ESP32 Audio Kit v2.2)**
- Firmware repo/subproject: `ots-fw-audiomodule`

## Architecture Overview

```
Game Event → Event Dispatcher → Sound Module → CAN Protocol Builder → CAN Driver → TWAI → Audio Module
                                        ↓
                                 Sound Index Mapper
```

## Key constraints

- Treat the Sound Module as **best-effort**: it may be powered off.
- The main controller should never block gameplay logic waiting for sound.
- Prefer a **single mapping table** (event → soundId) and keep it small.

## Integration Implementation

### Module Initialization (sound_init)
1. Initialize CAN driver (currently mock mode)
2. Check CAN driver readiness
3. Register with module manager
4. Set `initialized` flag

### Event Handling (sound_handle_event)
1. Filter for `GAME_EVENT_SOUND_PLAY` events
2. Parse event data JSON for:
   - `soundIndex` (required, uint16)
   - `interrupt` (optional, bool)
   - `priority` (optional, "high"|"normal")
3. Fallback: Map event type to sound index if not provided
4. Build CAN frame via `can_build_play_sound()`
5. Send via `can_driver_send()`
6. Track statistics (sounds_played, sounds_failed)

### Event Data Format
```json
{
  "soundId": "alert.atom",
  "soundIndex": 10,
  "interrupt": true,
  "priority": "high"
}
```

### Module Status (sound_get_status)
Reports:
- `initialized`: Module initialization complete
- `operational`: CAN driver ready
- `error_count`: Failed sound requests
- `last_error`: Error description or "OK"

## CAN Protocol Implementation (IMPLEMENTED)

**Current Status**: Protocol defined in `can_protocol.h`, CAN driver at `/ots-fw-shared/components/can_driver/`

**CAN Configuration**:
- CAN 2.0 classic, 500 kbps (configurable), standard 11-bit IDs
- Driver: ESP-IDF TWAI (Two-Wire Automotive Interface)
- Mode: MOCK (logs only) or PHYSICAL (when GPIO configured)

**Message IDs** (defined in `can_protocol.h`):
- `CAN_ID_PLAY_SOUND = 0x420` (main → audio module)
- `CAN_ID_STOP_SOUND = 0x421` (main → audio module)
- `CAN_ID_SOUND_STATUS = 0x422` (audio module → main)
- `CAN_ID_SOUND_ACK = 0x423` (audio module → main, optional)

**PLAY_SOUND Frame (`0x420`)** - 8 bytes:
```c
Byte 0: cmd = 0x01 (PLAY_SOUND)
Byte 1: flags (bit 0: interrupt, bit 1: high priority, bit 2: loop)
Bytes 2-3: soundIndex (uint16_t, little-endian)
Byte 4: volumeOverride (0xFF = ignore, 0-100 = set volume)
Bytes 5-6: requestId (uint16_t, for tracking)
Byte 7: reserved
```

**STOP_SOUND Frame (`0x421`)** - 8 bytes:
```c
Byte 0: cmd = 0x02 (STOP_SOUND)
Byte 1: flags (bit 0: stop all)
Bytes 2-3: soundIndex (0xFFFF = stop any/all)
Bytes 4-5: requestId (uint16_t)
Bytes 6-7: reserved
```

**Helper Functions** (in `can_protocol.c`):
- `can_build_play_sound()` - Constructs PLAY_SOUND CAN frame
- `can_build_stop_sound()` - Constructs STOP_SOUND CAN frame

**Usage Example**:
```c
can_frame_t frame;
can_build_play_sound(sound_index, flags, request_id, &frame);
esp_err_t ret = can_driver_send(&frame);
```

## Event Mapping Strategy (IMPLEMENTED)

**Centralized mapping** in `sound_module.c` via `map_event_to_sound_index()`:

```c
GAME_EVENT_GAME_START      → SOUND_INDEX_GAME_START (1)
GAME_EVENT_ALERT_NUKE      → SOUND_INDEX_ALERT_ATOM (10)
GAME_EVENT_ALERT_HYDRO     → SOUND_INDEX_ALERT_HYDRO (11)
GAME_EVENT_ALERT_MIRV      → SOUND_INDEX_ALERT_MIRV (12)
GAME_EVENT_ALERT_LAND      → SOUND_INDEX_ALERT_LAND (13)
GAME_EVENT_ALERT_NAVAL     → SOUND_INDEX_ALERT_NAVAL (14)
GAME_EVENT_NUKE_LAUNCHED   → SOUND_INDEX_NUKE_LAUNCH (20)
GAME_EVENT_NUKE_EXPLODED   → SOUND_INDEX_NUKE_EXPLODE (21)
GAME_EVENT_NUKE_INTERCEPTED→ SOUND_INDEX_NUKE_INTERCEPT (22)
GAME_EVENT_HARDWARE_TEST   → SOUND_INDEX_TEST_BEEP (50)
```

**Sound Index Catalog** (defined in `sound_module.h`):
- Indices correspond to files on audio module's SD card: `/sounds/0001.mp3`, `/sounds/0010.mp3`, etc.
- Range 1-9: System sounds (start, victory, defeat)
- Range 10-19: Alert sounds (incoming threats)
- Range 20-29: Nuke event sounds (launch, explode, intercept)
- Range 30-39: Game outcome sounds (victory, defeat)
- Range 50+: Test/utility sounds

**Primary Event**: `GAME_EVENT_SOUND_PLAY`
- Preferred method: Send explicit sound index in event data JSON
- Fallback: Use `map_event_to_sound_index()` if soundIndex not provided

## Emulator expectations (server)

In `ots-simulator` emulator mode:
- It should be able to show "Sound requested: X" in the UI.
- Actual audio playback in the browser is optional and can be added later.

## Failure Modes & Design Constraints

**Sound Module Best-Effort**:
- Module may be powered off or disconnected from CAN bus
- Main controller never blocks on sound playback
- CAN driver handles timeouts internally (no gameplay impact)
- Mock mode allows development without physical audio hardware

**Error Tracking**:
- Statistics: `sounds_played`, `sounds_failed`
- Logged but non-blocking errors
- Module status reports operational state

**CAN Driver Modes**:
1. **MOCK Mode** (current default):
   - Logs CAN frames to serial
   - No physical transmission
   - Always returns success
   - Useful for development/testing

2. **PHYSICAL Mode** (future):
   - Requires GPIO configuration (TX/RX pins)
   - ESP-IDF TWAI driver
   - Actual CAN bus communication
   - Enable via `can_driver_init()` config

## Next Steps / TODO

1. **Enable Physical CAN**:
   - Configure TWAI GPIO pins in `config.h`
   - Set `CONFIG_CAN_DRIVER_USE_TWAI=1` in sdkconfig
   - Test with actual ESP32-A1S audio module

2. **Audio Module Firmware** (`ots-fw-audiomodule`):
   - Implement CAN receive handler
   - SD card audio file management
   - Playback logic (queue, interrupt, priority)
   - Status/ACK frame transmission

3. **Protocol Refinements**:
   - Define STATUS frame format (0x422)
   - Define ACK frame format (0x423)
   - Implement status monitoring in main controller
   - Add heartbeat mechanism

4. **Sound Catalog Sync**:
   - Create definitive sound list in `/prompts/protocol-context.md`
   - Sync indices between firmware and audio module
   - Document SD card file naming convention

5. **Server Emulator**:
   - Add sound event visualization in dashboard
   - Optional: Browser audio playback for testing
   - Display CAN frame logs in UI

## References

- **Implementation**: `ots-fw-main/src/sound_module.c`, `ots-fw-main/include/sound_module.h`
- **CAN Protocol**: `ots-fw-main/include/can_protocol.h`, `ots-fw-main/src/can_protocol.c`
- **CAN Driver**: `/ots-fw-shared/components/can_driver/` (see `COMPONENT_PROMPT.md`)
- **Audio Firmware**: `ots-fw-audiomodule/` (separate project)
- **Protocol Spec**: `/prompts/protocol-context.md` (Sound Events section)
- **Hardware Module Interface**: `ots-fw-main/include/hardware_module.h`
