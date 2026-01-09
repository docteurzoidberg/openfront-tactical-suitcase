# OpenFront Tactical Suitcase (OTS)

**Physical tactical controller for [OpenFront.io](https://openfront.io)**.

OTS bridges OpenFront.io gameplay to physical controls (buttons, LEDs, LCD, audio) and provides a dashboard + developer tooling to test everything without hardware.

[![ESP32-S3](https://img.shields.io/badge/ESP32-S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Nuxt](https://img.shields.io/badge/Nuxt-4-00DC82.svg)](https://nuxt.com/)
[![TypeScript](https://img.shields.io/badge/TypeScript-5-3178C6.svg)](https://www.typescriptlang.org/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP--IDF-orange.svg)](https://platformio.org/)

## What is OTS?

At a high level:

- A **browser userscript** reads game state/events from the OpenFront.io page.
- A **WebSocket endpoint** relays messages between clients.
    - In **hardware mode**, the ESP32 firmware exposes `/ws` (WS/WSS) and clients connect to it.
    - In **simulator mode**, the Nuxt app exposes `/ws` and emulates hardware in the UI.
- The **firmware** drives real hardware modules (nukes, alerts, troops LCD/slider, audio over CAN).

- **Nuke control panel**: 3 buttons + LEDs (atom/hydro/MIRV)
- **Alert module**: LED indicators for incoming threats
- **Troops module**: 16×2 LCD troop display + slider (ADC)
- **Audio module**: ESP32-A1S playback over CAN
- **Dashboard**: real-time hardware emulator + event log

## Documentation

### For device owners
See [doc/user/](doc/user/) for setup, usage, and troubleshooting.
- [Quick Start Guide](doc/user/quick-start.md) - Get running in 10 minutes
- [WiFi Setup](doc/user/wifi-setup.md) - Connect your device
- [Userscript Installation](doc/user/userscript-install.md) - Browser extension setup

### For developers
See [doc/developer/](doc/developer/) for development, architecture, and contributing.
- [Getting Started](doc/developer/getting-started.md) - Dev environment setup
- [Development Environment](doc/developer/development-environment.md) - Tooling, flashing, debugging
- [Repository Overview](doc/developer/repository-overview.md) - Codebase structure
- [WebSocket Protocol (guide)](doc/developer/websocket-protocol.md) - Implementation patterns and debugging
- [CAN Bus Protocol (guide)](doc/developer/canbus-protocol.md) - Implementation patterns and debugging

Protocol references (single source of truth):
- WebSocket: [prompts/WEBSOCKET_MESSAGE_SPEC.md](prompts/WEBSOCKET_MESSAGE_SPEC.md)
- CAN bus: [prompts/CANBUS_MESSAGE_SPEC.md](prompts/CANBUS_MESSAGE_SPEC.md)

## OpenFront.io notice

OTS is an independent, unofficial project and is **not affiliated with, endorsed by, or sponsored by OpenFront.io**.

- “OpenFront” / “OpenFront.io” may be trademarks of their respective owners.
- This repo does **not** include OpenFront proprietary/premium assets; do not extract or reuse proprietary assets from OpenFront.
- If you use the userscript, review OpenFront’s license/terms for any constraints that may apply to automated access.

## Quick start

Pick one of the two common workflows:

### A) Simulator workflow (no hardware)

1) Start the dashboard + WebSocket server:

```bash
cd ots-simulator
npm install
npm run dev
```

2) Build the userscript and install it in Tampermonkey:

```bash
cd ../ots-userscript
npm install
npm run build
```

Output file: `ots-userscript/build/userscript.ots.user.js`

3) In the userscript HUD settings, set the WS URL to `ws://localhost:3000/ws`.

### B) Hardware workflow (ESP32-S3)

1) Build/flash firmware:

```bash
cd ots-fw-main
pio run -e esp32-s3-dev -t upload
pio device monitor
```

2) Point the userscript (and dashboard, if used) at the device’s `/ws` endpoint.

Notes:
- Userscripts on `https://openfront.io/*` often require **WSS**; firmware supports TLS mode (see `WS_USE_TLS` in firmware config).
- The firmware exposes an HTTP server (for `/ws` and optional web UI/handlers).

See [doc/developer/getting-started.md](doc/developer/getting-started.md) for full setup details.

## Repository structure

### Core components

| Directory | Description | Tech Stack |
|-----------|-------------|------------|
| **ots-fw-main/** | Main ESP32-S3 firmware (hardware controller + `/ws` server) | C, ESP-IDF, PlatformIO |
| **ots-fw-audiomodule/** | ESP32-A1S audio playback module (CAN) | C, ESP-IDF |
| **ots-fw-cantest/** | CAN bus testing/debug tool | C, ESP-IDF |
| **ots-fw-can-hw-test/** | CAN hardware validation firmware | C, ESP-IDF |
| **ots-fw-shared/** | Shared ESP-IDF components (CAN drivers) | C, ESP-IDF |
| **ots-simulator/** | Dashboard + WebSocket server (simulator mode) | Vue 3, Nuxt 4, TypeScript |
| **ots-userscript/** | Tampermonkey userscript (polls game, sends events) | TypeScript, esbuild |
| **ots-shared/** | Shared TypeScript protocol types | TypeScript |
| **ots-hardware/** | Hardware specs, PCB designs, module docs | Markdown, CAD |
| **ots-website/** | VitePress docs site (built from `doc/`) | VitePress |

### Docs, prompts, release tooling

| Directory | Description |
|-----------|-------------|
| **doc/** | Complete user and developer documentation |
| **prompts/** | Single source-of-truth protocol specs + AI workflow docs |
| **.github/** | GitHub workflows and Copilot instructions |
| **release.sh** | Unified release/version automation |

## Architecture

Two supported runtime modes share the same message types:

### Hardware mode (physical device)

```
OpenFront.io page
    ↓ (userscript polls + emits events)
Userscript (WebSocket client)
    ↓
ESP32-S3 firmware (WebSocket server: /ws)
    ↔ Dashboard UI (optional client)
    ↓
Hardware modules (I2C + CAN)
```

### Simulator mode (no hardware)

```
OpenFront.io page
    ↓ (userscript polls + emits events)
Userscript (WebSocket client)
    ↓
Nuxt dashboard (WebSocket server: /ws)
    ↓
Virtual hardware modules (UI)
```

## Key features

### Hardware integration
- Event-driven firmware modules
- I2C expanders (MCP23017) for buttons/LEDs
- CAN bus for module-to-module communication (audio today, more later)
- OTA firmware updates (see firmware docs)
- WebSocket endpoint `/ws` (WS or WSS depending on config)

### Software features
- Userscript trackers detect game events and state transitions
- Simulator dashboard for dev/testing without hardware
- Shared TypeScript types in `ots-shared/`
- Protocol-first workflow (spec → shared types → implementations)

### Protocol highlights
See [prompts/WEBSOCKET_MESSAGE_SPEC.md](prompts/WEBSOCKET_MESSAGE_SPEC.md) for the full list. Common ones include:

- Alerts: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
- Launch/outcomes: `NUKE_LAUNCHED`, `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
- Lifecycle: `GAME_START`, `GAME_END`, `WIN`, `LOOSE`
- Audio trigger: `SOUND_PLAY`

## Development

### Protocol change workflow (important)

WebSocket protocol changes must start in the spec, then propagate:

1) Update [prompts/WEBSOCKET_MESSAGE_SPEC.md](prompts/WEBSOCKET_MESSAGE_SPEC.md)
2) Update shared TS types in `ots-shared/src/game.ts`
3) Update firmware types/parsing in `ots-fw-main/include/protocol.h` (and related code)
4) Update implementations in `ots-userscript/`, `ots-simulator/`, and/or firmware modules

CAN protocol changes follow the same “spec first” approach:

1) Update [prompts/CANBUS_MESSAGE_SPEC.md](prompts/CANBUS_MESSAGE_SPEC.md)
2) Update [doc/developer/canbus-protocol.md](doc/developer/canbus-protocol.md)
3) Update shared firmware components in `ots-fw-shared/components/`

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

## Contributing

We welcome contributions. Start with [doc/developer/README.md](doc/developer/README.md) and the workflow docs in [prompts/](prompts/).

### Development Workflow
1. Create feature branch from `main`
2. Make changes following [prompts/GIT_WORKFLOW.md](prompts/GIT_WORKFLOW.md)
3. Test thoroughly (hardware tests, integration tests)
4. Update documentation if needed
5. Submit pull request with clear description

### Key Resources
- [WebSocket Spec](prompts/WEBSOCKET_MESSAGE_SPEC.md) - Protocol source of truth
- [CAN Spec](prompts/CANBUS_MESSAGE_SPEC.md) - CAN source of truth
- [Release Guide](prompts/RELEASE.md) - Versioning and release process

## 📋 Project Status

**Current version**: See [weekly_announces.md](weekly_announces.md) for release history.

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
- [Protocol Specification](prompts/WEBSOCKET_MESSAGE_SPEC.md) - Complete WebSocket message format
- [CAN Specification](prompts/CANBUS_MESSAGE_SPEC.md) - Complete CAN message format
- [WebSocket Protocol (guide)](doc/developer/websocket-protocol.md) - Implementation patterns and debugging
- [CAN Bus Protocol (guide)](doc/developer/canbus-protocol.md) - Implementation patterns and debugging

### Hardware Documentation  
- [Hardware Specifications](ots-hardware/hardware-spec.md) - Main controller design
- [Module Specifications](ots-hardware/modules/) - Individual module details
- [PCB Designs](ots-hardware/pcbs/) - Circuit board layouts

### Component Context Files
- [Firmware Context](ots-fw-main/copilot-project-context.md)
- [Server Context](ots-simulator/copilot-project-context.md)
- [Userscript Context](ots-userscript/copilot-project-context.md)
- [Hardware Context](ots-hardware/copilot-project-context.md)

## License

This repository does not currently ship a top-level LICENSE file.

- If you want, I can add a LICENSE file (MIT / Apache-2.0 / GPLv3 / AGPLv3 / other) and update this section accordingly.
- Note: OpenFront itself has its own license/terms; this project is separate and does not grant rights to OpenFront proprietary assets.
