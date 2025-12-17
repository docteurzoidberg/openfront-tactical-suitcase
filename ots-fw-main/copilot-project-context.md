# OTS Firmware – Project Context

## Overview

`ots-fw-main` is an **ESP-IDF firmware** for an **ESP32-S3** dev board that acts as the hardware controller for the OpenFront.io game. The firmware connects to the game server via WebSocket and controls physical hardware modules through I2C I/O expanders.

Goals:
- Act as a **WebSocket client** connecting to `ots-server` game backend
- Use the **same protocol** as defined in `protocol-context.md`
- Provide modular hardware abstraction mirroring physical PCB modules
- Support event-driven architecture with clean separation of concerns

This firmware:
- Maintains a stable WebSocket connection to the game server
- Sends and receives messages conforming to `protocol-context.md`
- Uses hardware modules (Alert, Nuke, Main Power) with standardized interfaces
- Implements event dispatcher for flexible event routing
- Supports Over-The-Air (OTA) updates via HTTP

## Tech Stack

- Platform: **ESP32-S3** dev board
- Framework: **ESP-IDF** (native Espressif framework)
- Build System: **CMake** + ESP-IDF components
- Language: **C** for firmware logic
- Connectivity: Wi-Fi + WebSocket **client** to game server
- I/O Expansion: **MCP23017** I2C I/O expanders (2 boards)
  - Native ESP-IDF I2C master driver
  - I2C bus: SDA=GPIO8, SCL=GPIO9 (ESP32-S3 compatible)
  - Addresses: 0x20 (inputs), 0x21 (outputs)
- OTA Updates: HTTP server on port 3232 with dual-partition support

## Responsibilities

- Connect to Wi-Fi using configured credentials with automatic reconnection.
- Host a WebSocket server for the OTS backend to connect to.
- Implement the same **message envelope** (`type`, `payload`) as described in `protocol-context.md`.
- Keep the **game state** and **event notifications** consistent with the `ots-server` simulator and `ots-userscript`.
- React to incoming `state` and `event` messages by updating hardware (LEDs, outputs).
- Monitor hardware inputs (buttons, sensors) and send `cmd` messages to the server.
- Manage MCP23017 I/O expander boards for reading inputs and controlling outputs.
- Support Over-The-Air (OTA) firmware updates for remote maintenance.

## Project Layout

```txt
ots-fw-main/
  CMakeLists.txt           # ESP-IDF project configuration
  platformio.ini           # PlatformIO configuration
  partitions.csv           # OTA partition table
  src/
    main.c                 # Firmware entrypoint
    protocol.c             # Game event type conversions
    io_expander.c          # MCP23017 I/O expander driver
    module_io.c            # Global I/O board configuration
    
    # Network & Communication
    network_manager.c      # WiFi lifecycle and mDNS
    ws_client.c            # WebSocket transport layer
    ws_protocol.c          # WebSocket protocol parsing/building
    ota_manager.c          # OTA HTTP server
    
    # Event System
    event_dispatcher.c     # Centralized event routing
    game_state.c           # Game phase tracking
    
    # Hardware Control
    led_controller.c       # LED effect management (dedicated task)
    button_handler.c       # Button debouncing and event posting
    io_task.c              # Dedicated I/O scanning task
    
    # Hardware Modules
    module_manager.c       # Hardware module registration
    nuke_module.c          # Nuke module (3 buttons + 3 LEDs) - handles button->nuke logic
    alert_module.c         # Alert module (6 LEDs)
    main_power_module.c    # Main power module (link LED + connection status)
    
    CMakeLists.txt         # Component registration
  include/
    config.h               # Wi-Fi, WebSocket, OTA config
    protocol.h             # Game event types
    game_state.h           # Game phase definitions
    
    # Hardware interfaces
    hardware_module.h      # Base module interface
    module_manager.h       # Module coordination
    nuke_module.h          # Nuke module interface
    alert_module.h         # Alert module interface
    main_power_module.h    # Main power interface
    
  prompts/               # AI assistant prompts for modules
  CHANGELOG.md
  copilot-project-context.md
```

## Synchronization with `ots-server` and `ots-userscript`

- The **authoritative specification** is `protocol-context.md` at repo root
- Event types defined in `protocol.h` must stay in sync with protocol-context.md:
  - Game events: `GAME_START`, `GAME_END`, `WIN`, `LOOSE`
  - Nuke events: `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`, `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
  - Alert events: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
  - Internal events: `INTERNAL_EVENT_NETWORK_CONNECTED`, `INTERNAL_EVENT_WS_CONNECTED`, etc.
- Game phases in `game_state.h` must match ots-shared: `LOBBY`, `SPAWNING`, `IN_GAME`, `WON`, `LOST`, `ENDED`
- When adding new events:
  1. Update `protocol-context.md`
  2. Add to `protocol.h` enum
  3. Update `protocol.c` string conversions
  4. Add handlers in appropriate hardware modules
  5. Update ots-server and ots-userscript

## WebSocket Behavior (Firmware)

**Important**: The firmware acts as a **WebSocket client** connecting TO `ots-server`.

The firmware WebSocket client:

- Connects to `ots-server` WebSocket endpoint (configured in `config.h`)
- Sends handshake on connection: `{"type": "handshake", "clientType": "firmware"}`
- Receives from server:
  - `event` messages with `GameEvent` payloads
  - `cmd` messages for server-initiated commands
- Sends to server:
  - `event` messages for button presses and hardware events
- Auto-reconnects on disconnection
- Posts internal events (`INTERNAL_EVENT_WS_CONNECTED`, etc.) to event dispatcher

## Hardware I/O Architecture

### Global I/O Board Configuration

The firmware uses **2 MCP23017** I2C I/O expander chips with global pin direction configuration:

- **Board 0 (0x20)**: ALL 16 pins configured as INPUT with pullup (buttons, sensors)
- **Board 1 (0x21)**: ALL 16 pins configured as OUTPUT (LEDs, relays)

This global configuration simplifies initialization and matches the physical hardware design.

### Key Components

1. **io_expander.c** - Low-level MCP23017 driver
   - Native ESP-IDF I2C master driver
   - Per-pin and per-port operations
   - Manages 2 boards on I2C bus

2. **module_io.c** - I/O board initialization and abstraction
   - Configures entire boards (16 pins at once)
   - Provides module-specific helper functions
   - Maps logical functions to physical pins

3. **Hardware Modules** - Event-driven module implementations
   - Each module implements `hardware_module_t` interface
   - Modules handle their own events via event dispatcher
   - Self-contained: init, update, handle_event, get_status, shutdown

4. **module_manager.c** - Module coordination
   - Registers up to 8 hardware modules
   - Initializes all modules in sequence
   - Routes events to all registered modules

### Main Power Module (main_power_module.c)

**Hardware:**
- LINK LED: Board 1, Pin 7 - Connection status indicator

**Event Handling:**
- `INTERNAL_EVENT_NETWORK_CONNECTED`: Blinks link LED (500ms) - waiting for WebSocket
- `INTERNAL_EVENT_NETWORK_DISCONNECTED`: Turns off link LED
- `INTERNAL_EVENT_WS_CONNECTED`: Solid ON - fully connected
- `INTERNAL_EVENT_WS_DISCONNECTED`: Blinks (500ms) - network OK but no WS
- `INTERNAL_EVENT_WS_ERROR`: Fast blink (200ms) - error state

**LED States:**
- OFF: No network connection
- Blinking (500ms): Network connected, waiting for WebSocket
- Solid ON: Fully connected to game server
- Fast blink (200ms): Error state

### Nuke Module (nuke_module.c)

**Hardware:**

Inputs (Board 0):
- ATOM button: Pin 1 (active-low with pullup)
- HYDRO button: Pin 2 (active-low with pullup)
- MIRV button: Pin 3 (active-low with pullup)

Outputs (Board 1):
- ATOM LED: Pin 8
- HYDRO LED: Pin 9
- MIRV LED: Pin 10

**Event Handling:**
- `GAME_EVENT_NUKE_LAUNCHED`: Blinks atom LED for 10 seconds
- `GAME_EVENT_HYDRO_LAUNCHED`: Blinks hydro LED for 10 seconds
- `GAME_EVENT_MIRV_LAUNCHED`: Blinks MIRV LED for 10 seconds

**Button Flow:**
1. Button handler scans buttons every 50ms (io_task)
2. On press, posts `GAME_EVENT_*_LAUNCHED` to event dispatcher
3. Event sent to WebSocket server
4. Nuke module receives event and blinks corresponding LED

### Alert Module (alert_module.c)

**Hardware:**

Outputs (Board 1):
- WARNING LED: Pin 0 (active when any alert is present)
- ATOM LED: Pin 1
- HYDRO LED: Pin 2
- MIRV LED: Pin 3
- LAND LED: Pin 4
- NAVAL LED: Pin 5

**Event Handling:**
- `GAME_EVENT_ALERT_ATOM/HYDRO/MIRV`: Blinks corresponding LED + warning LED (10s)
- `GAME_EVENT_ALERT_LAND/NAVAL`: Blinks corresponding LED + warning LED (15s)
- `GAME_EVENT_NUKE_EXPLODED/INTERCEPTED`: Clears all nuke alerts
- `GAME_EVENT_GAME_START`: Turns on warning LED
- `GAME_EVENT_GAME_END/WIN/LOOSE`: Turns off all alerts

## Implementation Notes

### Architecture Principles
- **Modular Design**: Each physical PCB = one software module with standardized interface
- **Event-Driven**: All module communication via event dispatcher
- **Separation of Concerns**: Network, protocol, I/O, and modules are independent
- **FreeRTOS Tasks**: Dedicated tasks for LED control, I/O scanning, event dispatching

### Hardware Configuration
- **I/O Board 0 (0x20)**: All pins = INPUT with pullup (configured globally)
- **I/O Board 1 (0x21)**: All pins = OUTPUT (configured globally)
- **Button Wiring**: Connect to Board 0 pins, other side to GND (active-low)
- **LED Wiring**: Connect to Board 1 pins with appropriate current limiting

### Event System
- Game events from protocol (GAME_EVENT_*) are routed to all modules
- Internal events (INTERNAL_EVENT_*) for system status (network, WebSocket)
- Modules register handlers via event dispatcher
- Event queue size: 32 events

### Adding New Modules
1. Create `module_name.h/c` implementing `hardware_module_t` interface
2. Define pin mappings in header
3. Implement init, update, handle_event, get_status, shutdown
4. Register module in `main.c` via `module_manager_register()`
5. Add to CMakeLists.txt

### Network & WebSocket
- WiFi auto-reconnects on disconnect
- WebSocket auto-reconnects with exponential backoff
- mDNS hostname: `ots-fw-main.local`
- Connection events posted to event dispatcher for LED feedback

## Over-The-Air (OTA) Updates

ESP-IDF native OTA with HTTP upload and dual-partition support.

### Configuration

OTA settings in `include/config.h`:
- **Hostname**: `ots-fw-main` (mDNS discovery)
- **Password**: `ots2025` (not fully implemented)
- **Port**: 3232 (HTTP server)

### Partition Layout

- **Factory**: 2MB (initial firmware)
- **OTA_0**: 2MB (partition A)
- **OTA_1**: 2MB (partition B)
- **Storage**: NVS, OTA data

### Uploading Firmware

**Using curl:**
```bash
# Upload binary to OTA endpoint
curl -X POST --data-binary @firmware.bin http://ots-fw-main.local:3232/update
```

**Using ESP-IDF:**
```bash
idf.py build
curl -X POST --data-binary @build/ots-fw-main.bin http://192.168.x.x:3232/update
```

### OTA Behavior

- All module LEDs turn off during update
- LINK LED blinks showing progress (every 5%)
- Serial logs progress percentage
- Auto-reboot after successful update
- Failed updates don't brick device (dual partition safety)

### See Also

See `OTA_GUIDE.md` for detailed instructions and troubleshooting.

## When Updating This Project

### Protocol Changes

1. Update `protocol-context.md` at repo root
2. Update event types in `include/protocol.h`
3. Add string conversions in `src/protocol.c`
4. Update handlers in relevant modules
5. Update `ots-server` and `ots-userscript` implementations
6. Test end-to-end event flow

### Adding Hardware Modules

1. Create `module_name.h` with pin definitions
2. Create `module_name.c` implementing `hardware_module_t`
3. Add module to `src/CMakeLists.txt`
4. Register in `main.c` via `module_manager_register()`
5. Update this context file with module description
6. Create prompt file in `prompts/` if needed

### Major Changes

- Update `CHANGELOG.md` with version and date
- Update this context file for architecture changes
- Update module prompt files if interfaces change
- Test compilation with `idf.py build`
- Test OTA update procedure
