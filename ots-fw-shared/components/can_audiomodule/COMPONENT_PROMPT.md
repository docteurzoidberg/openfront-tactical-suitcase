# CAN Audio Module Component

## Overview

The `can_audiomodule` component provides audio module-specific CAN message definitions and parsing/building functions for the OTS (OpenFront Tactical Station) audio module communication.

## Purpose

This shared component enables both the **main controller** (`ots-fw-main`) and **audio module** (`ots-fw-audiomodule`) to:
- Send PLAY_SOUND and STOP_SOUND commands
- Receive ACK and FINISHED notifications
- Exchange STATUS messages
- Use consistent message formats and error codes

## Location

- **Component**: `/ots-fw-shared/components/can_audiomodule/`
- **Protocol Spec**: `/ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md`
- **Integration Guide**: `/ots-fw-shared/prompts/CAN_PROTOCOL_INTEGRATION.md`

## Files

- `can_audio_protocol.h` - Audio message IDs, flags, status bits, error codes, function declarations
- `can_audio_protocol.c` - Message parsing and building implementation
- `CMakeLists.txt` - ESP-IDF component registration
- `idf_component.yml` - Component dependencies (requires `can_driver`)
- `COMPONENT_PROMPT.md` - This file

## Dependencies

Requires the `can_driver` component for:
- `can_frame_t` structure
- Basic CAN TX/RX functions

## Message IDs

Audio protocol uses the **0x420-0x42F** CAN ID block:

| ID | Name | Direction | Description |
|----|------|-----------|-------------|
| 0x420 | PLAY_SOUND | main → audio | Play request with loop/volume |
| 0x421 | STOP_SOUND | main → audio | Stop by queue ID |
| 0x422 | SOUND_STATUS | audio → main | Periodic status (1Hz) |
| 0x423 | SOUND_ACK | audio → main | Play ACK with queue ID |
| 0x424 | STOP_ALL | main → audio | Stop all sounds |
| 0x425 | SOUND_FINISHED | audio → main | Playback completion |
| 0x426-0x42F | Reserved | - | Future audio features |

## Key Constants

### Flags
- `CAN_AUDIO_FLAG_INTERRUPT` - Interrupt current playback
- `CAN_AUDIO_FLAG_HIGH_PRIORITY` - High priority sound
- `CAN_AUDIO_FLAG_LOOP` - Loop until stopped

### Status Bits
- `CAN_AUDIO_STATUS_READY` - Module ready
- `CAN_AUDIO_STATUS_SD_MOUNTED` - SD card available
- `CAN_AUDIO_STATUS_PLAYING` - Currently playing
- `CAN_AUDIO_STATUS_MUTED` - Hardware muted
- `CAN_AUDIO_STATUS_ERROR` - Error state

### Error Codes
- `CAN_AUDIO_ERR_OK` - No error
- `CAN_AUDIO_ERR_FILE_NOT_FOUND` - Sound file missing
- `CAN_AUDIO_ERR_SD_ERROR` - SD card error
- `CAN_AUDIO_ERR_MIXER_FULL` - No free mixer slots
- `CAN_AUDIO_ERR_INVALID_QUEUE_ID` - Queue ID not found

### Special Values
- `CAN_AUDIO_VOLUME_USE_POT` (0xFF) - Use hardware volume pot
- `CAN_AUDIO_QUEUE_ID_INVALID` (0x00) - Invalid queue ID

### Timing Constants
- `CAN_AUDIO_STATUS_INTERVAL_MS` (5000) - STATUS message interval
- `CAN_AUDIO_ACK_TIMEOUT_MS` (200) - ACK response timeout
- `CAN_AUDIO_RETRY_DELAY_MS` (500) - Retry delay on mixer full

## API Functions

### Utility Functions

```c
uint8_t can_audio_allocate_queue_id(uint8_t *current_id);
```
Allocate next queue ID (1-255, wraps). Pass pointer to your counter variable.

```c
uint16_t can_audio_allocate_request_id(uint16_t *current_id);
```
Generate next request ID (0-65535, wraps). Pass pointer to your counter variable.

```c
bool can_audio_queue_id_is_valid(uint8_t queue_id);
```
Check if queue ID is valid (1-255). Returns false for 0.

### Building (Main Controller Sends)

```c
void can_audio_build_play_sound(uint16_t sound_index, 
                                uint8_t flags, 
                                uint8_t volume, 
                                uint16_t request_id, 
                                can_frame_t *frame);
```
Build PLAY_SOUND (0x420) request to audio module.

```c
void can_audio_build_stop_sound(uint8_t queue_id, 
                                uint8_t flags, 
                                uint16_t request_id, 
                                can_frame_t *frame);
```
Build STOP_SOUND (0x421) request to audio module.

```c
void can_audio_build_stop_all(can_frame_t *frame);
```
Build STOP_ALL (0x424) request to audio module.

### Parsing (Audio Module Receives)

```c
bool can_audio_parse_play_sound(const can_frame_t *frame, 
                                uint16_t *sound_index, 
                                uint8_t *flags, 
                                uint8_t *volume, 
                                uint16_t *request_id);
```
Parse PLAY_SOUND (0x420) request from main controller.

```c
bool can_audio_parse_stop_sound(const can_frame_t *frame, 
                                uint8_t *queue_id, 
                                uint8_t *flags, 
                                uint16_t *request_id);
```
Parse STOP_SOUND (0x421) request from main controller.

### Building (Audio Module Sends)

```c
void can_audio_build_sound_status(uint8_t state_bits, 
                                  uint16_t current_sound, 
                                  uint8_t error_code, 
                                  uint8_t volume, 
                                  uint16_t uptime,
                                  can_frame_t *frame);
```
Build STATUS (0x422) message (sent periodically, 1Hz).

```c
void can_audio_build_sound_ack(uint8_t ok, 
                               uint16_t sound_index, 
                               uint8_t queue_id,
                               uint8_t error_code, 
                               uint16_t request_id, 
                               can_frame_t *frame);
```
Build ACK (0x423) response with queue ID (within 200ms of request).

```c
void can_audio_build_sound_finished(uint8_t queue_id, 
                                   uint16_t sound_index, 
                                   uint8_t reason, 
                                   can_frame_t *frame);
```
Build FINISHED (0x425) notification when playback completes or stops.

## Usage Example (Main Controller)

```c
#include "can_audio_protocol.h"
#include "can_driver.h"

// State variables
static uint16_t g_request_id = 0;

// Send play sound command
void play_game_start_sound(void) {
    can_frame_t frame;
    uint16_t sound_index = 0;  // game_start.mp3
    uint8_t flags = 0;  // No special flags
    uint8_t volume = 80;  // 80% volume
    uint16_t request_id = can_audio_allocate_request_id(&g_request_id);
    
    // Build frame using shared component
    can_audio_build_play_sound(sound_index, flags, volume, request_id, &frame);
    can_driver_send(&frame);
}

// Send stop by queue ID (with validation)
void stop_sound(uint8_t queue_id) {
    if (!can_audio_queue_id_is_valid(queue_id)) {
        printf("Invalid queue_id: %d\n", queue_id);
        return;
    }
    
    can_frame_t frame;
    uint16_t request_id = can_audio_allocate_request_id(&g_request_id);
    can_audio_build_stop_sound(queue_id, 0, request_id, &frame);
    can_driver_send(&frame);
}

// Send stop all
void stop_all_sounds(void) {
    can_frame_t frame;
    can_audio_build_stop_all(&frame);
    can_driver_send(&frame);
}

// Handle ACK response
void handle_can_message(const can_frame_t *frame) {
    if (frame->id == CAN_ID_SOUND_ACK) {
        uint8_t ok = frame->data[0];
        uint16_t sound_index = frame->data[1] | (frame->data[2] << 8);
        uint8_t queue_id = frame->data[3];
        uint8_t error_code = frame->data[4];
        
        if (ok && can_audio_queue_id_is_valid(queue_id)) {
            printf("Sound %d started with queue_id=%d\n", sound_index, queue_id);
        } else {
            printf("Sound %d failed: error=%d\n", sound_index, error_code);
        }
    }
}
```

## Usage Example (Audio Module)

```c
#include "can_audio_protocol.h"

// State variables
static uint8_t g_queue_id_counter = 1;

// Handle incoming PLAY request
void handle_can_message(const can_frame_t *frame) {
    if (frame->id == CAN_ID_PLAY_SOUND) {
        uint16_t sound_index;
        uint8_t flags, volume;
        uint16_t request_id;
        
        if (can_audio_parse_play_sound(frame, &sound_index, &flags, 
                                       &volume, &request_id)) {
            // Start playback
            uint8_t handle = audio_mixer_play(sound_index, flags, volume);
            
            // Allocate queue ID using shared utility
            uint8_t queue_id = can_audio_allocate_queue_id(&g_queue_id_counter);
            
            // Send ACK
            can_frame_t ack;
            can_audio_build_sound_ack(
                can_audio_queue_id_is_valid(queue_id),  // ok
                sound_index, 
                queue_id,
                queue_id ? CAN_AUDIO_ERR_OK : CAN_AUDIO_ERR_MIXER_FULL,
                request_id, 
                &ack
            );
            can_driver_send(&ack);
        }
    }
}
```

## Integration

### ots-fw-audiomodule
Already integrated:
- Uses protocol for receiving commands
- Sends ACK and FINISHED messages
- Implements queue ID system

### ots-fw-main
To integrate:
1. Add to `CMakeLists.txt`:
   ```cmake
   set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/../ots-fw-shared/components")
   ```
2. Require in component CMakeLists:
   ```cmake
   REQUIRES can_driver can_audiomodule
   ```
3. Include header: `#include "can_audio_protocol.h"`
4. Create sound module wrapper (see `/ots-fw-main/prompts/SOUND_MODULE_PROMPT.md`)

## Protocol Versions

Current version: **1.0.0** (Queue ID system with loop support)

Future versions may add:
- Volume fade commands
- Playlist support
- Effects (pitch, speed)
- Multi-channel mixing control

## See Also

- **Protocol Specification**: `/ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md`
- **Integration Guide**: `/ots-fw-shared/prompts/CAN_PROTOCOL_INTEGRATION.md`
- **CAN Driver Component**: `/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md`
- **Generic CAN Architecture**: `/ots-fw-shared/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md`
