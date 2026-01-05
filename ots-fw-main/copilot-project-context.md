# OTS Firmware – Project Context

## Overview

`ots-fw-main` is an **ESP-IDF firmware** for an **ESP32-S3** dev board that acts as the hardware controller for the OpenFront.io game. The firmware connects to the game server via WebSocket and controls physical hardware modules through I2C I/O expanders.

Goals:
- Act as a **WebSocket client** connecting to `ots-server` game backend
- Use the **same protocol** as defined in `prompts/protocol-context.md`
- Provide modular hardware abstraction mirroring physical PCB modules
- Support event-driven architecture with clean separation of concerns

This firmware:
- Maintains a stable WebSocket connection to the game server
- Sends and receives messages conforming to `prompts/protocol-context.md`
- Uses hardware modules (Alert, Nuke, Main Power) with standardized interfaces
- Implements event dispatcher for flexible event routing
- Supports Over-The-Air (OTA) updates via HTTP

## Prompt Files

Firmware prompt index: `prompts/PROMPTS_INDEX.md`

## Tech Stack

- Platform: **ESP32-S3** dev board
- Framework: **ESP-IDF** (native Espressif framework)
- Build System: **PlatformIO** (preferred) or **CMake** + ESP-IDF components
- Language: **C** for firmware logic
- Connectivity: Wi-Fi + WebSocket **client** to game server
- I/O Expansion: **MCP23017** I2C I/O expanders (2 boards)
  - Native ESP-IDF I2C master driver
  - I2C bus: SDA=GPIO8, SCL=GPIO9 (ESP32-S3 compatible)
  - Addresses: 0x20 (inputs), 0x21 (outputs)
- OTA Updates: HTTP server on port 3232 with dual-partition support

## Building the Firmware

## Build Tooling Notes (AI Reminder)

**CRITICAL BUILD COMMANDS:**
- **Main firmware:** ALWAYS use `pio run -e esp32-s3-dev`
- **Hardware tests:** ALWAYS use `pio run -e test-<name>` (e.g., `test-i2c`, `test-outputs`)
- **NEVER** run `pio run` without the `-e` flag - it will fail or use wrong environment

**Build validation:**
- Ignore this non-blocking warning: `esp_idf_size: error: unrecognized arguments: --ng`. Do not spend time debugging it unless the user explicitly asks.
- When validating builds, treat success as: compile + link + `pio run -e <env>` succeeds (even if `esp_idf_size` prints that warning).
- Test selection must be driven by PlatformIO `build_flags` (`-DTEST_*`) first, with fallbacks only when not provided.
- Keep terminal commands targeted (one env at a time) and run the smallest build needed to verify a change.

**Available environments:**
- `esp32-s3-dev` - Main production firmware (default)
- `test-i2c` - I2C bus scan test
- `test-outputs` - Output board LED test
- `test-inputs` - Input board button test
- `test-adc` - ADC slider test
- `test-lcd` - LCD display test
- `test-websocket` - WebSocket connection test

### Using PlatformIO (Recommended)

PlatformIO provides a simpler build experience with automatic dependency management.

**⚠️ CRITICAL: Always specify environment with `-e esp32-s3-dev` flag!**

**Build main firmware:**
```bash
cd ots-fw-main
pio run -e esp32-s3-dev
```

**Clean build:**
```bash
pio run -e esp32-s3-dev -t clean
pio run -e esp32-s3-dev
```

**Upload to device:**
```bash
pio run -e esp32-s3-dev -t upload
```

**Monitor serial output:**
```bash
pio device monitor
```

**Build and upload (recommended):**
```bash
pio run -e esp32-s3-dev -t upload && pio device monitor
```

**Common mistake:**
```bash
# ❌ WRONG - Do not run without -e flag
pio run

# ✅ CORRECT - Always specify environment
pio run -e esp32-s3-dev
```

**Configuration:**
- Edit `platformio.ini` for board settings
- Default environment: `esp32-s3-dev` (main firmware)
- Test environments: `test-i2c`, `test-outputs`, `test-inputs`, `test-adc`, `test-lcd`, `test-websocket`
- Build output: `.pio/build/esp32-s3-dev/firmware.bin`

### Using ESP-IDF (Alternative)

For advanced ESP-IDF features or direct toolchain access.

**Build firmware:**
```bash
idf.py build
```

**Flash and monitor:**
```bash
idf.py flash monitor
```

**Configuration:**
```bash
idf.py menuconfig
```

**Build output:**
- Firmware: `build/ots-fw-main.bin`
- Bootloader: `build/bootloader/bootloader.bin`
- Partitions: `build/partitions.bin`

## Responsibilities

- Connect to Wi-Fi using configured credentials with automatic reconnection.
- Act as a **WebSocket client** connecting TO `ots-server` game backend.
- Implement the same **message envelope** (`type`, `payload`) as described in `prompts/protocol-context.md`.
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
    ws_server.c            # WebSocket server (dashboard/userscript connect to firmware)
    ws_protocol.c          # WebSocket protocol parsing/building
    ota_manager.c          # OTA HTTP server
    
    # Event System
    event_dispatcher.c     # Centralized event routing
    game_state_manager.c   # Game phase tracking
    
    # Hardware Control
    led_handler.c          # LED effect management (dedicated task)
    button_handler.c       # Button debouncing and event posting
    io_task.c              # Dedicated I/O scanning task
    
    # I2C Device Drivers (Shared)
    lcd_driver.c           # HD44780 LCD via PCF8574 I2C backpack (0x27)
    adc_driver.c           # ADS1015 12-bit ADC (0x48)
    
    # Hardware Modules
    module_manager.c       # Hardware module registration
    nuke_module.c          # Nuke module (3 buttons + 3 LEDs) - handles button->nuke logic
    alert_module.c         # Alert module (6 LEDs)
    main_power_module.c    # Main power module (link LED + connection status)
    troops_module.c        # Troops module (LCD + slider) - troop count display and deployment control
    
    CMakeLists.txt         # Component registration
  include/
    config.h               # Wi-Fi, WebSocket, OTA config
    protocol.h             # Game event types
    game_state.h           # Game phase definitions
    
    # I2C Device Drivers
    lcd_driver.h           # LCD driver interface
    adc_driver.h           # ADC driver interface
    
    # Hardware interfaces
    hardware_module.h      # Base module interface
    module_manager.h       # Module coordination
    nuke_module.h          # Nuke module interface
    alert_module.h         # Alert module interface
    main_power_module.h    # Main power interface
    troops_module.h        # Troops module interface
    
  prompts/               # AI assistant prompts for modules
  CHANGELOG.md
  copilot-project-context.md
```

## Synchronization with `ots-server` and `ots-userscript`

- The **authoritative specification** is `prompts/protocol-context.md` at repo root
- Event types defined in `protocol.h` must stay in sync with prompts/protocol-context.md:
  - Game events: `GAME_START`, `GAME_END`, `WIN`, `LOOSE`
  - Nuke events: `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`, `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
  - Alert events: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
  - Internal events: `INTERNAL_EVENT_NETWORK_CONNECTED`, `INTERNAL_EVENT_WS_CONNECTED`, etc.
- Game phases in `game_state.h` must match ots-shared: `LOBBY`, `SPAWNING`, `IN_GAME`, `WON`, `LOST`, `ENDED`
- When adding new events:
  1. Update `prompts/protocol-context.md`
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

### Troops Module (troops_module.c)

**Hardware:**

- **LCD Display**: 16x2 I2C LCD (HD44780 via PCF8574 backpack) at 0x27
  - Uses shared `lcd_driver.c/h` for hardware abstraction
- **ADC**: ADS1015 12-bit I2C ADC at 0x48, reading slider on AIN0 channel
  - Uses shared `adc_driver.c/h` for hardware abstraction
- **I2C Bus**: Shared with MCP23017 expanders (GPIO8 SDA, GPIO9 SCL)

**Event Handling:**
- Receives troop count updates (current/max) from server via game state events
- Parses JSON data: `{"troops": {"current": 120000, "max": 1100000}}`
- Sends `set-troops-percent` commands when slider changes ≥1%
- Polls slider every 100ms via ADS1015 ADC

**Display Format:**
- Line 1: `"120K / 1.1M"` (current / max with K/M/B scaling, right-aligned)
- Line 2: `"50% (60K)"` (slider percent + calculated troops, left-aligned)

**Architecture:**
- Module focuses on business logic (protocol, display formatting, state management)
- Hardware drivers (`lcd_driver`, `adc_driver`) provide shared I2C device abstraction
- Clean separation allows other modules to reuse LCD/ADC drivers if needed

## Implementation Notes

### Architecture Principles
- **Modular Design**: Each physical PCB = one software module with standardized interface
- **Event-Driven**: All module communication via event dispatcher
- **Separation of Concerns**: Network, protocol, I/O, and modules are independent
- **Shared Drivers**: I2C device drivers (LCD, ADC) abstracted for reuse across modules
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

See `docs/OTA_GUIDE.md` for detailed instructions and troubleshooting.

## When Updating This Project

### Protocol Changes

1. Update `prompts/protocol-context.md` at repo root
2. Update event types in `include/protocol.h`
3. Add string conversions in `src/protocol.c`
4. Update handlers in relevant modules
5. Update `ots-server` and `ots-userscript` implementations
6. Test end-to-end event flow

### LCD Display Changes

1. Update display logic in `src/system_status_module.c` or `src/troops_module.c`
2. Update `/ots-hardware/DISPLAY_SCREENS_SPEC.md` with new screen definitions
3. Include exact text formatting, spacing, and character positions
4. Document state transitions and event triggers
5. Update `ots-server` emulator to match firmware behavior
6. Test display on physical hardware and verify spec accuracy

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
- Test compilation with `pio run -e esp32-s3-dev` (ALWAYS use -e flag!)
- Test OTA update procedure
- Verify firmware size fits in partition (max ~2MB per partition)

## CAN Protocol Documentation (Audio Module Integration)

**When implementing CAN communication with audio module**:
1. ⚠️ **Reference `/ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md` for message formats**
2. Use the **shared component**: `/ots-fw-shared/components/can_audiomodule/`
3. Use queue IDs (1-255) to track and control individual sounds
4. Implement retry logic for mixer-full errors (wait 500ms, retry once)
5. Set 200ms timeout for ACK responses
6. **Update shared component if protocol needs change**

**Protocol Documentation:**
- `/ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md` - Audio module protocol specification
- `/ots-fw-shared/prompts/CAN_PROTOCOL_INTEGRATION.md` - Integration guide with examples
- `/ots-fw-shared/components/can_audiomodule/COMPONENT_PROMPT.md` - Shared component usage
- `/ots-fw-shared/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md` - Multi-module architecture (future)

## Hardware Driver Components

All hardware-specific logic is isolated into **independent ESP-IDF components**. For detailed information:

- **Architecture Overview**: [docs/COMPONENTS_ARCHITECTURE.md](docs/COMPONENTS_ARCHITECTURE.md)
  - Component types and structure
  - Integration patterns
  - Creating new components

- **Individual Component Documentation**:
  - **CAN Driver**: [/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md](/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md)
    - Generic CAN bus (TWAI) driver with mock fallback
    - Used by sound_module.c
    - **Protocol docs**: See "CAN Protocol Documentation" section above
  - **MCP23017 Driver**: [components/mcp23017_driver/COMPONENT_PROMPT.md](components/mcp23017_driver/COMPONENT_PROMPT.md)
    - I2C 16-pin I/O expander for buttons and LEDs
    - Used by all hardware modules via io_expander.c
  - **ADS1015 Driver**: [components/ads1015_driver/COMPONENT_PROMPT.md](components/ads1015_driver/COMPONENT_PROMPT.md)
    - 12-bit I2C ADC for analog input (slider)
    - Used by troops_module.c
  - **HD44780 LCD Driver**: [components/hd44780_pcf8574/COMPONENT_PROMPT.md](components/hd44780_pcf8574/COMPONENT_PROMPT.md)
    - I2C 16x2 character LCD display
    - Used by troops_module.c
  - **WS2812 Driver**: [components/ws2812_rmt/COMPONENT_PROMPT.md](components/ws2812_rmt/COMPONENT_PROMPT.md)
    - RGB LED strip driver using RMT peripheral
    - Reserved for future lighting module
