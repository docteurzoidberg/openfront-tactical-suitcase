# Repository Overview

Complete guide to the OTS codebase structure.

## 📁 Repository Layout

```
ots/                                 # Monorepo root
├── README.md                        # Project overview
├── release.sh                       # Automated release script
├── weekly_announces.md              # Release changelog
│
├── doc/                            # 📚 Documentation
│   ├── user/                       # User guides
│   └── developer/                  # Developer docs (you are here!)
│
├── prompts/                        # 🤖 AI Assistant Context
│   ├── WEBSOCKET_MESSAGE_SPEC.md         # SOURCE OF TRUTH for WebSocket protocol
│   ├── GIT_WORKFLOW.md
│   └── RELEASE.md
│
├── .github/                        # ⚙️ GitHub Configuration
│   └── copilot-instructions.md     # Global AI assistant guidance
│
├── ots-simulator/                     # 🌐 Nuxt Dashboard
├── ots-userscript/                 # 🔧 Browser Extension
├── ots-fw-main/                    # 🎛️ Main Firmware
├── ots-fw-audiomodule/             # 🔊 Audio Firmware
├── ots-fw-cantest/                 # 🧪 CAN Testing Tool
├── ots-fw-shared/                  # 📦 Shared Firmware Components
├── ots-shared/                     # 🔗 Shared TypeScript Types
└── ots-hardware/                   # 🔩 Hardware Specs
```

## 🌐 ots-simulator (Nuxt Dashboard)

**Purpose**: Web dashboard + WebSocket server for hardware emulation and game visualization

**Tech Stack**: Nuxt 4, Vue 3, TypeScript, Tailwind CSS, nuxt-ui

### Structure

```
ots-simulator/
├── app/                            # Nuxt application
│   ├── pages/
│   │   └── index.vue               # Main dashboard page
│   ├── components/
│   │   ├── dashboard/              # Dashboard UI components
│   │   │   ├── HeaderStatus.vue    # Connection status pills
│   │   │   └── EventsList.vue      # Event log display
│   │   └── hardware/               # Hardware module emulators
│   │       ├── NukeModule.vue      # Nuke button + LED emulator
│   │       ├── AlertModule.vue     # Alert LED emulator
│   │       ├── TroopsModule.vue    # LCD + slider emulator
│   │       └── MainPowerModule.vue # Connection status
│   ├── composables/
│   │   └── useGameSocket.ts        # WebSocket client hook
│   ├── assets/css/
│   │   └── main.css                # Global styles
│   └── public/                     # Static assets
│
├── server/                         # Nuxt server (Nitro)
│   └── routes/
│       └── ws.ts                   # WebSocket server endpoint
│
├── nuxt.config.ts                  # Nuxt configuration
├── package.json                    # Dependencies
└── README.md                       # Server-specific docs
```

### Key Files

| File | Purpose |
|------|---------|
| `app/pages/index.vue` | Main dashboard with all hardware modules |
| `app/composables/useGameSocket.ts` | WebSocket connection management |
| `server/routes/ws.ts` | Unified WebSocket server (dashboard + userscript) |
| `nuxt.config.ts` | Server config, experimental WebSocket support |

### Development

```bash
npm install        # Install dependencies
npm run dev        # Dev server on http://localhost:3000
npm run build      # Production build
npm run preview    # Preview production build
```

See [Server Development Guide](server-development.md) for details.

## 🔧 ots-userscript (Browser Extension)

**Purpose**: Browser extension that monitors OpenFront.io game and sends events to hardware/dashboard

**Tech Stack**: TypeScript, esbuild, Tampermonkey API

### Structure

```
ots-userscript/
├── src/
│   ├── main.user.ts                # Entry point (Tampermonkey header)
│   ├── websocket/
│   │   └── client.ts               # WebSocket client with auto-reconnect
│   ├── game/
│   │   ├── openfront-bridge.ts     # Game API integration
│   │   └── trackers/               # Game state tracking
│   │       ├── NukeTracker.ts      # Nuclear launch detection
│   │       ├── BoatTracker.ts      # Naval invasion tracking
│   │       └── LandAttackTracker.ts # Land attack tracking
│   ├── hud/
│   │   ├── sidebar-hud.ts          # Main HUD interface
│   │   └── window.ts               # Draggable window base classes
│   └── utils/
│       └── README.md               # Utility functions
│
├── build.mjs                       # esbuild bundler
├── package.json                    # Dependencies
└── build/
    └── userscript.ots.user.js      # Built output (install in Tampermonkey)
```

### Key Features

- **100ms polling loop** - Monitors game state continuously
- **Event deduplication** - Tracks entities by ID to prevent duplicate events
- **Tabbed HUD**: Logs, Hardware, Sound tabs
- **Draggable/resizable** window with position persistence
- **Filter system** for logs (by direction and event type)
- **Sound control** toggles with remote testing

### Development

```bash
npm install        # Install dependencies
npm run build      # Build userscript
npm run watch      # Auto-rebuild on changes
```

Output: `build/userscript.ots.user.js` - Install in Tampermonkey to test.

See [Userscript Development Guide](userscript-development.md) for details.

## 🎛️ ots-fw-main (Main Controller Firmware)

**Purpose**: ESP32-S3 firmware for main hardware controller

**Tech Stack**: C, ESP-IDF, PlatformIO, FreeRTOS

### Structure

```
ots-fw-main/
├── src/                            # Source files
│   ├── main.c                      # Application entry point
│   ├── protocol.c                  # Game event type conversions
│   ├── network_manager.c           # WiFi + mDNS
│   ├── ws_server.c                 # WebSocket server (WSS support)
│   ├── ws_protocol.c               # Protocol parsing/serialization
│   ├── ota_manager.c               # OTA update server
│   ├── event_dispatcher.c          # Central event routing
│   ├── game_state_manager.c        # Game phase tracking
│   ├── nuke_state_manager.c        # Nuke tracking (up to 32 concurrent)
│   ├── module_manager.c            # Hardware module lifecycle
│   ├── io_expander.c               # MCP23017 I/O driver
│   ├── i2c_handler.c               # Shared I2C bus
│   ├── led_handler.c               # LED effect management
│   ├── button_handler.c            # Button debouncing
│   ├── rgb_handler.c               # Onboard RGB LED status
│   ├── serial_command_handler.c    # WiFi provisioning via serial
│   │
│   ├── *_module.c                  # Hardware modules
│   │   ├── nuke_module.c           # Nuke buttons + LEDs
│   │   ├── alert_module.c          # Alert LEDs
│   │   ├── troops_module.c         # LCD + slider
│   │   └── main_power_module.c     # Connection status LED
│   │
│   └── tests/                      # Hardware test firmwares
│       ├── test_i2c.c
│       ├── test_outputs.c
│       └── ...
│
├── include/                        # Header files
│   ├── config.h                    # WiFi, WebSocket, OTA config
│   ├── protocol.h                  # Game event types (matches TS)
│   ├── hardware_module.h           # Module interface definition
│   └── *.h                         # Module headers
│
├── components/                     # Hardware driver components
│   ├── mcp23017_driver/            # I/O expander driver
│   ├── ads1015_driver/             # 12-bit ADC driver
│   ├── hd44780_pcf8574/            # LCD driver (via I2C backpack)
│   ├── ws2812_rmt/                 # RGB LED driver
│   └── esp_http_server_core/       # HTTP server utilities
│
├── platformio.ini                  # Build configuration
├── partitions.csv                  # OTA partition table
├── CMakeLists.txt                  # ESP-IDF build config
└── docs/                          # Firmware-specific docs
    └── ...
```

### Module System

All hardware modules implement the `hardware_module_t` interface:

```c
typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    void (*update)(void);
    void (*handle_event)(game_event_type_t type, const char *data);
    esp_err_t (*get_status)(char *buffer, size_t len);
    void (*shutdown)(void);
} hardware_module_t;
```

Modules are event-driven and self-contained.

### Build Environments

```bash
# Main firmware
pio run -e esp32-s3-dev

# Hardware tests (standalone firmwares)
pio run -e test-i2c          # I2C bus scan
pio run -e test-outputs      # LED outputs
pio run -e test-inputs       # Button inputs
pio run -e test-lcd          # LCD display
pio run -e test-websocket    # WebSocket only
```

See [Firmware Development Guide](firmware-development.md) for details.

## 🔊 ots-fw-audiomodule (Audio Module)

**Purpose**: ESP32-A1S audio playback module with CAN bus integration

**Tech Stack**: C, ESP-IDF, ES8388 codec, CAN/TWAI driver

### Structure

```
ots-fw-audiomodule/
├── src/
│   ├── main.c                      # Audio app entry
│   ├── can_audio_handler.c         # CAN protocol handler
│   ├── audio_mixer.c               # 8-channel mixer
│   ├── sound_queue.c               # Sound command queue
│   └── ...
├── sounds/                         # Embedded WAV files
└── platformio.ini
```

### Features

- **ES8388 codec** for high-quality audio
- **SD card support** for custom sounds
- **8-channel mixer** for simultaneous sounds
- **CAN bus** communication with main controller
- **Queue management** for sound commands

See [Audio Module Documentation](../../ots-fw-audiomodule/README.md).

## 🧪 ots-fw-cantest (CAN Testing Tool)

**Purpose**: Interactive CAN bus testing and debugging firmware

**Tech Stack**: C, ESP-IDF, CAN/TWAI driver

### Operating Modes

- **Monitor mode** (`m`) - Passive bus sniffer with protocol decoder
- **Audio simulator** (`a`) - Simulates audio module responses
- **Controller simulator** (`c`) - Manual command sending
- **Interactive CLI** - Single-key commands, real-time stats

See [CAN Test README](../../ots-fw-cantest/README.md) for usage.

## 📦 ots-fw-shared (Shared Firmware Components)

**Purpose**: Reusable ESP-IDF components for all firmware projects

### Components

```
ots-fw-shared/components/
├── can_driver/                     # Generic CAN/TWAI driver
│   ├── can_driver.h
│   ├── can_driver.c
│   └── COMPONENT_PROMPT.md
│
├── can_discovery/                  # Module discovery protocol
│   ├── can_discovery.h
│   └── can_discovery.c
│
└── can_audiomodule/                # Audio module CAN protocol
    ├── can_audiomodule.h
    └── can_audiomodule.c
```

### Usage

Reference in project `CMakeLists.txt`:

```cmake
list(APPEND EXTRA_COMPONENT_DIRS "../ots-fw-shared/components")
```

See [Shared Components Documentation](architecture/shared-components.md).

## 🔗 ots-shared (Shared TypeScript Types)

**Purpose**: Shared TypeScript types for server and userscript

### Structure

```
ots-shared/
├── src/
│   ├── game.ts                     # Game event types, protocol
│   └── index.ts                    # Exports
├── package.json
└── tsconfig.json
```

### Key Types

```typescript
// Event types (matches protocol.h in firmware)
enum GameEventType {
  GAME_START, GAME_END, WIN, LOOSE,
  NUKE_LAUNCHED, HYDRO_LAUNCHED, MIRV_LAUNCHED,
  ALERT_ATOM, ALERT_HYDRO, ALERT_MIRV,
  ALERT_LAND, ALERT_NAVAL,
  NUKE_EXPLODED, NUKE_INTERCEPTED,
  SOUND_PLAY, INFO, HARDWARE_TEST, HARDWARE_DIAGNOSTIC
}

// Message envelopes
interface IncomingMessage { type: 'event' | 'cmd'; payload: ... }
interface OutgoingMessage { type: 'event' | 'cmd'; payload: ... }
```

**IMPORTANT**: These types must stay in sync with `prompts/WEBSOCKET_MESSAGE_SPEC.md` and `ots-fw-main/include/protocol.h`.

## 🔩 ots-hardware (Hardware Specifications)

**Purpose**: Hardware module specs, PCB designs, schematics

### Structure

```
ots-hardware/
├── README.md                       # Hardware overview
├── hardware-spec.md                # Main controller spec
├── module-template.md              # Template for new modules
├── DISPLAY_SCREENS_SPEC.md         # LCD screen specifications
│
├── modules/                        # Module specifications
│   ├── nuke-module.md
│   ├── alert-module.md
│   ├── troops-module.md
│   └── ...
│
├── pcbs/                           # PCB designs
│   └── (KiCad files, Gerbers, etc.)
│
└── cad/                           # Mechanical designs
    └── (STL files, etc.)
```

These specs drive firmware and UI implementation.

## 🤖 prompts/ (AI Assistant Context)

**Purpose**: AI assistant prompts and protocol specifications

### Key Files

| File | Purpose |
|------|---------|
| `WEBSOCKET_MESSAGE_SPEC.md` | **SOURCE OF TRUTH** for WebSocket protocol (1155 lines) |

| `GIT_WORKFLOW.md` | Git branching and commit conventions |
| `RELEASE.md` | Release process and version management |

**CRITICAL**: Always update `WEBSOCKET_MESSAGE_SPEC.md` first when changing protocol.

## 🔄 Data Flow

### Game → Hardware

```
OpenFront.io Game
    ↓ (Userscript polls 100ms)
Browser Userscript (trackers detect events)
    ↓ (WebSocket: type: 'event')
Firmware WebSocket Server OR Dashboard
    ↓ (Event dispatcher)
Hardware Modules (LEDs, LCD, sounds)
```

### Hardware → Game

```
Hardware Button Press
    ↓ (Button handler)
Module Event (NUKE_LAUNCHED)
    ↓ (Event dispatcher)
WebSocket Server (broadcast to clients)
    ↓ (WebSocket: type: 'cmd')
Userscript (receives command)
    ↓ (Ghost structure API)
Game (places nuke on map)
```

## 📝 Configuration Files

### Root Level

- `.gitignore` - Git exclusions
- `release.sh` - Automated versioning and release
- `weekly_announces.md` - Release changelog

### Per-Project

- `package.json` - Node.js projects (server, userscript, shared)
- `platformio.ini` - Firmware projects (fw-main, fw-audiomodule, fw-cantest)
- `CMakeLists.txt` - ESP-IDF build configuration
- `tsconfig.json` - TypeScript configuration
- `nuxt.config.ts` - Nuxt-specific config

### AI Assistant Context

- `.github/copilot-instructions.md` - Global workspace guidance
- `ots-*/copilot-project-context.md` - Per-project context files
- `ots-fw-main/prompts/*.md` - Module-specific prompts

## 🚀 Quick Reference

### Build All Projects

```bash
# Server
cd ots-simulator && npm install && npm run build

# Userscript
cd ots-userscript && npm install && npm run build

# Shared types
cd ots-shared && npm install && npm run build

# Main firmware
cd ots-fw-main && pio run -e esp32-s3-dev

# Audio module
cd ots-fw-audiomodule && pio run -e esp32-a1s-espidf

# CAN test tool
cd ots-fw-cantest && pio run -e esp32-s3-devkit
```

### Common Workflows

| Task | Command |
|------|---------|
| Start dashboard dev server | `cd ots-simulator && npm run dev` |
| Build userscript | `cd ots-userscript && npm run build` |
| Flash firmware | `cd ots-fw-main && pio run -e esp32-s3-dev -t upload` |
| Monitor serial | `pio device monitor` |
| Run hardware test | `pio run -e test-i2c -t upload && pio device monitor` |
| Create release | `./release.sh -u -p -m "Description"` |

## 📖 Next Steps

- **[Firmware Development](firmware-development.md)** - Build ESP32 firmware
- **[Server Development](server-development.md)** - Nuxt dashboard
- **[Userscript Development](userscript-development.md)** - Browser extension
- **[Architecture](architecture/)** - System design deep dive

---

**Last Updated**: January 2026  
**Repository Structure**: 8 projects, 4 firmware, 3 software, 1 specs
