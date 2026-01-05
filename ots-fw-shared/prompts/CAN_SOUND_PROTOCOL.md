# CAN Sound Control Protocol

Protocol specification for sound control between `ots-fw-main` (main controller) and `ots-fw-audiomodule` (audio module).

## Prerequisites: Module Discovery

> **⚠️ IMPORTANT**: Before using this protocol, the main controller must first **discover** the audio module using the CAN discovery protocol.

The audio module automatically responds to discovery queries:
- **Module Type**: `MODULE_TYPE_AUDIO` (0x01)
- **CAN ID Block**: 0x42 (uses CAN IDs 0x420-0x42F)
- **Capabilities**: `MODULE_CAP_STATUS` (sends periodic status updates)

**Discovery Reference**: `/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`

If no audio module is discovered at boot, the main controller disables sound features and logs:
```
[sound_module] ✗ No audio module detected - sound features disabled
```

---

## Overview

The main controller (ots-fw-main) sends sound play/stop requests via CAN bus. The audio module acknowledges each request and provides a **queue ID** (handle) for tracking individual sounds. This allows:

- Multiple simultaneous sounds
- Individual sound control (stop by queue ID)
- Looping sounds that persist until explicitly stopped
- Per-sound volume control

---

## Message Format

All CAN messages use **8-byte data frames** (standard CAN DLC=8).

### CAN IDs

> **Note**: These CAN IDs are in the audio module's allocated block (0x420-0x42F).  
> The block allocation is communicated during discovery (see above).

| Direction | Message Type | CAN ID | Description |
|-----------|--------------|--------|-------------|
| Main → Audio | PLAY_SOUND_REQUEST | 0x420 | Request to play sound |
| Audio → Main | PLAY_SOUND_ACK | 0x423 | Acknowledgment with queue ID |
| Main → Audio | STOP_SOUND_REQUEST | 0x421 | Request to stop sound |
| Audio → Main | STOP_SOUND_ACK | 0x424 | Stop acknowledgment |
| Main → Audio | STOP_ALL_REQUEST | 0x422 | Stop all sounds |
| Audio → Main | STOP_ALL_ACK | (none) | Stop all acknowledgment |
| Audio → Main | SOUND_FINISHED | 0x425 | Sound completed (not looping) |

**Discovery IDs** (reserved, handled by discovery component):
- **0x410**: MODULE_ANNOUNCE (audio module → main controller)
- **0x411**: MODULE_QUERY (main controller → all modules)

---

## Message Structures

### 1. PLAY_SOUND_REQUEST (0x420)

**Direction**: Main controller → Audio module  
**Purpose**: Request to play a sound with options

```
Byte 0: Sound Index (0-255)
Byte 1: Flags
        bit 0: Loop (0=play once, 1=loop until stopped)
        bit 1: Interrupt (0=normal, 1=stop all and play)
        bits 2-7: Reserved (must be 0)
Byte 2: Volume (0-100, default: 100)
Byte 3: Priority (0=normal, 1-255=higher, reserved for future)
Bytes 4-7: Reserved (must be 0)
```

**Example - Play sound 1, no loop, 100% volume**:
```
CAN ID: 0x420
Data: [01 00 64 00 00 00 00 00]
       │  │  │  └─ Priority (0)
       │  │  └─ Volume (100)
       │  └─ Flags (0x00 = no loop, no interrupt)
       └─ Sound index (1)
```

**Example - Play sound 5, looping, 80% volume**:
```
CAN ID: 0x420
Data: [05 01 50 00 00 00 00 00]
       │  │  │
       │  │  └─ Volume (80)
       │  └─ Flags (0x01 = loop enabled)
       └─ Sound index (5)
```

---

### 2. PLAY_SOUND_ACK (0x111)

**Direction**: Audio module → Main controller  
**Purpose**: Acknowledge play request and provide queue ID

```
Byte 0: Sound Index (echoed from request)
Byte 1: Status
        0x00 = Success
        0x01 = Error (file not found)
        0x02 = Error (mixer full, no free slots)
        0x03 = Error (SD card error)
        0xFF = Unknown error
Byte 2: Queue ID (0-255, handle for this sound instance)
Byte 3: Reserved
Bytes 4-7: Reserved (must be 0)
```

**Example - Success, queue ID = 3**:
```
CAN ID: 0x423
Data: [01 00 03 00 00 00 00 00]
       │  │  │
       │  │  └─ Queue ID (3) - use this to stop sound later
       │  └─ Status (0x00 = success)
       └─ Sound index (1, echoed)
```

**Example - Error, file not found**:
```
CAN ID: 0x423
Data: [05 01 00 00 00 00 00 00]
       │  │  │
       │  │  └─ Queue ID (0 = invalid)
       │  └─ Status (0x01 = file not found)
       └─ Sound index (5)
```

---

### 3. STOP_SOUND_REQUEST (0x421)

**Direction**: Main controller → Audio module  
**Purpose**: Stop a specific sound by queue ID

```
Byte 0: Queue ID (from PLAY_SOUND_ACK)
Bytes 1-7: Reserved (must be 0)
```

**Example - Stop queue ID 3**:
```
CAN ID: 0x421
Data: [03 00 00 00 00 00 00 00]
       │
       └─ Queue ID (3)
```

---

### 4. STOP_SOUND_ACK (0x424)

**Direction**: Audio module → Main controller  
**Purpose**: Acknowledge stop request

```
Byte 0: Queue ID (echoed from request)
Byte 1: Status
        0x00 = Success (sound stopped)
        0x01 = Not found (queue ID invalid or already stopped)
Bytes 2-7: Reserved (must be 0)
```

**Example - Success**:
```
CAN ID: 0x424
Data: [03 00 00 00 00 00 00 00]
       │  │
       │  └─ Status (0x00 = stopped)
       └─ Queue ID (3)
```

---

### 5. STOP_ALL_REQUEST (0x422)

**Direction**: Main controller → Audio module  
**Purpose**: Stop all currently playing sounds

```
Bytes 0-7: Reserved (must be 0)
```

**Example**:
```
CAN ID: 0x422
Data: [00 00 00 00 00 00 00 00]
```

---

### 6. STOP_ALL_ACK (Not Currently Implemented)

**Direction**: Audio module → Main controller  
**Purpose**: Acknowledge stop all request (future enhancement)

```
Byte 0: Stopped count (number of sounds stopped)
Bytes 1-7: Reserved (must be 0)
```

**Note**: The current implementation does not send STOP_ALL_ACK. This is reserved for future protocol enhancement.

---

### 7. SOUND_FINISHED (0x425)

**Direction**: Audio module → Main controller  
**Purpose**: Notify that a sound has finished playing (not looping)

```
Byte 0: Queue ID (the sound that finished)
Byte 1: Sound Index (original sound index)
Byte 2: Reason
        0x00 = Completed normally
        0x01 = Stopped by user
        0x02 = Error during playback
Bytes 3-7: Reserved (must be 0)
```

**Example - Sound completed normally**:
```
CAN ID: 0x425
Data: [03 01 00 00 00 00 00 00]
       │  │  │
       │  │  └─ Reason (0x00 = completed)
       │  └─ Sound index (1)
       └─ Queue ID (3)
```

---

## Sound Index Mapping

Sound indices map to files on SD card (configured in `sound_config.c`):

| Index | Filename | Description | Default Volume | Loopable |
|-------|----------|-------------|----------------|----------|
| 0 | `game_start.wav` | Game start sound | 100% | No |
| 1 | `game_victory.wav` | Victory sound | 100% | No |
| 2 | `game_defeat.wav` | Defeat sound | 100% | No |
| 3 | `game_player_death.wav` | Player death | 100% | No |
| 4 | `alert_nuke.wav` | Nuclear alert | 80% | Yes |
| 5 | `alert_land.wav` | Land invasion | 80% | Yes |
| 6 | `alert_naval.wav` | Naval invasion | 80% | Yes |
| 7 | `nuke_launch.wav` | Nuke launch | 100% | No |

**Notes**:
- Indices 0-7 are reserved for game sounds
- Indices 8-255 available for custom sounds
- Default volume is recommendation, can be overridden in PLAY_SOUND_REQUEST
- Loopable flag indicates if sound makes sense to loop (alerts), but any sound can loop

---

## Usage Examples

### Example 1: Play One-Shot Sound

**Main Controller**:
```c
// Play victory sound (index 1), no loop, 100% volume
uint8_t can_data[8] = {1, 0x00, 100, 0, 0, 0, 0, 0};
can_send(0x110, can_data, 8);

// Wait for ACK...
// Response: queue_id = 3
```

**Expected Flow**:
```
Main → Audio: PLAY_SOUND_REQUEST [01 00 64 00 ...]
Audio → Main: PLAY_SOUND_ACK [01 00 03 00 ...] (queue_id=3)
... (1-2 seconds later)
Audio → Main: SOUND_FINISHED [03 01 00 00 ...] (completed)
```

---

### Example 2: Play Looping Alert, Stop Later

**Main Controller**:
```c
// Play nuclear alert (index 4), LOOP, 80% volume
uint8_t can_data[8] = {4, 0x01, 80, 0, 0, 0, 0, 0};
//                        │   └─ Loop flag (bit 0)
can_send(0x110, can_data, 8);

// Wait for ACK to get queue ID...
uint8_t queue_id = 5; // From ACK response

// ... (10 seconds later, threat cleared)

// Stop the looping alert
uint8_t stop_data[8] = {queue_id, 0, 0, 0, 0, 0, 0, 0};
can_send(0x112, stop_data, 8);

// Wait for stop ACK...
```

**Expected Flow**:
```
Main → Audio: PLAY_SOUND_REQUEST [04 01 50 00 ...] (loop=true)
Audio → Main: PLAY_SOUND_ACK [04 00 05 00 ...] (queue_id=5)
... (sound loops continuously)
Main → Audio: STOP_SOUND_REQUEST [05 00 00 ...]
Audio → Main: STOP_SOUND_ACK [05 00 00 ...] (stopped)
```

---

### Example 3: Play Multiple Sounds Simultaneously

**Main Controller**:
```c
// Play nuke launch sound
uint8_t data1[8] = {7, 0x00, 100, 0, 0, 0, 0, 0};
can_send(0x110, data1, 8);
// ACK: queue_id = 1

// Immediately play alert (while launch sound still playing)
uint8_t data2[8] = {4, 0x01, 80, 0, 0, 0, 0, 0};
can_send(0x110, data2, 8);
// ACK: queue_id = 2

// Both sounds play simultaneously (mixed)
```

---

### Example 4: Stop All Sounds (Panic Button)

**Main Controller**:
```c
// Stop everything immediately
uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
can_send(0x114, data, 8);

// Wait for ACK with count of stopped sounds
```

**Expected Flow**:
```
Main → Audio: STOP_ALL_REQUEST [00 00 00 ...]
Audio → Main: STOP_ALL_ACK [03 00 00 ...] (stopped 3 sounds)
```

---

## Queue ID Management

### Audio Module Behavior

- **Queue IDs** are assigned sequentially: 1, 2, 3, ..., 255, then wrap to 1
- **Queue ID 0** is reserved (invalid/error)
- **Maximum concurrent sounds**: 4 (MAX_AUDIO_SOURCES)
- When a sound finishes or is stopped, its queue ID becomes available for reuse
- Queue IDs are NOT reused until at least 10 other IDs have been assigned (prevent confusion)

### Main Controller Tracking

Main controller should maintain a mapping:
```c
typedef struct {
    uint8_t queue_id;
    uint8_t sound_index;
    bool is_looping;
    bool is_active;
} sound_handle_t;

sound_handle_t active_sounds[MAX_TRACKED_SOUNDS];
```

**Recommended**: Track active sounds and their queue IDs to enable:
- Stop specific sounds by game event (e.g., stop alert when threat neutralized)
- Avoid sending duplicate play requests
- Clean up on game state transitions

---

## Error Handling

### Audio Module Error Responses

| Status Code | Meaning | Action |
|-------------|---------|--------|
| 0x01 | File not found | Check sound index, verify SD card files |
| 0x02 | Mixer full | Wait for sound to finish, or stop one first |
| 0x03 | SD card error | Check SD card connection, reinitialize |
| 0xFF | Unknown error | Log error, retry once |

### Retry Strategy (Main Controller)

**File not found** (0x01):
- Do NOT retry (file is missing)
- Log error and fallback to alternative sound

**Mixer full** (0x02):
- Wait 500ms and retry
- Or stop oldest non-looping sound and retry

**SD card error** (0x03):
- Retry once after 1 second
- If still fails, disable sound for this session

---

## Implementation Notes

### Audio Module (`ots-fw-audiomodule`)

**Files to modify**:
- `src/can_handler.c` - Parse new CAN messages
- `src/can_protocol.h` - Define message structures
- `src/audio_mixer.c` - Add queue ID tracking to sources
- `src/sound_config.c` - Sound index → filename mapping

**Key changes**:
1. Add `queue_id` field to `audio_source` structure
2. Implement queue ID allocator (1-255, wraparound with reuse delay)
3. Add `audio_mixer_stop_by_queue_id(uint8_t queue_id)` function
4. Send ACK via CAN after mixer source creation
5. Send SOUND_FINISHED when non-looping sound completes

---

### Main Controller (`ots-fw-main`)

**Files to create/modify**:
- `src/sound_control.h/c` - Sound control API
- `src/modules/sound_module.c` - Integration with hardware modules

**Recommended API**:
```c
typedef void (*sound_ack_callback_t)(uint8_t sound_index, uint8_t queue_id, uint8_t status);

// Play sound with options
esp_err_t sound_play(uint8_t sound_index, bool loop, uint8_t volume, sound_ack_callback_t callback);

// Stop specific sound
esp_err_t sound_stop(uint8_t queue_id);

// Stop all sounds
esp_err_t sound_stop_all(void);

// Check if sound is playing
bool sound_is_playing(uint8_t queue_id);
```

---

## Testing Checklist

### Audio Module Tests

- [ ] Play single sound, verify ACK with queue ID
- [ ] Play sound with loop=true, verify it loops indefinitely
- [ ] Stop looping sound by queue ID, verify it stops
- [ ] Play 4 sounds simultaneously (fill mixer)
- [ ] Try to play 5th sound, verify mixer full error (0x02)
- [ ] Stop specific sound while others continue
- [ ] Stop all sounds, verify all stop
- [ ] Verify SOUND_FINISHED sent when non-looping sound completes
- [ ] Verify queue IDs don't repeat immediately (10+ gap)
- [ ] Test volume control (0%, 50%, 100%)

### Main Controller Tests

- [ ] Send play request, receive and parse ACK
- [ ] Track queue ID, use it to stop sound later
- [ ] Handle ACK errors (file not found, mixer full)
- [ ] Stop looping alert on game event
- [ ] Multiple simultaneous sounds (explosion + alert)
- [ ] Stop all sounds on game end

---

## Protocol Version

**Version**: 1.0  
**Date**: January 4, 2026  
**Status**: Draft (Implementation pending)

---

## Future Extensions

Possible future enhancements (not in v1.0):

- **Fade in/out**: Byte 4 of PLAY request = fade duration (0-255 × 10ms)
- **Spatial audio**: Byte 5 = pan/balance (-100 to +100)
- **Playlist**: Chain multiple sounds with single request
- **Volume query**: Get current master volume from audio module
- **Status query**: Get list of active queue IDs

---

## See Also

- `docs/SDCARD_AUDIO_SETUP.md` - SD card file setup guide
- `src/sound_config.c` - Sound index → filename mapping
- `src/can_handler.c` - CAN message handler implementation
- `/ots-fw-main/prompts/SOUND_MODULE_PROMPT.md` - Main controller sound module guide

