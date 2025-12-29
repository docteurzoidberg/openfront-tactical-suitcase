# Openfront Tactical Suitcase (OTS Hardware)

Hardware controller for Openfront - a modular, ruggedized game controller system.

## Overview

The Openfront Tactical Suitcase is a physical hardware device that provides tactile controls and visual feedback for the Openfront game. It features a modular design where different functional modules can be added or removed based on needs.

## Architecture

### Main Controller
- **MCU**: ESP32-S3
- **Role**: Central coordinator and WebSocket server
- **Connectivity**: Hosts WebSocket server for ots-server client to connect to
- **Bus Interface**: Controls and communicates with daughter boards via shared bus

### Module Bus Specification
- **Power**: Shared power rail (voltage TBD - 5V/3.3V/12V)
- **I2C**: For low-speed module communication and configuration
- **CAN Bus**: For real-time game state updates and reliable messaging
- **Physical**: Standard connectors (type TBD)

### Module Design
- **Modular Units**: Standard physical sizes (dimensions TBD)
- **Domain-specific**: Each module handles specific aspect of game state

## Software Integration

### Firmware (ots-fw-main)
- Runs on ESP32-S3 main controller
- WebSocket **server** exposing hardware to network
- Receives game state updates from ots-server
- Distributes state to modules via I2C/CAN
- Collects input from modules and sends commands back

### Server (ots-server)
Two roles:
1. **Client mode**: Connects to real hardware WebSocket server
2. **Emulator mode**: Emulates hardware firmware for dev/debug without physical device

### UI Dashboard
- Modular component system mirroring physical modules
- Each hardware module has corresponding Vue component
- Displays same information as physical hardware

## Communication Flow

```
Game (userscript)
    ↓ WebSocket
ots-server
    ↓ WebSocket (client connects to hardware server)
Hardware (ESP32-S3) ← main controller
    ↓ I2C/CAN Bus
Modules (daughter boards)
```

## Modules

Current module specifications:

- Main Power Module: `modules/main-power-module.md`
- Nuke Module: `modules/nuke-module.md`
- Alert Module: `modules/alert-module.md`
- Troops Module: `modules/troops-module.md`
- Sound Module: `modules/sound-module.md`

Modules will be documented as they are designed. Each module specification will include:
- Physical dimensions and mounting
- I2C/CAN addressing and protocol
- Game state domain (what subset of state it handles)
- Input/output capabilities
- Firmware interface
- Vue component mockup

---

See individual prompt files for detailed specifications.
