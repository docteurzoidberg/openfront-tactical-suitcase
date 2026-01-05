# CAN Bus Protocol Specification

**Version**: 1.0  
**Date**: January 5, 2026  
**Status**: Implemented (Audio Module + Discovery)

## Purpose

This document defines the CAN bus message protocol for the OpenFront Tactical Suitcase (OTS) hardware system. It serves as the single source of truth for CAN message formats, IDs, and data structures used across all CAN-enabled modules.

---

## Transport Layer

### Physical Layer

- **Protocol**: CAN 2.0B (Controller Area Network)
- **Bitrate**: 500 kbps (configurable)
- **Frame Format**: Standard 11-bit ID (extended 29-bit supported)
- **Data Length**: 0-8 bytes per frame
- **Bus Topology**: Multi-drop, two-wire differential (CANH/CANL)
- **Termination**: 120Ω resistors at each bus end

### Hardware

**ESP32 TWAI Controller**:
- Integrated CAN 2.0B controller (TWAI peripheral)
- Auto-detection with mock mode fallback
- 500 kbps @ 80MHz APB clock

**External Transceiver** 
- TJA1050 (5V, high-speed) or SN65HVD230 (3.3V, recommended)
- MCP2551 (5V, alternative)

**GPIO Pins** (board-specific):

*Main Controller (ESP32-S3):*
- TX: GPIO 5 (configurable)
- RX: GPIO 4 (configurable)

*Audio Module (ESP32-A1S AudioKit):*
- TX: GPIO 18 (expansion header, bottom row position 5)
- RX: GPIO 19 (expansion header, bottom row position 3)
- Alternative: See `ots-fw-audiomodule/ESP32_A1S_AUDIOKIT_BOARD.md` for other pin options

**Note**: Pin assignments are configurable in each firmware's `board_config.h` or equivalent configuration file.

### Frame Structure

```
┌──────────┬────────────┬─────┬─────────┬───────┬────────────┬─────┐
│   SOF    │  CAN ID    │ RTR │   DLC   │ DATA  │    CRC     │ ACK │
│  (1 bit) │ (11 bits)  │(1b) │ (4 bits)│(0-8B) │ (15 bits)  │(2b) │
└──────────┴────────────┴─────┴─────────┴───────┴────────────┴─────┘

Standard Frame (used by OTS):
- SOF: Start of Frame (dominant bit)
- CAN ID: 11-bit identifier (0x000-0x7FF)
- RTR: Remote Transmission Request
- DLC: Data Length Code (0-8)
- DATA: Payload bytes
- CRC: 15-bit cyclic redundancy check
- ACK: Acknowledgment slot
```

### Bus Arbitration

- **Priority**: Lower CAN ID = higher priority
- **Non-destructive arbitration**: Dominant bits (0) win over recessive bits (1)
- **Example**: ID 0x410 has priority over ID 0x420

---

## CAN ID Allocation

### Current Allocation (v1.0)

| CAN ID Range | Module | Usage | Status |
|--------------|--------|-------|--------|
| **0x410-0x411** | Discovery | Module enumeration | ✅ Implemented |
| **0x420-0x42F** | Audio Module | Sound control | ✅ Implemented |
| **0x430-0x43F** | Reserved | Future any module | 📋 Planned |
| **0x440-0x44F** | Reserved | Future any module | 📋 Planned |
| **0x450-0x45F** | Reserved | Future any module | 📋 Planned |

### Reserved Ranges

- **0x000-0x0FF**: System messages (emergency, broadcast)
- **0x100-0x3FF**: Reserved for future allocation
- **0x400-0x40F**: Discovery protocol (global)
- **0x410-0x7FF**: Module-specific blocks (16 IDs per module)

### Future ID Structure (Multi-Module Architecture)

**11-bit ID Format**: `[PPP][TTTT][AAAA]`

```
Bits 10-8 (PPP): Priority Level
Bits 7-4 (TTTT): Message Type  
Bits 3-0 (AAAA): Module Address

Priority Levels:
  0 = Emergency (highest)
  1 = High (real-time game events)
  2-4 = Normal (commands, status)
  5-7 = Low (diagnostics, bulk data)

Message Types:
  0x0 = Broadcast
  0x1 = Discovery
  0x2 = Command
  0x3 = Response
  0x4 = Status
  0x5 = Event
  0x6 = Configuration
  0x7 = Diagnostics
```

**Note**: Current implementation (v1.0) uses fixed CAN IDs per module. Future versions may migrate to structured addressing.

---

## Discovery Protocol

### Overview

Boot-time module detection protocol:
1. Main controller broadcasts `MODULE_QUERY` on startup
2. Each module responds with `MODULE_ANNOUNCE` (type, version, CAN block)
3. Main controller waits 500ms for all responses
4. No runtime heartbeat (keeps bus traffic low)

### CAN IDs

| CAN ID | Direction | Message | Description |
|--------|-----------|---------|-------------|
| **0x410** | Module → Main | MODULE_ANNOUNCE | Module identification response |
| **0x411** | Main → Module | MODULE_QUERY | Broadcast query for modules |

### MODULE_QUERY (0x411)

**Direction**: Main controller → All modules (broadcast)  
**Purpose**: Request all modules to identify themselves  
**DLC**: 8 bytes

```
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ 0xFF│ 0x00│ 0x00│ 0x00│ 0x00│ 0x00│ 0x00│ 0x00│
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
  │
  └─ Magic byte: 0xFF = enumerate all modules

Example:
CAN ID: 0x411
Data: [FF 00 00 00 00 00 00 00]
```

### MODULE_ANNOUNCE (0x410)

**Direction**: Module → Main controller  
**Purpose**: Module identifies itself with type, version, capabilities  
**DLC**: 8 bytes

```
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│Type │Ver  │Ver  │Caps │CAN  │Node │   Reserved  │
│     │Major│Minor│     │Block│ ID  │             │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘

Byte 0: Module Type
Byte 1: Firmware Major Version
Byte 2: Firmware Minor Version
Byte 3: Capabilities (bitfield)
Byte 4: CAN Block Base (high nibble of allocated IDs)
Byte 5: Node ID (0x00 = primary/single module)
Bytes 6-7: Reserved (must be 0x00)

Example - Audio Module v1.0:
CAN ID: 0x410
Data: [01 01 00 01 42 00 00 00]
       │  │  │  │  │  └─ Node ID: 0 (single module)
       │  │  │  │  └─ CAN block: 0x42 (uses 0x420-0x42F)
       │  │  │  └─ Capabilities: 0x01 (STATUS)
       │  │  └─ Firmware minor: 0
       │  └─ Firmware major: 1
       └─ Module type: 0x01 (AUDIO)
```

### Module Types

```c
#define MODULE_TYPE_NONE        0x00  // Reserved
#define MODULE_TYPE_AUDIO       0x01  // ✅ Audio playback module
// 0x02-0x7F: Future module types
// 0x80-0xFF: Custom/experimental modules
```

### Capability Flags

```c
#define MODULE_CAP_STATUS       (1 << 0)  // 0x01 - Sends periodic status
#define MODULE_CAP_OTA          (1 << 1)  // 0x02 - Supports firmware updates
#define MODULE_CAP_BATTERY      (1 << 2)  // 0x04 - Battery powered
// bits 3-7: Reserved for future capabilities
```

### Discovery Flow

```
Main Controller                          Audio Module
     │                                        │
     ├──────── MODULE_QUERY (0x411) ────────→│
     │         [FF 00 00 00 00 00 00 00]     │
     │                                        │
     │                                   [Detect query]
     │                                   [Prepare response]
     │                                        │
     │←─────── MODULE_ANNOUNCE (0x410) ──────┤
     │         [01 01 00 01 42 00 00 00]     │
     │         (AUDIO v1.0, block 0x42)      │
     │                                        │
[Wait 500ms for other modules]               │
[Build module registry]                      │
[Enable audio features]                      │
     │                                        │
     ▼                                        ▼
   Ready                                   Ready
```

---

## Audio Module Protocol

### Overview

Protocol for sound control between main controller and audio module:
- Play sounds with options (loop, volume, priority)
- Queue ID tracking for multiple simultaneous sounds
- Individual sound control (stop by queue ID)
- Status reporting (ACK/NACK with error codes)

### Prerequisites

Before using audio protocol:
1. Audio module must be discovered via MODULE_ANNOUNCE
2. Module type must be `MODULE_TYPE_AUDIO` (0x01)
3. CAN block must be allocated (typically 0x42 = 0x420-0x42F)

If no audio module discovered at boot:
- Main controller disables sound features
- Sound commands are not sent

### CAN IDs

| CAN ID | Direction | Message | Description |
|--------|-----------|---------|-------------|
| **0x420** | Main → Audio | PLAY_SOUND_REQUEST | Request to play sound |
| **0x423** | Audio → Main | PLAY_SOUND_ACK | Acknowledgment with queue ID |
| **0x421** | Main → Audio | STOP_SOUND_REQUEST | Stop specific sound by queue ID |
| **0x424** | Audio → Main | STOP_SOUND_ACK | Stop acknowledgment |
| **0x422** | Main → Audio | STOP_ALL_REQUEST | Stop all sounds |
| **0x425** | Audio → Main | SOUND_FINISHED | Sound completed (not looping) |

**Note**: STOP_ALL_ACK is reserved for future implementation (no CAN ID assigned yet).

### PLAY_SOUND_REQUEST (0x420)

**Direction**: Main controller → Audio module  
**Purpose**: Request to play a sound with options  
**DLC**: 8 bytes

```
┌─────┬─────┬─────┬─────┬─────────────────────────┐
│Index│Flags│Vol  │Prior│      Reserved           │
└─────┴─────┴─────┴─────┴─────────────────────────┘

Byte 0: Sound Index (0-255)
Byte 1: Flags (bitfield)
        bit 0: Loop (0=play once, 1=loop until stopped)
        bit 1: Interrupt (0=normal, 1=stop all and play)
        bits 2-7: Reserved (must be 0)
Byte 2: Volume (0-100, percent)
Byte 3: Priority (0=normal, 1-255=higher, reserved)
Bytes 4-7: Reserved (must be 0x00)

Example - Play sound 1, no loop, 100% volume:
CAN ID: 0x420
Data: [01 00 64 00 00 00 00 00]
       │  │  │  └─ Priority: 0 (normal)
       │  │  └─ Volume: 100 (100%)
       │  └─ Flags: 0x00 (no loop, no interrupt)
       └─ Sound index: 1

Example - Play sound 5, looping, 80% volume:
CAN ID: 0x420
Data: [05 01 50 00 00 00 00 00]
       │  │  │
       │  │  └─ Volume: 80 (80%)
       │  └─ Flags: 0x01 (loop enabled)
       └─ Sound index: 5
```

**Flag Details**:
- **Loop (bit 0)**: If set, sound plays continuously until stopped via STOP_SOUND_REQUEST
- **Interrupt (bit 1)**: If set, stops all currently playing sounds before starting this one

### PLAY_SOUND_ACK (0x423)

**Direction**: Audio module → Main controller  
**Purpose**: Acknowledge play request and provide queue ID  
**DLC**: 8 bytes

```
┌─────┬─────┬─────┬─────┬─────────────────────────┐
│Index│Stat │Queue│Rsvd │      Reserved           │
└─────┴─────┴─────┴─────┴─────────────────────────┘

Byte 0: Sound Index (echoed from request)
Byte 1: Status Code
        0x00 = Success
        0x01 = Error: File not found
        0x02 = Error: Mixer full (no free slots)
        0x03 = Error: SD card error
        0xFF = Unknown error
Byte 2: Queue ID (1-255, handle for this sound instance)
        0x00 = Invalid (only on error)
Byte 3: Reserved
Bytes 4-7: Reserved (must be 0x00)

Example - Success, queue ID = 3:
CAN ID: 0x423
Data: [01 00 03 00 00 00 00 00]
       │  │  │
       │  │  └─ Queue ID: 3 (use to stop sound later)
       │  └─ Status: 0x00 (success)
       └─ Sound index: 1 (echoed)

Example - Error, file not found:
CAN ID: 0x423
Data: [05 01 00 00 00 00 00 00]
       │  │  │
       │  │  └─ Queue ID: 0 (invalid on error)
       │  └─ Status: 0x01 (file not found)
       └─ Sound index: 5
```

**Status Codes**:
- **0x00 (Success)**: Sound loaded into mixer, playing/queued
- **0x01 (File Not Found)**: Sound index doesn't map to valid file
- **0x02 (Mixer Full)**: All 4 mixer slots occupied (wait or stop one)
- **0x03 (SD Card Error)**: SD card read failure or not mounted
- **0xFF (Unknown Error)**: Unspecified error

### STOP_SOUND_REQUEST (0x421)

**Direction**: Main controller → Audio module  
**Purpose**: Stop a specific sound by queue ID  
**DLC**: 8 bytes

```
┌─────┬─────────────────────────────────────────┐
│Queue│              Reserved                   │
└─────┴─────────────────────────────────────────┘

Byte 0: Queue ID (from PLAY_SOUND_ACK)
Bytes 1-7: Reserved (must be 0x00)

Example - Stop queue ID 3:
CAN ID: 0x421
Data: [03 00 00 00 00 00 00 00]
       │
       └─ Queue ID: 3
```

### STOP_SOUND_ACK (0x424)

**Direction**: Audio module → Main controller  
**Purpose**: Acknowledge stop request  
**DLC**: 8 bytes

```
┌─────┬─────┬─────────────────────────────────────┐
│Queue│Stat │           Reserved                  │
└─────┴─────┴─────────────────────────────────────┘

Byte 0: Queue ID (echoed from request)
Byte 1: Status Code
        0x00 = Success (sound stopped)
        0x01 = Not found (invalid queue ID or already stopped)
Bytes 2-7: Reserved (must be 0x00)

Example - Success:
CAN ID: 0x424
Data: [03 00 00 00 00 00 00 00]
       │  │
       │  └─ Status: 0x00 (stopped)
       └─ Queue ID: 3
```

### STOP_ALL_REQUEST (0x422)

**Direction**: Main controller → Audio module  
**Purpose**: Stop all currently playing sounds  
**DLC**: 8 bytes

```
┌─────────────────────────────────────────────────┐
│                  Reserved (all 0x00)            │
└─────────────────────────────────────────────────┘

Bytes 0-7: Reserved (must be 0x00)

Example:
CAN ID: 0x422
Data: [00 00 00 00 00 00 00 00]
```

**Note**: STOP_ALL_ACK is not currently implemented. Audio module stops all sounds but does not send acknowledgment.

### SOUND_FINISHED (0x425)

**Direction**: Audio module → Main controller  
**Purpose**: Notify that a sound has finished playing (non-looping only)  
**DLC**: 8 bytes

```
┌─────┬─────┬─────┬─────────────────────────────┐
│Queue│Index│Reas │        Reserved             │
└─────┴─────┴─────┴─────────────────────────────┘

Byte 0: Queue ID (the sound that finished)
Byte 1: Sound Index (original sound index)
Byte 2: Reason Code
        0x00 = Completed normally
        0x01 = Stopped by user (STOP_SOUND or STOP_ALL)
        0x02 = Error during playback
Bytes 3-7: Reserved (must be 0x00)

Example - Sound completed normally:
CAN ID: 0x425
Data: [03 01 00 00 00 00 00 00]
       │  │  │
       │  │  └─ Reason: 0x00 (completed)
       │  └─ Sound index: 1
       └─ Queue ID: 3
```

**Note**: SOUND_FINISHED is only sent for non-looping sounds that reach their natural end. Looping sounds never send this message unless explicitly stopped.

### Queue ID Management

**Audio Module Behavior**:
- Queue IDs assigned sequentially: 1, 2, 3, ..., 255, wrap to 1
- Queue ID 0 is reserved (invalid/error indicator)
- Maximum concurrent sounds: 4 (MAX_AUDIO_SOURCES)
- Queue IDs not reused immediately (10+ ID gap to prevent confusion)

**Main Controller Tracking**:
- Should maintain mapping of queue IDs to sound indices
- Track which sounds are looping (for cleanup on game state changes)
- Stop specific sounds by game event (e.g., stop alert when threat cleared)

### Sound Index Mapping

Standard sound indices (0-7 reserved for game sounds):

| Index | Filename | Description | Default Vol | Loopable |
|-------|----------|-------------|-------------|----------|
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
- Mapping configured in audio module firmware (`sound_config.c`)

---

## Message Flow Examples

### Example 1: Play One-Shot Sound

```
Main Controller                          Audio Module
     │                                        │
     ├── PLAY_SOUND_REQUEST (0x420) ────────→│
     │   [01 00 64 00 00 00 00 00]           │
     │   (sound 1, no loop, 100% vol)        │
     │                                        │
     │                                   [Load sound]
     │                                   [Start playback]
     │                                        │
     │←─── PLAY_SOUND_ACK (0x423) ───────────┤
     │   [01 00 03 00 00 00 00 00]           │
     │   (success, queue_id=3)               │
     │                                        │
     │                          [Sound plays for 1-2s]
     │                                        │
     │←─── SOUND_FINISHED (0x425) ───────────┤
     │   [03 01 00 00 00 00 00 00]           │
     │   (queue 3, completed normally)       │
     ▼                                        ▼
```

### Example 2: Play Looping Alert, Stop Later

```
Main Controller                          Audio Module
     │                                        │
     ├── PLAY_SOUND_REQUEST (0x420) ────────→│
     │   [04 01 50 00 00 00 00 00]           │
     │   (sound 4, LOOP, 80% vol)            │
     │                                        │
     │←─── PLAY_SOUND_ACK (0x423) ───────────┤
     │   [04 00 05 00 00 00 00 00]           │
     │   (success, queue_id=5)               │
     │                                        │
     │                       [Sound loops continuously]
     │                                        │
[10 seconds pass, threat cleared]            │
     │                                        │
     ├── STOP_SOUND_REQUEST (0x421) ────────→│
     │   [05 00 00 00 00 00 00 00]           │
     │   (stop queue_id=5)                   │
     │                                        │
     │                                   [Stop sound]
     │                                        │
     │←─── STOP_SOUND_ACK (0x424) ───────────┤
     │   [05 00 00 00 00 00 00 00]           │
     │   (queue 5 stopped)                   │
     ▼                                        ▼
```

### Example 3: Error Handling (Mixer Full)

```
Main Controller                          Audio Module
     │                                        │
[4 sounds already playing]                   │
     │                                        │
     ├── PLAY_SOUND_REQUEST (0x420) ────────→│
     │   [07 00 64 00 00 00 00 00]           │
     │   (sound 7, no loop, 100% vol)        │
     │                                        │
     │                               [Check mixer slots]
     │                               [All 4 slots full]
     │                                        │
     │←─── PLAY_SOUND_ACK (0x423) ───────────┤
     │   [07 02 00 00 00 00 00 00]           │
     │   (error: mixer full, queue_id=0)     │
     │                                        │
[Wait 500ms]                                 │
[Retry or stop oldest sound]                 │
     ▼                                        ▼
```

---

## Error Handling

### Audio Module Error Codes

| Status | Meaning | Recommended Action |
|--------|---------|-------------------|
| 0x01 | File not found | Check sound index mapping, verify SD card files |
| 0x02 | Mixer full | Wait 500ms and retry, or stop oldest non-looping sound |
| 0x03 | SD card error | Check SD card connection, retry once after 1s delay |
| 0xFF | Unknown error | Log error, retry once, disable sound if persistent |

### Retry Strategy

**File not found (0x01)**:
- Do NOT retry (file is missing)
- Log error and use fallback sound

**Mixer full (0x02)**:
- Wait 500ms and retry once
- Alternative: Stop oldest non-looping sound, then retry

**SD card error (0x03)**:
- Retry once after 1 second delay
- If still fails, disable sound features for session

---

## Timing Constants

### Discovery

| Parameter | Value | Notes |
|-----------|-------|-------|
| Query interval | Once at boot | No periodic queries |
| Response timeout | 500ms | Wait for all modules |
| Max modules | 14 | Limited by CAN ID space |

### Audio Protocol

| Parameter | Value | Notes |
|-----------|-------|-------|
| ACK timeout | 200ms | Wait for PLAY_SOUND_ACK |
| Retry delay | 500ms | Delay before retry on mixer full |
| Max retries | 1 | Retry once on mixer full, else fail |
| Max concurrent | 4 | Audio mixer slots |

---

## Protocol Version

**Current Version**: 1.0  
**Implementation Status**:
- ✅ Discovery protocol (MODULE_QUERY, MODULE_ANNOUNCE)
- ✅ Audio module protocol (PLAY, STOP, ACK, FINISHED)
- 📋 Future: Display module protocol
- 📋 Future: Lighting module protocol
- 📋 Future: Generic multi-module routing

**Changelog**:
- **v1.0** (2026-01-05): Initial specification with discovery and audio protocols

---

## Future Extensions

### Short Term
- STOP_ALL_ACK implementation (with stopped sound count)
- Volume query command (get current master volume)
- Audio module status periodic messages

### Medium Term
- Display module protocol (LCD/OLED text, graphics commands)
- Lighting module protocol (LED strips, RGB control)
- Generic message routing layer (priority-based ID structure)

### Long Term
- Firmware update over CAN (OTA for modules)
- Bulk data transfer (multi-frame segmentation)
- Time synchronization (coordinated effects across modules)
- Bus monitoring/diagnostics interface

---

## References

**Component Documentation**:
- `/ots-fw-shared/components/can_driver/` - Generic CAN driver (hardware layer)
- `/ots-fw-shared/components/can_discovery/` - Discovery protocol implementation
- `/ots-fw-shared/components/can_audiomodule/` - Audio protocol implementation

**Implementation**:
- `ots-fw-main` - Main controller (initiates discovery, sends commands)
- `ots-fw-audiomodule` - Audio module (responds to discovery, handles sound commands)
- `ots-fw-cantest` - CAN bus testing and debugging tool

**Standards**:
- CAN 2.0B Specification (ISO 11898)
- ESP-IDF TWAI Driver Documentation

---

**Note**: For implementation patterns, code examples, and debugging guidance, see [`/doc/developer/canbus-protocol.md`](../doc/developer/canbus-protocol.md).
