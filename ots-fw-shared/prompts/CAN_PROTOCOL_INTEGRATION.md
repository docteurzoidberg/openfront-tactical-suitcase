# CAN Protocol Integration Guide - Audio Module

This document describes how the **audio module** integrates with the OTS CAN bus architecture.

## Related Documentation

- **CAN Driver Component**: `/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md` - Generic hardware layer
- **CAN Discovery Component**: `/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md` - ✅ **Module discovery protocol**
- **Multi-Module Architecture**: `/ots-fw-shared/prompts/CAN_PROTOCOL_ARCHITECTURE.md` - Future extensible protocol
- **Audio Protocol Spec**: `CAN_SOUND_PROTOCOL.md` - Current audio-specific protocol
- **Main Controller Roadmap**: `/ots-fw-main/docs/CAN_MULTI_MODULE_ROADMAP.md` - Implementation phases

---

## Prerequisites: Module Discovery

> **⚠️ IMPORTANT**: Audio module must respond to discovery queries before it can receive sound commands.

### Discovery Integration (Audio Module Side)

The audio module automatically responds to `MODULE_QUERY` (0x411) messages:

```c
#include "can_discovery.h"

void can_rx_task(void *arg) {
    can_frame_t frame;
    while (1) {
        if (can_driver_receive(&frame, portMAX_DELAY) == ESP_OK) {
            // Handle discovery query
            if (frame.id == CAN_ID_MODULE_QUERY) {
                can_discovery_handle_query(&frame, 
                    MODULE_TYPE_AUDIO,      // Type: Audio module
                    1,                      // Firmware major: 1
                    0,                      // Firmware minor: 0
                    MODULE_CAP_STATUS,      // Capabilities: STATUS
                    0x42,                   // CAN block: 0x420-0x42F
                    0                       // Node ID (reserved)
                );
            }
            
            // Handle sound protocol messages...
            // (PLAY_SOUND, STOP_SOUND, etc.)
        }
    }
}
```

### Discovery Integration (Main Controller Side)

The main controller queries for modules at boot:

```c
#include "can_discovery.h"

void sound_init(void) {
    // Send discovery query
    ESP_LOGI(TAG, "Discovering CAN modules...");
    can_discovery_query_all();
    
    // Wait for responses
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Check if audio module was discovered (handled in CAN RX task)
    if (s_state.audio_module_discovered) {
        ESP_LOGI(TAG, "✓ Audio module v%d.%d detected", 
                 s_state.audio_module_version_major,
                 s_state.audio_module_version_minor);
        s_state.can_ready = true;
    } else {
        ESP_LOGW(TAG, "✗ No audio module detected - sound features disabled");
        s_state.can_ready = false;
    }
}
```

For complete discovery API reference, see:  
**`/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`**

---

## Current Implementation (Phase 1)

### Architecture Overview

```
┌─────────────────┐
│   ots-fw-main   │  Main Controller
│  (ESP32-S3)     │
└────────┬────────┘
         │ CAN Bus (500 kbps)
         │ Discovery: 0x410-0x411
         │ Audio: 0x420-0x42F
         │
┌────────┴────────┐
│ ots-fw-audio    │  Audio Module
│  (ESP32-A1S)    │
│  + Audio DAC    │
│  + SD Card      │
└─────────────────┘
```

### Protocol Layer Stack

```
┌─────────────────────────────────────┐
│  Application Layer                  │  can_handler.c
│  - Parse commands                   │  - Play/stop sounds
│  - Control mixer                    │  - Send ACKs
│  - Track queue IDs                  │
├─────────────────────────────────────┤
│  Discovery Layer                    │  can_discovery (shared)
│  - Boot-time module detection       │  - MODULE_QUERY/ANNOUNCE
│  - Module type/version tracking     │  - 0x410-0x411
├─────────────────────────────────────┤
│  Protocol Layer                     │  can_protocol.h/c
│  - Message encoding/decoding        │  - PLAY_SOUND_REQ
│  - CAN ID definitions               │  - SOUND_ACK
│  - Data structures                  │  - SOUND_FINISHED
├─────────────────────────────────────┤
│  Hardware Driver                    │  can_driver (shared component)
│  - TWAI peripheral                  │  - Auto-detection
│  - Frame TX/RX                      │  - Mock mode fallback
│  - Error handling                   │  - Statistics
└─────────────────────────────────────┘
```

---

## Audio-Specific Protocol (Current)

### CAN Message IDs

The audio module uses a **contiguous block of IDs** in the 0x420-0x42F range (allocated during discovery):

| CAN ID | Direction | Message Type | Description |
|--------|-----------|--------------|-------------|
| **0x420** | Main → Audio | PLAY_SOUND | Play sound with options (loop, volume) |
| **0x421** | Main → Audio | STOP_SOUND | Stop sound by queue ID |
| **0x422** | Main → Audio | STOP_ALL | Stop all sounds |
| **0x423** | Audio → Main | SOUND_ACK | Play acknowledgment with queue ID |
| **0x424** | Audio → Main | STOP_ACK | Stop acknowledgment |
| **0x425** | Audio → Main | SOUND_FINISHED | Sound playback finished |
| 0x426-0x42F | - | Reserved | Future audio features |

**Discovery IDs** (handled by discovery component):
| **0x410** | Module → Main | MODULE_ANNOUNCE | Module presence + capabilities |
| **0x411** | Main → Module | MODULE_QUERY | Discovery broadcast |

**Why this range?**
- Chosen to avoid conflicts with future generic protocol
- Allows audio module to own a dedicated block
- Easy to identify audio traffic in bus analyzer
- Block allocation communicated during discovery

---

## Message Format Details

### PLAY_SOUND (0x420)

**Direction**: Main → Audio  
**Purpose**: Request to play a sound with options

```
Byte 0-1: Sound index (uint16_t, little-endian)
Byte 2:   Flags
          bit 0: Interrupt (stop all and play this)
          bit 1: High priority (reserved)
          bit 2: Loop (play until stopped)
          bits 3-7: Reserved
Byte 3:   Volume (0-100, or 0xFF = use pot)
Byte 4-5: Request ID (uint16_t, little-endian)
Byte 6-7: Reserved
```

**Example - Play sound 5, looping, 80% volume:**
```c
can_frame_t frame;
frame.id = 0x420;
frame.dlc = 8;
frame.data[0] = 5;      // Sound index low byte
frame.data[1] = 0;      // Sound index high byte
frame.data[2] = 0x04;   // Flags: loop bit set
frame.data[3] = 80;     // Volume 80%
frame.data[4] = 0x01;   // Request ID (low)
frame.data[5] = 0x00;   // Request ID (high)
frame.data[6] = 0;
frame.data[7] = 0;
```

---

### SOUND_ACK (0x423)

**Direction**: Audio → Main  
**Purpose**: Acknowledge play request, provide queue ID

```
Byte 0:   OK flag (1=success, 0=error)
Byte 1-2: Sound index (echoed from request)
Byte 3:   Queue ID (1-255, or 0=invalid)
Byte 4:   Error code (if OK=0)
          0x00 = No error
          0x01 = File not found
          0x02 = SD card error
          0x03 = Mixer full
          0x04 = Invalid index
          0x05 = Mixer full (no free slots)
Byte 5-6: Request ID (echoed from request)
Byte 7:   Reserved
```

**Example - Success with queue ID 7:**
```c
can_frame_t ack;
ack.id = 0x423;
ack.dlc = 8;
ack.data[0] = 1;      // OK
ack.data[1] = 5;      // Sound index (echo)
ack.data[2] = 0;
ack.data[3] = 7;      // Queue ID assigned
ack.data[4] = 0;      // No error
ack.data[5] = 0x01;   // Request ID (echo)
ack.data[6] = 0x00;
ack.data[7] = 0;
```

---

### STOP_SOUND (0x421)

**Direction**: Main → Audio  
**Purpose**: Stop a specific sound by queue ID

```
Byte 0-1: Queue ID (or 0xFFFF for all/current)
Byte 2:   Flags
          bit 0: Stop all (deprecated, use STOP_ALL instead)
          bits 1-7: Reserved
Byte 3-5: Request ID (uint16_t)
Byte 6-7: Reserved
```

---

### STOP_ALL (0x424)

**Direction**: Main → Audio  
**Purpose**: Stop all currently playing sounds

```
Bytes 0-7: Reserved (all zeros)
```

**Response**: SOUND_STATUS message will reflect 0 active sources

---

### SOUND_FINISHED (0x425)

**Direction**: Audio → Main  
**Purpose**: Notify that a non-looping sound has completed

```
Byte 0:   Queue ID (the sound that finished)
Byte 1-2: Sound index (original)
Byte 3:   Reason
          0x00 = Completed normally
          0x01 = Stopped by user
          0x02 = Error during playback
Bytes 4-7: Reserved
```

**Note**: Only sent for non-looping sounds. Looping sounds never send FINISHED (must be explicitly stopped).

---

### SOUND_STATUS (0x422)

**Direction**: Audio → Main  
**Purpose**: Periodic status broadcast (every 5 seconds)

```
Byte 0:   State bits
          bit 0: Ready (module initialized)
          bit 1: SD mounted
          bit 2: Playing (at least one active source)
          bit 3: Muted (volume pot at 0)
          bit 4: Error state
Byte 1-2: Current sound index (or 0xFFFF if none)
Byte 3:   Error code (last error)
Byte 4:   Volume (0-100)
Byte 5-6: Uptime (seconds, wraps at 65535)
Byte 7:   Active source count (0-4)
```

---

## Queue ID Management

### Audio Module Behavior

**Queue ID Allocator**:
```c
static uint8_t g_next_queue_id = 1;  // Starts at 1, wraps to 1 (never 0)

uint8_t allocate_queue_id(void) {
    uint8_t id = g_next_queue_id++;
    if (g_next_queue_id == 0) {
        g_next_queue_id = 1;  // Skip 0 (reserved for errors)
    }
    return id;
}
```

**Key Rules**:
- Queue ID 0 = **invalid** (error indicator in ACK)
- Queue IDs 1-255 are valid handles
- IDs wrap around after 255 → back to 1
- Each active sound has a unique queue ID
- When sound stops/finishes, ID can be reused (after 10+ other IDs assigned)

**Example Flow**:
```
PLAY sound 1 → Queue ID 5 assigned
PLAY sound 2 → Queue ID 6 assigned  
PLAY sound 3 → Queue ID 7 assigned
STOP queue 6 → Sound 2 stops
PLAY sound 4 → Queue ID 8 assigned (not reusing 6 yet)
```

---

## Integration with Mixer

### Audio Source Extensions

The `audio_mixer` module tracks queue IDs:

```c
typedef struct {
    bool active;
    audio_source_state_t state;
    char filepath[128];
    uint8_t volume;
    bool loop;
    uint8_t queue_id;        // NEW: CAN queue ID (0=not CAN-triggered)
    uint16_t sound_index;    // NEW: Original sound index
    
    // ... rest of fields ...
} audio_source_t;
```

**New mixer functions**:
```c
// Stop by queue ID
esp_err_t audio_mixer_stop_by_queue_id(uint8_t queue_id);

// Get source info by queue ID
audio_source_t* audio_mixer_get_by_queue_id(uint8_t queue_id);

// Assign queue ID to new source
void audio_mixer_set_queue_id(audio_source_handle_t handle, uint8_t queue_id, uint16_t sound_index);
```

---

## Error Handling Strategy

### Main Controller Retry Logic

| Error Code | Error | Retry Strategy |
|------------|-------|----------------|
| 0x01 | File not found | **Don't retry** - file is missing, log error |
| 0x02 | SD card error | **Retry once** after 1 second |
| 0x03 | Mixer full | **Wait 500ms and retry**, or stop oldest sound first |
| 0x04 | Invalid index | **Don't retry** - programming error |

### Audio Module Response Times

- **ACK latency**: < 50ms typical
- **Play latency**: < 100ms (WAV header parse + mixer source creation)
- **Stop latency**: < 10ms (immediate)

### Timeout Handling

**Main controller should timeout if no ACK received within 200ms**:
```c
// Pseudo-code
send_play_request(sound_index, flags, volume);
if (!wait_for_ack(200ms)) {
    ESP_LOGE(TAG, "Audio module timeout - no ACK");
    // Retry or fail gracefully
}
```

---

## Future Multi-Module Migration (Phase 2+)

### Compatibility Plan

The current audio protocol (0x420-0x42F) will remain **unchanged** when migrating to the generic multi-module protocol. Future enhancements:

**Phase 2: Discovery Integration**
- Audio module will respond to generic discovery (0x61F)
- Registers as "Module Type 0x01 = Audio Module"
- Keeps 0x420-0x42F protocol as-is

**Phase 3: Priority-Based IDs**
- Audio messages could migrate to priority-based IDs:
  - Emergency sounds: Priority 0 (0x0XX)
  - Game events: Priority 1 (0x1XX)
  - UI feedback: Priority 2-4 (0x2XX-0x4XX)
- **Backward compatibility**: Old IDs remain functional

**Phase 4: Generic Commands**
- Audio module could support generic module commands:
  - 0x600 = MODULE_RESET
  - 0x610 = MODULE_STATUS_REQ
  - 0x620 = MODULE_CONFIG_SET
- Audio-specific commands (0x420-0x42F) continue to work

---

## Testing & Validation

### CAN Bus Analyzer Capture

**Expected traffic during game**:
```
Time     ID    DLC  Data                              Description
----------------------------------------------------------------------
0.000    0x420  8   [05 00 04 50 01 00 00 00]       PLAY sound 5, loop, 80%
0.035    0x423  8   [01 05 00 07 00 01 00 00]       ACK: queue_id=7
5.000    0x422  8   [07 05 00 00 50 05 00 01]       STATUS: playing, vol=80
10.000   0x421  8   [07 00 00 00 02 00 00 00]       STOP queue_id=7
10.012   0x422  8   [03 FF FF 00 50 0A 00 00]       STATUS: not playing
```

### Unit Tests

**Mock CAN driver tests**:
```c
// Test play → ACK → finished sequence
void test_play_oneshot_sound(void) {
    can_frame_t play_req = build_play_request(1, 0, 100, 1);
    can_handler_process_frame(&play_req);
    
    can_frame_t ack = get_last_sent_frame();
    assert(ack.id == 0x423);
    assert(ack.data[0] == 1);  // OK
    uint8_t queue_id = ack.data[3];
    
    // Wait for sound to finish
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    can_frame_t finished = get_last_sent_frame();
    assert(finished.id == 0x425);
    assert(finished.data[0] == queue_id);
    assert(finished.data[3] == 0);  // Completed normally
}
```

---

## Implementation Checklist

### Audio Module (ots-fw-audiomodule)

- [x] CAN protocol header with queue ID support
- [x] Protocol documentation (CAN_SOUND_PROTOCOL.md)
- [ ] Update audio_mixer to track queue IDs
- [ ] Implement `audio_mixer_stop_by_queue_id()`
- [ ] Update can_handler to use new ACK format with queue ID
- [ ] Send SOUND_FINISHED when non-looping sounds complete
- [ ] Add queue ID allocator with wraparound
- [ ] Test: Play multiple sounds, verify unique queue IDs
- [ ] Test: Stop by queue ID while others continue
- [ ] Test: Loop sound, stop later by queue ID

### Main Controller (ots-fw-main)

- [ ] Create sound_control.h/c API layer
- [ ] Track active sounds and their queue IDs
- [ ] Implement retry logic for mixer-full errors
- [ ] Add timeout handling for ACKs (200ms)
- [ ] Parse SOUND_FINISHED notifications
- [ ] Clean up finished sounds from tracking table
- [ ] Test: Game event → sound play → verify ACK
- [ ] Test: Alert loop → stop on event → verify stopped
- [ ] Test: Multiple simultaneous sounds

---

## Performance Considerations

### CAN Bus Bandwidth

**Maximum throughput** at 500 kbps with 8-byte frames:
- Theoretical: ~5000 frames/second
- Practical: ~1000-2000 frames/second (with arbitration, ACKs, error handling)

**Audio module traffic**:
- PLAY requests: ~10-50/second (peak during intense gameplay)
- ACKs: Same as requests
- STATUS: 1 frame every 5 seconds
- FINISHED: ~10-50/second (peak)

**Total bandwidth usage**: < 5% of bus capacity (plenty of headroom)

### Latency Requirements

| Operation | Target Latency | Acceptable Maximum |
|-----------|----------------|---------------------|
| Play sound ACK | < 50ms | 200ms (timeout) |
| Stop sound | < 10ms | 50ms |
| Status broadcast | N/A (periodic) | - |

**Audio playback latency** (separate from CAN):
- WAV header parse: 5-10ms
- Mixer source creation: 10-20ms
- First audio output: 50-100ms
- **Total**: ~100ms from CAN receive to audio output

---

## Debugging Tips

### Enable CAN Driver Logging

```c
esp_log_level_set("CAN_DRV", ESP_LOG_DEBUG);
esp_log_level_set("CAN_HANDLER", ESP_LOG_DEBUG);
```

### Monitor CAN Traffic

```bash
# Use pio device monitor
pio device monitor --raw

# Look for:
# [CAN_HANDLER] CAN RX: ID=0x420 DLC=8
# [CAN_HANDLER] PLAY_SOUND: index=5 flags=0x04 vol=80
# [CAN_HANDLER] Sent ACK: queue_id=7
```

### Common Issues

**No ACK received**:
- Check CAN transceiver wiring (TX/RX pins)
- Verify both modules at same bitrate (500 kbps)
- Check if audio module initialized (SD card mounted)

**Wrong queue ID**:
- Check queue ID allocator isn't resetting unexpectedly
- Verify ACK parsing extracts byte 3 correctly

**Sound doesn't play**:
- ACK.OK flag = 0? Check error code
- File not found? Verify SD card files
- Mixer full? Check active source count

---

## References

- **ESP32 TWAI Controller**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html
- **CAN 2.0B Specification**: Bosch CAN Specification Version 2.0 (1991)
- **Audio Mixer Implementation**: `src/audio_mixer.c`
- **Sound Config Registry**: `src/sound_config.c`

---

**Last Updated**: January 4, 2026  
**Protocol Version**: 1.0 (Audio-specific, Phase 1)  
**Status**: Implementation in progress
