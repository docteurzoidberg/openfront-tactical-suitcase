# CAN Test Firmware - Project Prompt

## Overview

`ots-fw-cantest` is an **interactive CAN bus testing and debugging tool** for the OpenFront Tactical Suitcase (OTS) project. It provides real-time protocol validation, traffic monitoring, and module simulation capabilities through a command-line interface.

## Purpose

This firmware is a **development and debugging tool**, NOT part of the production system. It enables:

1. **Protocol Validation**: Verify CAN message formats match specification
2. **Module Simulation**: Emulate audio modules or controllers without physical hardware
3. **Traffic Monitoring**: Passive bus sniffer with human-readable protocol decoding
4. **Integration Testing**: Test communication between main controller and modules
5. **Debugging Aid**: Diagnose protocol issues, timing problems, and communication failures
6. **Shared Component Validation**: Validate CAN shared components in `ots-fw-shared` (e.g., `can_driver`, `can_discovery`, `can_audiomodule`) as used by both `ots-fw-main` and `ots-fw-audiomodule`

**Key Distinction from ots-fw-can-hw-test**:
- **ots-fw-can-hw-test**: Hardware validation (tests TWAI peripheral and transceiver)
- **ots-fw-cantest**: Protocol validation (tests CAN message formats and behavior)

## Supported Boards

### ESP32-S3 DevKit (Primary)
- **TX Pin**: GPIO5
- **RX Pin**: GPIO4
- **Use Case**: Main controller testing and protocol development
- **PlatformIO Environment**: `esp32-s3-devkit`

### ESP32-A1S AudioKit (Secondary)
- **TX Pin**: GPIO5
- **RX Pin**: GPIO18
- **Use Case**: Audio module protocol testing
- **PlatformIO Environment**: `esp32-a1s-audiokit`

## Operating Modes

The firmware provides **4 distinct operating modes** selectable via single-letter commands:

### 1. Idle Mode (Default)

**Purpose**: Passive state, displays incoming messages without active processing

**Behavior**:
- Shows all CAN bus traffic with decoder
- No auto-responses or transmissions
- Useful for observing bus activity between other devices

**Entry**: Type `i` or `q` from any other mode

**Display**:
```
0x420 [8] PLAY_SOUND      | 05 00 64 00 00 00 00 00
      Sound: 5, Vol: 100, Flags: 0x00
```

### 2. Monitor Mode

**Purpose**: Passive bus sniffer with timestamps and statistics

**Behavior**:
- Timestamps each message relative to start time
- Counts RX/TX messages and errors
- Displays real-time message rate
- Shows final statistics on exit

**Entry**: Type `m`

**Exit**: Type `q` (shows statistics)

**Display**:
```
╔════════════════════════════════════════════════════════════════╗
║                    MONITOR MODE - PASSIVE                      ║
╠════════════════════════════════════════════════════════════════╣
║  Listening to all CAN bus traffic...                          ║
║  Press 'q' to stop monitoring                                 ║
╚════════════════════════════════════════════════════════════════╝

[   0.123] 0x411 [8] MODULE_QUERY      | FF 00 00 00 00 00 00 00
      (Broadcast discovery query)
[   0.158] 0x410 [8] MODULE_ANNOUNCE   | 01 01 00 01 42 00 00 00
      Type: Audio Module (0x01), Ver: 1.0, Block: 0x42, Caps: 0x01
[   5.234] 0x420 [8] PLAY_SOUND        | 01 00 64 00 00 00 00 00
      Sound: 1, Vol: 100, Flags: 0x00

--- Statistics on 'q' ---
Duration: 10.45 seconds
Messages RX: 15
Avg Rate: 1.4 msg/s
```

**Use Cases**:
- Debug communication issues between devices
- Verify message timing and order
- Monitor bus health (error counts)
- Capture traffic logs for analysis

### 3. Audio Module Simulator

**Purpose**: Emulate a CAN audio module for testing main controller

**Behavior**:
- **Auto-responds to MODULE_QUERY**: Sends MODULE_ANNOUNCE (type=AUDIO, v1.0, block=0x42)
- **Auto-responds to PLAY_SOUND**: Sends SOUND_ACK with assigned queue_id
- **Auto-responds to STOP_SOUND**: Sends STOP_ACK
- **Auto-responds to STOP_ALL**: Acknowledges (no specific ACK message in protocol yet)
- **Manual SOUND_FINISHED**: User can trigger via `f <queue_id>` command

**Entry**: Type `a`

**Exit**: Type `q`

**Commands in Audio Module Mode**:
- `f <queue_id>` - Send SOUND_FINISHED for specific queue ID
- `q` - Exit audio module mode

**Display**:
```
╔════════════════════════════════════════════════════════════════╗
║                AUDIO MODULE SIMULATOR - ACTIVE                 ║
╠════════════════════════════════════════════════════════════════╣
║  Responding to discovery and sound commands...                ║
║  Type 'f <qid>' to send SOUND_FINISHED                        ║
║  Press 'q' to stop                                            ║
╚════════════════════════════════════════════════════════════════╝

← RX: 0x411 [8] MODULE_QUERY
→ TX: MODULE_ANNOUNCE (Audio Module v1.0)

← RX: 0x420 [8] PLAY_SOUND | 05 00 64 00 00 00 00 00
      Sound: 5, Vol: 100, Flags: 0x00
→ TX: SOUND_ACK (queue_id=3, status=SUCCESS)

[User types: f 3]
→ TX: SOUND_FINISHED (queue_id=3, sound=5, reason=COMPLETED)
```

**Use Cases**:
- Test main controller CAN integration without physical audio module
- Validate discovery protocol implementation
- Test sound command handling and ACK responses
- Debug queue ID tracking and lifecycle

### 4. Controller Simulator

**Purpose**: Emulate main controller for testing audio modules

**Behavior**:
- Sends commands to modules on bus
- Does NOT auto-respond to incoming messages (passive to module responses)
- Manual command sending only

**Entry**: Type `c`

**Exit**: Type `q`

**Commands in Controller Mode**:
- `d` - Send MODULE_QUERY (discovery broadcast)
- `p <sound_index>` - Send PLAY_SOUND (index 0-255)
- `s <queue_id>` - Send STOP_SOUND for specific queue ID
- `x` - Send STOP_ALL
- `q` - Exit controller mode

**Display**:
```
╔════════════════════════════════════════════════════════════════╗
║                  CONTROLLER SIMULATOR - ACTIVE                 ║
╠════════════════════════════════════════════════════════════════╣
║  Send commands to modules on bus                              ║
║  d       - Send MODULE_QUERY                                  ║
║  p <idx> - Send PLAY_SOUND                                    ║
║  s <qid> - Send STOP_SOUND                                    ║
║  x       - Send STOP_ALL                                      ║
║  q       - Exit controller mode                               ║
╚════════════════════════════════════════════════════════════════╝

[User types: d]
→ TX: MODULE_QUERY (broadcast)
← RX: 0x410 [8] MODULE_ANNOUNCE | 01 01 00 01 42 00 00 00
      Type: Audio Module (0x01), Ver: 1.0, Block: 0x42

[User types: p 5]
→ TX: PLAY_SOUND (sound=5, vol=100, flags=0x00)
← RX: 0x423 [8] SOUND_ACK | 05 00 03 00 00 00 00 00
      Sound: 5, Status: SUCCESS, Queue ID: 3
```

**Use Cases**:
- Test audio module firmware without main controller
- Validate module response times and ACK formats
- Debug sound playback and queue management
- Test error handling (mixer full, file not found, etc.)

## Protocol Support

The firmware implements the **OTS CAN Bus Protocol** as defined in:
- **Specification**: [`/prompts/CANBUS_MESSAGE_SPEC.md`](../prompts/CANBUS_MESSAGE_SPEC.md)
- **Developer Guide**: [`/doc/developer/canbus-protocol.md`](../doc/developer/canbus-protocol.md)

### Discovery Protocol (0x410-0x411)

| CAN ID | Direction | Message | Decoder |
|--------|-----------|---------|---------|
| **0x410** | Module → Main | MODULE_ANNOUNCE | ✅ Full decode |
| **0x411** | Main → Modules | MODULE_QUERY | ✅ Recognized |

**MODULE_ANNOUNCE Decoding**:
- Module type (0x01 = Audio Module)
- Firmware version (major.minor)
- CAN block allocation (0x42 = 0x420-0x42F)
- Capability flags

### Audio Protocol (0x420-0x42F)

| CAN ID | Direction | Message | Decoder |
|--------|-----------|---------|---------|
| **0x420** | Main → Audio | PLAY_SOUND | ✅ Full decode (sound index, volume, flags) |
| **0x421** | Main → Audio | STOP_SOUND | ✅ Decode queue_id |
| **0x422** | Main → Audio | STOP_ALL | ✅ Recognized |
| **0x423** | Audio → Main | SOUND_ACK | ✅ Full decode (sound, status, queue_id) |
| **0x424** | Audio → Main | STOP_ACK | ✅ Decode queue_id + status |
| **0x425** | Audio → Main | SOUND_FINISHED | ✅ Full decode (queue_id, sound, reason) |

**Status Code Decoding**:
- `0x00` - SUCCESS
- `0x01` - FILE_NOT_FOUND
- `0x02` - MIXER_FULL
- `0x03` - SD_ERROR
- `0xFF` - UNKNOWN_ERROR

**Reason Code Decoding** (SOUND_FINISHED):
- `0x00` - COMPLETED (normal end)
- `0x01` - STOPPED_BY_USER
- `0x02` - PLAYBACK_ERROR

## Build & Flash

### Using PlatformIO (Recommended)

**ESP32-S3 DevKit:**
```bash
cd ots-fw-cantest
pio run -e esp32-s3-devkit -t upload && pio device monitor
```

**ESP32-A1S AudioKit:**
```bash
cd ots-fw-cantest
pio run -e esp32-a1s-audiokit -t upload && pio device monitor
```

### Using ESP-IDF

**ESP32-S3:**
```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

## Hardware Setup

### Minimal Setup (Mock Mode)

For testing without physical CAN bus:
- Flash firmware to ESP32
- Monitor serial output (115200 baud)
- Firmware detects no transceiver and runs in **mock mode**
- Commands work, but no physical CAN traffic

**Use Case**: Protocol development, message format testing

### Full Setup (Physical CAN Bus)

For real CAN communication:
- **ESP32-S3 DevKit** with **CAN transceiver** (SN65HVD230, MCP2551, or TJA1050)
- **Second device** (ots-fw-main or ots-fw-audiomodule)
- **120Ω termination** at each bus end
- **Common ground** between all devices

**Wiring**:
```
ESP32-S3       CAN Transceiver
GPIO5 -------> CTX (TXD)
GPIO4 -------> CRX (RXD)
3.3V  -------> VCC
GND   -------> GND

CAN Bus:
Device 1 CANH <---> Device 2 CANH
Device 1 CANL <---> Device 2 CANL
120Ω between CANH-CANL on Device 1
120Ω between CANH-CANL on Device 2
```

## Command Reference

### Mode Control Commands

| Key | Command | Description |
|-----|---------|-------------|
| `m` | Monitor Mode | Passive bus sniffer with timestamps |
| `a` | Audio Module Simulator | Emulate audio module (auto-responds) |
| `c` | Controller Simulator | Emulate main controller (send commands) |
| `i` | Idle Mode | Stop current mode, display traffic only |
| `q` | Quit/Stop | Exit current mode, return to idle |

### Controller Commands (in `c` mode)

| Key | Command | Parameters | Example |
|-----|---------|------------|---------|
| `d` | Discovery Query | None | `d` → Send MODULE_QUERY |
| `p` | Play Sound | `<sound_index>` | `p 5` → Play sound 5 |
| `s` | Stop Sound | `<queue_id>` | `s 3` → Stop queue ID 3 |
| `x` | Stop All | None | `x` → Stop all sounds |

### Audio Module Commands (in `a` mode)

| Key | Command | Parameters | Example |
|-----|---------|------------|---------|
| `f` | Sound Finished | `<queue_id>` | `f 3` → Send SOUND_FINISHED for queue 3 |

### Display Options (any mode)

| Key | Command | Description |
|-----|---------|-------------|
| `r` | Toggle Raw Hex | Show/hide hex data bytes |
| `v` | Toggle Parsed | Show/hide decoded message fields |
| `t` | Show Statistics | Display RX/TX counts, error counts, TWAI status |

### General Commands

| Key | Command | Description |
|-----|---------|-------------|
| `h` | Help | Show full command reference |
| `?` | Help | Same as `h` |

## Testing Workflows

### Workflow 1: Test Discovery Protocol

**Setup**: Two ESP32-S3 boards with CAN transceivers

**Steps**:
1. Flash `ots-fw-cantest` to Board A
2. Flash `ots-fw-main` to Board B (or another cantest as audio module)
3. Connect CAN bus (CANH-CANH, CANL-CANL, GND-GND, 120Ω termination)

**On Board A (cantest)**:
```
> a                    # Enter audio module simulator mode
(wait for queries)
```

**On Board B (fw-main)**:
- Main controller boots and sends MODULE_QUERY
- Board A receives query and responds with MODULE_ANNOUNCE
- Board B logs: "✓ Audio module v1.0 detected (CAN block 0x42)"

**Expected Traffic**:
```
[Monitor on Board A]
← RX: 0x411 [8] MODULE_QUERY | FF 00 00 00 00 00 00 00
→ TX: MODULE_ANNOUNCE (Audio Module v1.0, block 0x42)
```

### Workflow 2: Test Sound Commands

**Setup**: cantest (controller) + ots-fw-audiomodule (real audio)

**On cantest**:
```
> c                    # Enter controller mode
> p 1                  # Send PLAY_SOUND for sound index 1
(wait for ACK)
```

**Expected Traffic**:
```
→ TX: PLAY_SOUND (sound=1, vol=100, flags=0x00)
← RX: 0x423 [8] SOUND_ACK | 01 00 03 00 00 00 00 00
      Sound: 1, Status: SUCCESS, Queue ID: 3
(2-3 seconds later)
← RX: 0x425 [8] SOUND_FINISHED | 03 01 00 00 00 00 00 00
      Queue ID: 3, Sound: 1, Reason: COMPLETED
```

### Workflow 3: Debug Communication Issues

**Setup**: cantest (monitor) + fw-main + fw-audiomodule (live system)

**On cantest**:
```
> m                    # Enter monitor mode
(observe all traffic between main and audio module)
> q                    # Stop monitoring, view statistics
```

**Use Cases**:
- Verify messages are sent with correct format
- Check timing between commands and responses
- Identify missing ACKs or timeouts
- Measure message rate and bus utilization

### Workflow 4: Test Error Handling

**Setup**: cantest (controller) + fw-audiomodule

**On cantest**:
```
> c                    # Controller mode
> p 1                  # Play sound 1 (queue_id=1)
> p 2                  # Play sound 2 (queue_id=2)
> p 3                  # Play sound 3 (queue_id=3)
> p 4                  # Play sound 4 (queue_id=4)
> p 5                  # This should fail: MIXER_FULL
```

**Expected**:
```
← RX: SOUND_ACK (sound=5, status=MIXER_FULL, queue_id=0)
```

**Verify**: Audio module correctly rejects when mixer full

## Serial Output Format

### Baud Rate
**115200** (configured in `platformio.ini`)

### Message Format

**Raw Hex Display** (if `r` enabled):
```
0x420 [8] PLAY_SOUND      | 05 00 64 00 00 00 00 00
```

**Parsed Display** (if `v` enabled):
```
0x420 [8] PLAY_SOUND
      Sound: 5, Vol: 100, Flags: 0x00
```

**Both Enabled** (default):
```
0x420 [8] PLAY_SOUND      | 05 00 64 00 00 00 00 00
      Sound: 5, Vol: 100, Flags: 0x00
```

**Monitor Mode** (adds timestamp):
```
[   5.234] 0x420 [8] PLAY_SOUND | 05 00 64 00 00 00 00 00
      Sound: 5, Vol: 100, Flags: 0x00
```

## Mock Mode vs Physical Mode

The firmware **auto-detects** CAN hardware presence during initialization.

### Physical Mode (Transceiver Detected)

**Indication**:
```
Testing CAN hardware...
✓ CAN hardware detected (physical mode)
```

**Behavior**:
- All commands sent on physical CAN bus
- RX task actively monitors bus traffic
- Real message exchanges with other devices

### Mock Mode (No Transceiver)

**Indication**:
```
Testing CAN hardware...
⚠ No CAN transceiver detected (mock mode)
  Commands will work, but no bus traffic will be received
```

**Behavior**:
- Commands logged to serial (no physical transmission)
- RX task sleeps (no bus monitoring)
- Useful for testing protocol logic without hardware

**Use Cases**:
- Develop/test on single board without wiring
- Verify message formatting and parsing
- Test CLI command handling

## Code Structure

```
ots-fw-cantest/
  platformio.ini           # Build config (GPIO pins, boards)
  CMakeLists.txt           # ESP-IDF project
  partitions.csv           # Partition table
  
  src/
    main.c                 # Entry point, initialization, CAN RX task
    cli_handler.c          # Command-line interface and input handling
    can_monitor.c          # Monitor mode implementation
    can_simulator.c        # Audio module & controller simulators
    can_decoder.c          # Protocol message parsing and display
    
    include/
      can_test.h           # Shared types and function declarations
```

### Component Architecture

```
main.c
  ├─> CAN RX Task (loops on can_driver_receive)
  │     └─> Routes frames to active mode
  │
  ├─> CLI Handler (stdin command processing)
  │     ├─> Mode switching (m/a/c/i/q)
  │     ├─> Controller commands (d/p/s/x)
  │     ├─> Audio module commands (f)
  │     └─> Display options (r/v/t)
  │
  └─> Mode Processors
        ├─> can_monitor (timestamp, stats, display)
        ├─> can_simulator_audio_module (auto-respond)
        ├─> can_simulator_controller (send commands)
        └─> can_decoder (parse and pretty-print)
```

### Shared Components

Uses components from `ots-fw-shared/`:
- **can_driver**: Generic CAN/TWAI hardware abstraction
- **can_discovery**: Discovery protocol constants and helpers

## Troubleshooting

### No CAN Traffic Received

**Symptoms**: Monitor mode shows no messages, statistics show RX=0

**Causes**:
1. **Mock mode active**: No physical transceiver
2. **Wiring issue**: CANH/CANL disconnected or swapped
3. **No termination**: Missing 120Ω resistors at bus ends
4. **No other device**: Nothing transmitting on bus

**Fixes**:
- Verify transceiver power and wiring
- Check for "✓ CAN hardware detected" at startup
- Measure voltage on CANH/CANL with oscilloscope
- Ensure another device is transmitting (use two cantest boards)

### Commands Sent But Not Received

**Symptoms**: TX counter increments, but other device doesn't respond

**Causes**:
1. **Wrong CAN ID**: Recipient not listening on that ID
2. **Bitrate mismatch**: Sender and receiver use different rates (check: 125 kbps vs 500 kbps)
3. **Bus error**: Device enters BUS_OFF state after too many errors

**Fixes**:
- Verify both devices use **125 kbps** (cantest default)
- Check TWAI status: `t` command shows TX/RX error counters
- Look for "State: BUS_OFF" in statistics (indicates wiring problem)
- Try hardware test firmware (`ots-fw-can-hw-test`) to validate transceiver

### Parsed Messages Show "UNKNOWN"

**Symptoms**: Raw hex displays correctly, but decoder shows "UNKNOWN"

**Cause**: CAN ID not recognized by decoder

**Fix**: Add CAN ID to `can_decoder.c`:
```c
#define CAN_ID_NEW_MESSAGE 0x430  // Add definition

const char* can_decoder_get_message_name(uint16_t can_id) {
    switch (can_id) {
        // ... existing cases ...
        case CAN_ID_NEW_MESSAGE: return "NEW_MESSAGE";
        default: return "UNKNOWN";
    }
}
```

### MIXER_FULL Errors Unexpectedly

**Symptoms**: Audio module rejects PLAY_SOUND with MIXER_FULL status

**Cause**: Audio module has only 4 mixer slots (MAX_AUDIO_SOURCES)

**Expected Behavior**: This is correct! Test that audio module enforces limits.

**Workaround**: Send STOP_SOUND or wait for SOUND_FINISHED before playing 5th sound

### Module Not Responding to Discovery

**Symptoms**: Send MODULE_QUERY (`d` command), no MODULE_ANNOUNCE received

**Causes**:
1. Module not running or not on bus
2. Module CAN pins misconfigured
3. Module firmware not implementing discovery protocol

**Fixes**:
- Verify module is powered and running (check serial output)
- Swap cantest and module (test other direction)
- Use monitor mode on third board to see if query is sent

## Statistics Display

Press `t` in any mode to show detailed statistics:

```
╔════════════════════════════════════════════════════════════════╗
║                         STATISTICS                             ║
╠════════════════════════════════════════════════════════════════╣
║  Mode:            CONTROLLER                                   ║
║  Messages RX:     15                                           ║
║  Messages TX:     8                                            ║
║  Errors:          0                                            ║
║  Show Raw Hex:    YES                                          ║
║  Show Parsed:     YES                                          ║
╚════════════════════════════════════════════════════════════════╝

TWAI Status:
  State:            RUNNING
  TX Queue:         0/10
  RX Queue:         0/10
  TX Error Counter: 0
  RX Error Counter: 0
  TX Failed:        0
  Bus-off Counter:  0
  Arbitration Lost: 0
```

**Key Indicators**:
- **State: RUNNING** = Good, CAN bus operational
- **State: BUS_OFF** = Too many errors, check wiring
- **TX/RX Error Counter** = Should be 0 or very low (< 10)
- **TX Failed** = Messages that couldn't be sent (queue full or bus error)

## Extending the Firmware

### Adding New Module Types

1. **Define CAN IDs** in `can_decoder.c`:
```c
#define CAN_ID_NEW_MODULE_CMD 0x430
```

2. **Add decoder function**:
```c
static void decode_new_module_cmd(const can_frame_t *frame) {
    // Parse frame->data[] and print fields
}
```

3. **Update message name lookup**:
```c
const char* can_decoder_get_message_name(uint16_t can_id) {
    case CAN_ID_NEW_MODULE_CMD: return "NEW_MODULE_CMD";
}
```

4. **Add simulator support** in `can_simulator.c`:
```c
// In simulator, respond to new commands
if (frame->id == CAN_ID_NEW_MODULE_CMD) {
    // Build response frame
    // Send via can_driver_send()
}
```

5. **Add CLI commands** in `cli_handler.c`:
```c
case 'n':  // New command
    // Build CAN frame
    can_simulator_send_custom(CAN_ID_NEW_MODULE_CMD, data, dlc);
    break;
```

### Custom Traffic Injection

For advanced testing, inject arbitrary CAN messages:

```c
// In cli_handler.c, add custom command
case 'z': {
    uint8_t data[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    can_simulator_send_custom(0x123, data, 8);
    printf("→ TX: Custom message 0x123\n");
    break;
}
```

### Automated Testing

Send commands via serial for scripted tests:

**Python Example**:
```python
import serial
import time

ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
time.sleep(2)

# Enter controller mode
ser.write(b'c\n')
time.sleep(0.5)

# Send discovery
ser.write(b'd\n')
time.sleep(1)
response = ser.read_all()
assert b'MODULE_ANNOUNCE' in response

# Play sound
ser.write(b'p 1\n')
time.sleep(0.5)
response = ser.read_all()
assert b'SOUND_ACK' in response

print("✓ All tests passed")
```

## Comparison with Related Tools

| Tool | Purpose | CAN Bus | Protocol | Use Case |
|------|---------|---------|----------|----------|
| **ots-fw-cantest** | Protocol testing | ✅ Physical or mock | ✅ Full protocol | Debug protocol logic |
| **ots-fw-can-hw-test** | Hardware validation | ✅ Physical only | ❌ None (raw frames) | Debug transceiver wiring |
| **ots-fw-main** | Production firmware | ✅ Physical | ✅ Controller side | Normal operation |
| **ots-fw-audiomodule** | Production firmware | ✅ Physical | ✅ Module side | Normal operation |

**When to Use**:
- ❓ "Is CAN hardware working?" → Use **ots-fw-can-hw-test**
- ❓ "Is protocol implementation correct?" → Use **ots-fw-cantest**
- ❓ "Need to test without audio module hardware?" → Use **ots-fw-cantest** (audio simulator)
- ❓ "Need to test without main controller?" → Use **ots-fw-cantest** (controller simulator)

## Success Criteria

### Module Simulation Works

**Test**: Run cantest in audio module mode, send discovery from fw-main
- ✅ Cantest responds with MODULE_ANNOUNCE
- ✅ fw-main logs "Audio module detected"
- ✅ fw-main can send PLAY_SOUND commands
- ✅ Cantest responds with SOUND_ACK

### Controller Simulation Works

**Test**: Run cantest in controller mode, connect to fw-audiomodule
- ✅ Send `d` → Audio module responds with MODULE_ANNOUNCE
- ✅ Send `p 1` → Audio module plays sound and responds with ACK
- ✅ Audio sends SOUND_FINISHED when playback completes

### Monitor Mode Works

**Test**: Monitor traffic between fw-main and fw-audiomodule
- ✅ Timestamps displayed for each message
- ✅ All messages decoded correctly
- ✅ Statistics show message rate and counts
- ✅ No errors or crashes

## Related Documentation

- **CAN Protocol Specification**: [`/prompts/CANBUS_MESSAGE_SPEC.md`](../prompts/CANBUS_MESSAGE_SPEC.md)
- **Developer Guide**: [`/doc/developer/canbus-protocol.md`](../doc/developer/canbus-protocol.md)
- **CAN Driver Component**: [`/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md`](../ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md)
- **Discovery Protocol**: [`/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`](../ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md)
- **Hardware Test**: [`/ots-fw-can-hw-test/PROJECT_PROMPT.md`](../ots-fw-can-hw-test/PROJECT_PROMPT.md)

---

**Last Updated**: January 7, 2026  
**Version**: 1.0.0  
**Firmware Type**: Development/testing tool (non-production)  
**Platforms**: ESP32-S3, ESP32-A1S (ESP-IDF framework)  
**Complexity**: Medium (interactive CLI + protocol implementation)
