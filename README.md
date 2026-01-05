# OpenFront Tactical Suitcase (OTS)

**Physical tactical controller for [OpenFront.io](https://openfront.io)** - Transform your browser-based strategy game into a hands-on hardware experience with buttons, LEDs, displays, and real-time alerts.

[![ESP32-S3](https://img.shields.io/badge/ESP32-S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Nuxt](https://img.shields.io/badge/Nuxt-4-00DC82.svg)](https://nuxt.com/)
[![TypeScript](https://img.shields.io/badge/TypeScript-5-3178C6.svg)](https://www.typescriptlang.org/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP--IDF-orange.svg)](https://platformio.org/)

## 🎮 What is OTS?

OTS bridges the gap between digital gameplay and physical interaction, providing:

- **🚀 Nuke Control Panel** - Launch atomic, hydrogen, and MIRV strikes with physical buttons
- **⚠️ Alert Module** - LED indicators for incoming threats (nukes, land/naval invasions)
- **👥 Troops Module** - Live troop counter with LCD display and deployment slider
- **🔊 Audio Module** - Sound effects and ambient audio via ESP32-A1S
- **📊 Dashboard** - Real-time hardware emulator and game state visualization

## 📚 Documentation

### 👤 For Device Owners
**→ [User Documentation](doc/user/)** - Setup, usage, and troubleshooting
- [Quick Start Guide](doc/user/quick-start.md) - Get running in 10 minutes
- [WiFi Setup](doc/user/wifi-setup.md) - Connect your device
- [Userscript Installation](doc/user/userscript-install.md) - Browser extension setup

### 🛠️ For Developers
**→ [Developer Documentation](doc/developer/)** - Development, architecture, and contributing
- [Getting Started](doc/developer/getting-started.md) - Dev environment setup
- [Repository Overview](doc/developer/repository-overview.md) - Codebase structure
- [Architecture](doc/developer/architecture/) - System design and protocols

## 🚀 Quick Start

### Device Owner Setup

1. **Power on your OTS device**
2. **Connect to WiFi** (see [WiFi Setup Guide](doc/user/wifi-setup.md))
3. **Install userscript** (see [Installation Guide](doc/user/userscript-install.md))
4. **Start playing!** Launch nukes, monitor alerts, deploy troops

### Developer Setup

```bash
# Clone repository
git clone https://github.com/yourusername/ots.git
cd ots

# Install server dependencies
cd ots-server
npm install
npm run dev  # http://localhost:3000

# Build userscript
cd ../ots-userscript
npm install
npm run build  # Output: build/userscript.ots.user.js

# Build firmware (requires PlatformIO)
cd ../ots-fw-main
pio run -e esp32-s3-dev -t upload
```

See [Developer Getting Started](doc/developer/getting-started.md) for complete setup instructions.

## 📦 Repository Structure

### Core Components

| Directory | Description | Tech Stack |
|-----------|-------------|------------|
| **ots-fw-main/** | Main ESP32-S3 controller firmware | C, ESP-IDF, PlatformIO |
| **ots-fw-audiomodule/** | ESP32-A1S audio playback module | C, ESP-IDF, CAN bus |
| **ots-fw-cantest/** | CAN bus testing and debugging tool | C, ESP-IDF |
| **ots-fw-shared/** | Shared ESP-IDF components (CAN drivers) | C, ESP-IDF |
| **ots-server/** | Nuxt dashboard + WebSocket server | Vue 3, Nuxt 4, TypeScript |
| **ots-userscript/** | Browser extension (Tampermonkey) | TypeScript, esbuild |
| **ots-shared/** | Shared TypeScript protocol types | TypeScript |
| **ots-hardware/** | Hardware specs, PCB designs, modules | Markdown, CAD |

### Documentation & Configuration

| Directory | Description |
|-----------|-------------|
| **doc/** | Complete user and developer documentation |
| **prompts/** | AI assistant prompts and protocol specs |
| **.github/** | GitHub workflows and Copilot instructions |

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   OpenFront.io Game                     │
│              (Browser-based Strategy Game)              │
└────────────────────────┬────────────────────────────────┘
                         │
                         │ Polls every 100ms
                         ↓
         ┌───────────────────────────────────┐
         │    Browser Userscript (HUD)       │
         │  - Game state detection           │
         │  - Event tracking                 │
         │  - WebSocket client               │
         └───────────────┬───────────────────┘
                         │
                         │ WebSocket Protocol
                         ↓
    ┌────────────────────────────────────────────┐
    │        OTS Hardware / Dashboard            │
    ├────────────────────────────────────────────┤
    │  Option A: Physical Device (ESP32-S3)      │
    │    - Nuke buttons with LED feedback        │
    │    - Alert LEDs (6 types)                  │
    │    - LCD troop counter + slider            │
    │    - Audio module (ESP32-A1S + CAN bus)    │
    │                                            │
    │  Option B: Dashboard Emulator (Nuxt)       │
    │    - Virtual hardware modules              │
    │    - Real-time state visualization         │
    │    - Development/testing tool              │
    └────────────────────────────────────────────┘
```

## ✨ Key Features

### Hardware Integration
- **Event-Driven Architecture** - Modular firmware with clean separation
- **I2C Expansion** - MCP23017 I/O expanders for buttons and LEDs
- **CAN Bus** - Multi-module communication (audio, future expansions)
- **OTA Updates** - Over-the-air firmware updates (HTTP, port 3232)
- **WebSocket Server** - Firmware acts as WSS server for dashboard/userscript

### Software Features
- **Real-time Event Tracking** - Detects nukes, invasions, game state changes
- **Protocol-Driven** - Single source of truth in `prompts/protocol-context.md`
- **Hardware Emulation** - Full dashboard simulator for development
- **Type Safety** - Shared TypeScript types across all components
- **Modular Design** - Easy to add new modules or features

### Protocol & Events
- Alert events (ALERT_ATOM, ALERT_HYDRO, ALERT_MIRV, ALERT_LAND, ALERT_NAVAL)
- Launch events (NUKE_LAUNCHED, HYDRO_LAUNCHED, MIRV_LAUNCHED)
- Outcome events (NUKE_EXPLODED, NUKE_INTERCEPTED)
- Game lifecycle (GAME_START, GAME_END, WIN, LOOSE)
- Sound events (SOUND_PLAY for audio module integration)

## 🧪 Development

### Testing Hardware
```bash
# Test I2C bus
cd ots-fw-main
pio run -e test-i2c -t upload && pio device monitor

# Test CAN bus protocol
cd ../ots-fw-cantest
pio run -e esp32-s3-devkit -t upload && pio device monitor
# Press 'm' for monitor mode, 'a' for audio simulator
```

### Building Releases
```bash
# Automated release script (updates all versions, builds, tags)
./release.sh -u -p -m "Release description"

# See prompts/RELEASE.md for complete guide
```

## 🤝 Contributing

We welcome contributions! See [CONTRIBUTING.md](doc/developer/CONTRIBUTING.md) for guidelines.

### Development Workflow
1. Create feature branch from `main`
2. Make changes following [coding standards](doc/developer/standards/coding-standards.md)
3. Test thoroughly (hardware tests, integration tests)
4. Update documentation if needed
5. Submit pull request with clear description

### Key Resources
- [Protocol Changes](doc/developer/workflows/protocol-changes.md) - Modifying WebSocket protocol
- [Adding Hardware Module](doc/developer/workflows/add-hardware-module.md) - New module workflow
- [Git Workflow](doc/developer/standards/git-workflow.md) - Branching and commit conventions

## 📋 Project Status

**Current Version**: See [weekly_announces.md](weekly_announces.md) for latest releases

**Hardware Status**:
- ✅ Main controller (ESP32-S3 + MCP23017 I/O expanders)
- ✅ Nuke module (3 buttons + 3 LEDs)
- ✅ Alert module (6 LEDs for threat types)
- ✅ Troops module (LCD + ADC slider)
- ✅ Audio module (ESP32-A1S with CAN bus)
- ✅ CAN bus testing tool

**Software Status**:
- ✅ Dashboard emulator (Nuxt 4)
- ✅ Browser userscript (TypeScript)
- ✅ WebSocket protocol (fully implemented)
- ✅ OTA firmware updates
- ✅ Sound system integration

## 📖 Technical Documentation

### Protocol & Architecture
- [Protocol Specification](prompts/protocol-context.md) - Complete WebSocket message format
- [Firmware Architecture](doc/developer/architecture/firmware.md) - ESP32-S3 design
- [Shared Components](doc/developer/architecture/shared-components.md) - Reusable code

### Hardware Documentation  
- [Hardware Specifications](ots-hardware/hardware-spec.md) - Main controller design
- [Module Specifications](ots-hardware/modules/) - Individual module details
- [PCB Designs](ots-hardware/pcbs/) - Circuit board layouts

### Component Context Files
- [Firmware Context](ots-fw-main/copilot-project-context.md)
- [Server Context](ots-server/copilot-project-context.md)
- [Userscript Context](ots-userscript/copilot-project-context.md)
- [Hardware Context](ots-hardware/copilot-project-context.md)

## License

TODO: Add license information for this project.
