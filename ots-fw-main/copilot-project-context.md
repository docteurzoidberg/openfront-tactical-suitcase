# OTS Firmware – Project Context

## Overview

`ots-fw-main` is a **PlatformIO-based firmware** for an **ESP32-S3** dev board that emulates the OTS device controller for the OpenFront.io game.

Goals:
- Connect to the same WebSocket server and use the **same protocol** as `ots-server` / `ots-userscript`.
- Reuse the **shared protocol description** so that message types and semantics stay in sync.
- Allow rapid iteration on game logic by keeping device behavior and `ots-server` behavior aligned.

This firmware should:
- Maintain a stable WebSocket connection to the OTS backend.
- Send and receive messages that conform to the protocol defined in `protocol-context.md` at the repo root.
- Mirror the logical behavior of the server-side OTS simulator where appropriate (e.g., state updates, event emission, command handling).

## Tech Stack

- Platform: **ESP32-S3** dev board
- Framework: **PlatformIO** with Arduino framework
- Language: **C++** (Arduino-style) for firmware logic
- Connectivity: Wi-Fi + WebSocket server running on-device
- I/O Expansion: **MCP23017** I2C I/O expanders (up to 8 boards supported)
  - Library: Adafruit MCP23017 Arduino Library
  - I2C bus: SDA=GPIO21, SCL=GPIO22, 100kHz
  - Default addresses: 0x20, 0x21 (configurable in `config.h`)

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
  platformio.ini           # PlatformIO configuration for ESP32-S3 board
  src/
    main.cpp               # Firmware entrypoint
    ws_client.cpp          # WebSocket server implementation
    protocol.cpp           # Protocol message parsing/serialization
    io_expander.cpp        # MCP23017 I/O expander driver
    module_io.cpp          # Module-specific I/O abstractions
  include/
    ws_client.h            # WebSocket server interface
    protocol.h             # C++ representation of shared protocol structures
    io_expander.h          # IOExpander class for MCP23017 management
    module_io.h            # ModuleIO class with pin mappings
    config.h               # Wi-Fi credentials, server URL, I2C pins, MCP addresses
  copilot-project-context.md
```

> NOTE: The exact set of files may evolve. Copilot should keep this file updated when structural changes are introduced.

## Synchronization with `ots-server` and `ots-userscript`

- The **authoritative specification** for messages is `protocol-context.md` at the repo root.
- Current protocol uses hardware-focused `GameState` with nested `hwState` containing:
  - `m_general`: General module state (link status)
  - `m_alert`: Alert module state (warning, atom, hydro, mirv, land, naval)
  - `m_nuke`: Nuke module state (nuke, hydro, mirv launched)
- Event types include hardware-specific events: `NUKE_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`, alert types, plus `INFO`, `WIN`, `LOOSE`
- When adding new commands, events, or state fields:
  - Update `protocol-context.md`.
  - Then update both:
    - The server/clients in `ots-server` and `ots-userscript`.
    - The firmware structures and handlers in `ots-fw-main`.

## WebSocket Behavior (Firmware)

**Important**: The firmware acts as a **WebSocket server**, not a client. The `ots-server` connects TO the firmware.

The firmware WebSocket server should:

- Host a WebSocket server on the ESP32-S3 for the OTS backend to connect to.
- Accept connections from `ots-server` (which acts as WebSocket client to hardware).
- Receive from server:
  - `state` messages with `GameState` payloads (containing game state and hwState).
  - `event` messages with `GameEvent` payloads.
- Send to server:
  - `cmd` messages from hardware modules (button presses, sensor inputs, etc.).
  - `event` messages for hardware-specific events (errors, status changes).

## Hardware I/O Architecture

### MCP23017 I/O Expander Boards

The firmware uses **MCP23017** I2C I/O expander chips to interface with physical buttons and LEDs. This allows the ESP32-S3 to control many more I/O pins than natively available.

**Key components:**

1. **IOExpander class** (`io_expander.h/cpp`)
   - Low-level driver for MCP23017 chips
   - Manages multiple boards (up to 8) on the I2C bus
   - Provides per-pin digital I/O operations
   - Configurable I2C addresses (default: 0x20, 0x21)

2. **ModuleIO class** (`module_io.h/cpp`)
   - High-level abstraction for module-specific I/O
   - Uses hardware-agnostic pin mappings (PinMap struct)
   - Batch operations for reading/writing module states
   - LED state tracking and toggle support

3. **Pin Mappings** (in `module_io.h`)
   - Namespaced pin definitions per module (MainModule, NukeModule, AlertModule, etc.)
   - Each PinMap specifies board index + pin number
   - Centralized location for hardware wiring configuration

### Main Power Module Hardware

The Main Power Module provides power distribution and connection status indication:

**Physical Components (Not Firmware Controlled):**
- Global Power Switch: Hardware on/off switch that provides 12V to entire suitcase
- POWER LED: Directly connected to 12V rail, illuminates when power is ON

**Outputs (Firmware Controlled):**
- LINK LED: Board 0, Pin 0 - WebSocket connection status indicator

**Workflow:**
1. User turns power switch ON → 12V provided to all modules
2. ESP32 boots and connects to WiFi
3. Firmware establishes WebSocket connection to game server
4. LINK LED turns ON when connection is active
5. LINK LED turns OFF when connection is lost

### Nuke Module Hardware

The Nuke Module uses the following I/O:

**Inputs (Buttons):**
- ATOM button: Board 0, Pin 1 (configured with internal pull-up, active-low)
- HYDRO button: Board 0, Pin 2 (configured with internal pull-up, active-low)
- MIRV button: Board 0, Pin 3 (configured with internal pull-up, active-low)

**Outputs (LEDs):**
- ATOM LED: Board 0, Pin 8
- HYDRO LED: Board 0, Pin 9
- MIRV LED: Board 0, Pin 10

**Button Configuration:**
- All button inputs use MCP23017 internal pull-up resistors (100kΩ)
- Buttons are active-low: pressing button connects pin to GND
- Firmware automatically configures INPUT_PULLUP mode
- Debouncing: 50ms polling interval in main loop

**Workflow:**
1. Firmware polls button states via `ModuleIO::readNukeButtons()`
2. On button press, firmware sends `cmd` message to server
3. Server processes command and sends back `state` update
4. Firmware updates LED states via `ModuleIO::writeNukeLEDs()`

### Alert Module Hardware

Alert Module LED outputs on Board 1:
- WARNING LED: Board 1, Pin 0
- ATOM LED: Board 1, Pin 1
- HYDRO LED: Board 1, Pin 2
- MIRV LED: Board 1, Pin 3
- LAND LED: Board 1, Pin 4
- NAVAL LED: Board 1, Pin 5

## Implementation Notes

- Prefer small, testable modules (e.g., separate WebSocket, protocol, I/O, and game logic).
- Avoid blocking loops where possible; use `loop()` to drive I/O polling, reconnection, and heartbeat logic.
- Keep message parsing and construction centralized in `protocol.h/.cpp` so protocol changes are easy to propagate.
- Pin mappings are defined in `module_io.h` - update this file when wiring changes.
- MCP23017 addresses are configured in `config.h` - adjust for hardware setup.
- All button inputs are automatically configured with internal pull-ups (INPUT_PULLUP mode).
- Button wiring: Connect one side to MCP23017 pin, other side to GND (active-low).
- WiFi automatically reconnects if connection is lost (checked every 5 seconds).
- OTA updates are enabled - use PlatformIO or Arduino IDE to upload remotely.

## Over-The-Air (OTA) Updates

The firmware supports OTA updates for remote maintenance without physical access to the device.

### Configuration

OTA settings are defined in `include/config.h`:
- **Hostname**: `ots-fw-main` (mDNS name for network discovery)
- **Password**: `ots2025` (change this for security!)
- **Port**: 3232 (default Arduino OTA port)

### Uploading via OTA

**Using PlatformIO:**
```bash
# Upload firmware over network
pio run -t upload --upload-port ots-fw-main.local

# Or use IP address directly
pio run -t upload --upload-port 192.168.x.x
```

**Using Arduino IDE:**
1. Tools → Port → Select "ots-fw-main at 192.168.x.x"
2. Sketch → Upload
3. Enter password when prompted: `ots2025`

### OTA Behavior

During OTA update:
- All LEDs turn off
- LINK LED blinks to indicate upload progress
- Serial output shows progress percentage
- Device automatically reboots after successful update

On OTA error:
- LINK LED rapid blinks (10 times)
- Error message printed to serial
- Device continues normal operation

### Security Recommendations

1. **Change the default OTA password** in `config.h`
2. Only enable OTA on trusted networks
3. Use strong WiFi encryption (WPA2/WPA3)
4. Consider adding IP filtering if needed

### Troubleshooting OTA

**Cannot find device:**
- Ensure device and computer are on same network
- Check mDNS is working: `ping ots-fw-main.local`
- Try using IP address instead of hostname
- Verify WiFi is connected (LINK LED should be ON)

**Authentication failed:**
- Check OTA_PASSWORD in `config.h` matches upload password
- Recompile and upload via USB if password was changed

**Upload fails midway:**
- Check WiFi signal strength
- Ensure sufficient free heap memory
- Verify firmware size is within flash limits

## When Updating This Project

When making changes that affect the protocol or behavior shared with `ots-server` / `ots-userscript`:

1. Update `protocol-context.md` at repo root.
2. Update corresponding implementations in:
   - `ots-server` (Nuxt server routes and client composables).
   - `ots-userscript` (userscript WebSocket bridge).
   - `ots-fw-main` (firmware WebSocket client and protocol structs).
3. Reflect major changes in this file so Copilot understands the updated expectations.
