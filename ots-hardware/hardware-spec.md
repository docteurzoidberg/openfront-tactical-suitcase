# OTS Hardware Specification

Main controller and bus specification for the Openfront Tactical Suitcase.

## Main Controller Board

### Microcontroller
- **Model**: ESP32-S3
- **Key Features**:
  - Dual-core Xtensa LX7
  - WiFi 802.11 b/g/n
  - Sufficient GPIO for bus interfaces
  - Built-in USB for programming/debug

### Primary Functions
1. **WebSocket Server**: Hosts WS server for ots-server client connection
2. **Game State Receiver**: Receives game state updates from ots-server
3. **Bus Master**: Controls I2C and CAN bus communication with modules
4. **State Distribution**: Distributes relevant game state to each module
5. **Input Aggregation**: Collects module inputs and sends commands back

### Network Configuration
- **Mode**: WiFi Station (connects to local network) or AP mode
- **Protocol**: WebSocket server (same protocol as defined in `prompts/protocol-context.md`)
- **Port**: TBD (e.g., 8080)
- **Discovery**: mDNS/DNS-SD for auto-discovery (optional)

## Module Bus Specification

The bus connecting main controller to daughter boards contains:

### Physical Layer
- **Connector Type**: TBD (consider: JST XH, Molex, or RJ45 for robustness)
- **Pin Count**: Minimum 6 pins
  - Power (+V)
  - Ground (GND)
  - I2C SDA
  - I2C SCL
  - CAN H
  - CAN L
  - (Optional: Additional GPIO, interrupt lines)

### Power Distribution
- **Voltage Levels**: TBD - options:
  - 5V main (buck down to 3.3V on modules as needed)
  - 3.3V only (simpler, lower power)
  - 12V (for high-power modules like motors/displays)
- **Current Budget**: TBD based on module requirements
- **Protection**: Fuse/polyfuse on main board, reverse polarity protection

### I2C Bus
- **Purpose**: Configuration, low-speed telemetry, module detection
- **Speed**: 100 kHz (standard) or 400 kHz (fast mode)
- **Addressing**: 
  - Each module has unique 7-bit I2C address
  - Address range: 0x10-0x7F (avoid reserved addresses)
  - Can be set via DIP switches or hardcoded per module type
- **Pull-ups**: 4.7kΩ on main controller board

### CAN Bus
- **Purpose**: Real-time game state updates, high-priority commands
- **Speed**: 250 kbit/s or 500 kbit/s (TBD)
- **Arbitration**: Standard 11-bit identifiers
- **Message Types**:
  - Game state broadcasts (from main controller)
  - Module status updates (from modules)
  - Commands (bidirectional)
- **Termination**: 120Ω terminators at both ends of bus

## Module Standard

### Physical Form Factor
- **Unit Sizes**: TBD (examples):
  - 1U: 50mm x 50mm
  - 2U: 100mm x 50mm
  - 4U: 100mm x 100mm
- **Mounting**: Standard holes/slots for suitcase foam cutouts
- **Enclosure**: Individual module enclosures or exposed PCBs in suitcase

### Module Capabilities
Each module can:
- **Receive**: Subset of game state relevant to its domain
- **Send**: Input events/commands back to main controller
- **Configure**: Expose settings via I2C registers
- **Identify**: Report module type, version, capabilities

### Module Addressing Scheme
- **I2C Address**: Unique per physical module instance
- **CAN ID Range**: Allocated per module type
  - Base ID + offset for multi-message modules
  - Example: Module type 0x01 uses CAN IDs 0x100-0x10F

### Discovery & Initialization
- **Boot Sequence**:
  1. Main controller scans I2C bus for modules
  2. Queries each module for type/version
  3. Configures module (sets CAN filters, update rates, etc.)
  4. Enables module in active module list
- **Hot-swap**: TBD (likely not supported in v1, boot-time detection only)

## Protocol Integration

The hardware uses the same protocol defined in `/prompts/protocol-context.md` at the WebSocket layer:

### Incoming to Hardware (from ots-server)
```json
{
  "type": "state",
  "payload": { /* GameState */ }
}
```

Main controller:
1. Parses WebSocket message
2. Extracts relevant fields for each module
3. Broadcasts via CAN or sends via I2C

### Outgoing from Hardware (to ots-server)
```json
{
  "type": "cmd",
  "payload": {
    "action": "button_press",
    "params": { "button": "tactical_1" }
  }
}
```

Module → Main Controller → WebSocket → ots-server → userscript → game

## Code Generation Requirements

From this spec, generate:

### Firmware (C++)
- `ots-fw-main/include/hardware_config.h` - Pin definitions, addresses
- `ots-fw-main/include/modules/module_base.h` - Base module interface
- `ots-fw-main/src/bus_manager.cpp` - I2C/CAN communication layer

### Shared Types (TypeScript)
- `ots-shared/src/hardware/module-types.ts` - Module type enums, configs
- `ots-shared/src/hardware/commands.ts` - Hardware command types

### Server Emulator (TypeScript)
- `ots-server/server/hardware/emulator.ts` - Virtual hardware state
- `ots-server/server/hardware/module-registry.ts` - Module management

---

**Next Steps**: Define individual modules in `modules/` directory.
