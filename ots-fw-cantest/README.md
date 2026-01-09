# OTS CAN Test Firmware

Interactive CAN bus testing tool for OTS project development and debugging.

## CAN Protocol Reference

- **Protocol Specification**: [`/prompts/CANBUS_MESSAGE_SPEC.md`](../prompts/CANBUS_MESSAGE_SPEC.md) - Message formats, CAN IDs, byte layouts
- **Developer Guide**: [`/doc/developer/canbus-protocol.md`](../doc/developer/canbus-protocol.md) - Implementation examples and debugging

## Overview

This test firmware runs on an ESP32-S3 DevKit board and provides:

- **Monitor Mode**: Passive bus sniffer with protocol decoding
- **Audio Module Simulator**: Emulate audio module (responds to discovery and sound commands)
- **Controller Simulator**: Emulate main controller (send commands to modules)
- **Protocol Validation**: Real-time message parsing and validation
- **Traffic Injection**: Manually inject custom CAN messages
- **Shared Component Validation**: Validate shared CAN components (`ots-fw-shared`) used by `ots-fw-main` and `ots-fw-audiomodule`

## Hardware Setup

### Requirements

- ESP32-S3 DevKit board (same as ots-fw-main)
- CAN transceiver (TJA1050)
- 120Ω termination resistors (if at bus ends)

### Wiring

```
ESP32-S3 DevKit    CAN Transceiver    CAN Bus
GPIO4  ----------- CTX                
GPIO5  ----------- CRX
3.3V   ----------- VCC
GND    ----------- GND
                   CANH --------------- CANH (to other devices)
                   CANL --------------- CANL (to other devices)
```

**Note**: If no CAN transceiver is detected, the firmware runs in MOCK mode (logs to serial only).

## Building & Flashing

### Using PlatformIO

```bash
cd ots-fw-cantest
pio run -e esp32-s3-devkit -t upload
pio device monitor
```

## Usage

### Quick Start

1. Flash firmware to ESP32-S3 DevKit
2. Connect to serial terminal (115200 baud)
3. Type `h` or `?` for help

### Common Workflows

#### Test Discovery Protocol

```
> c                    # Enter controller mode
> d                    # Send MODULE_QUERY
(wait for MODULE_ANNOUNCE responses)
> q                    # Exit to idle
```

#### Simulate Audio Module

```
> a                    # Enter audio module simulator mode
(automatically responds to PLAY_SOUND, STOP_SOUND, discovery)
> f 3                  # Manually send SOUND_FINISHED for queue_id=3
> q                    # Exit to idle
```

#### Monitor Real Traffic

```
> m                    # Enter monitor mode
(watch all CAN bus traffic with timestamps)
> q                    # Stop monitoring (shows statistics)
```

#### Send Custom Commands

```
> c                    # Enter controller mode
> p 5                  # Send PLAY_SOUND for sound index 5
> s 3                  # Send STOP_SOUND for queue_id 3
> x                    # Send STOP_ALL
```

### Display Options

- `r` - Toggle raw hex display (on/off)
- `v` - Toggle parsed message display (on/off)
- `t` - Show statistics (RX/TX counts, errors)

## Command Reference

### Mode Control
- `m` - Monitor mode (passive sniffer)
- `a` - Audio module simulator
- `c` - Controller simulator
- `i` - Idle mode (stop current)
- `q` - Quit/stop current mode

### Controller Commands (in controller mode)
- `d` - Send MODULE_QUERY (discovery)
- `p <idx>` - Send PLAY_SOUND (sound index 0-255)
- `s <qid>` - Send STOP_SOUND (queue ID)
- `x` - Send STOP_ALL

### Audio Module Commands (in audio module mode)
- `f <qid>` - Send SOUND_FINISHED (queue ID)

### Display Options
- `r` - Toggle raw hex display
- `v` - Toggle parsed message display
- `t` - Show statistics

### General
- `h` or `?` - Show help
- `q` - Quit/stop current mode

## Example Output

### Monitor Mode
```
[MONITOR MODE - PASSIVE]
Listening to all CAN bus traffic...

[  12.345] 0x411 [8] MODULE_QUERY      | FF 00 00 00 00 00 00 00
      (Broadcast discovery query)
[  12.349] 0x410 [8] MODULE_ANNOUNCE   | 01 01 00 01 42 00 00 00
      Type: Audio Module (0x01), Ver: 1.0, Block: 0x42, Caps: 0x01
[  15.678] 0x420 [8] PLAY_SOUND        | 05 00 64 00 00 00 00 00
      Sound: 5, Vol: 100, Flags: 0x00
[  15.682] 0x423 [8] SOUND_ACK         | 05 00 03 00 00 00 00 00
      Sound: 5, Status: SUCCESS, Queue ID: 3
```

### Audio Module Simulator
```
[AUDIO MODULE SIMULATOR - ACTIVE]
Responding to discovery and sound commands...

← RX: 0x420 [8] PLAY_SOUND | 01 00 64 00 00 00 00 00
      Sound: 1, Vol: 100, Flags: 0x00
→ TX: SOUND_ACK (queue_id=1)

← RX: 0x421 [8] STOP_SOUND | 01 00 00 00 00 00 00 00
      Queue ID: 1
→ TX: STOP_ACK (queue_id=1)
```

## Testing Scenarios

### 1. Test fw-main Discovery
- Flash this firmware to DevKit #1
- Flash ots-fw-main to DevKit #2
- Connect both to CAN bus
- On test firmware: `a` (audio module mode)
- On fw-main: Should detect audio module and log "✓ Audio module v1.0 detected"
- On test firmware: See MODULE_QUERY and respond with MODULE_ANNOUNCE

### 2. Test Audio Module Protocol
- Flash this firmware to DevKit #1
- Flash ots-fw-audiomodule to DevKit #2
- Connect both to CAN bus
- On test firmware: `c` (controller mode), then `p 1` (play sound 1)
- On audio module: Should receive PLAY_SOUND and respond with ACK
- On test firmware: See SOUND_ACK and optional SOUND_FINISHED

### 3. Debug Communication Issues
- Flash this firmware to DevKit
- Connect to existing CAN bus with fw-main + fw-audiomodule
- Use `m` (monitor mode) to watch all traffic
- Identify missing messages, timing issues, or protocol violations

## Protocol Support

This firmware understands the following OTS CAN protocol messages:

### Discovery Protocol
- `0x410` MODULE_ANNOUNCE (module → main)
- `0x411` MODULE_QUERY (main → modules)

### Audio Protocol (0x420-0x42F block)
- `0x420` PLAY_SOUND (main → audio)
- `0x421` STOP_SOUND (main → audio)
- `0x422` STOP_ALL (main → audio)
- `0x423` SOUND_ACK (audio → main)
- `0x424` STOP_ACK (audio → main)
- `0x425` SOUND_FINISHED (audio → main)

## Troubleshooting

### No CAN Traffic Received
- Check CAN_H/CAN_L wiring
- Verify 120Ω termination resistors at bus ends
- Ensure all devices share common ground
- Check CAN transceiver power (3.3V or 5V depending on model)

### Mock Mode (no physical CAN)
- Firmware detects no transceiver and uses mock mode
- Messages logged to serial but not sent on physical bus
- Useful for testing without hardware

### Wrong Baud Rate
- OTS uses 500 kbps (default in can_driver)
- Verify all devices on bus use same rate

## Advanced Usage

### Custom Message Injection

The firmware can be extended to inject custom CAN messages by modifying `cli_handler.c`:

```c
// Add new command 'z' to send custom message
case 'z': {
    uint8_t data[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    can_simulator_send_custom(0x123, data, 8);
    break;
}
```

### Test Scripts

For automated testing, commands can be sent via serial. Example Python script:

```python
import serial
import time

ser = serial.Serial('/dev/ttyUSB0', 115200)
time.sleep(2)

# Enter controller mode
ser.write(b'c\n')
time.sleep(0.5)

# Send discovery query
ser.write(b'd\n')
time.sleep(1)

# Play sound 1
ser.write(b'p 1\n')
time.sleep(0.5)
```

## Contributing

When adding support for new module types:

1. Add CAN ID definitions to `can_decoder.c`
2. Add message decoder functions
3. Update `can_simulator.c` to handle new messages
4. Update this README with new commands

## See Also

- **CAN Discovery Protocol**: `/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`
- **CAN Protocol Spec**: `/prompts/CANBUS_MESSAGE_SPEC.md`
- **Developer Guide**: `/doc/developer/canbus-protocol.md`
- **CAN Driver Component**: `/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md`
