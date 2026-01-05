# Developer Documentation - Table of Contents

Documentation for developers and makers working on OTS.

## 🚀 Getting Started

### Essential Setup
1. **[Getting Started](getting-started.md)** ⭐ **START HERE**
   - Prerequisites and tools
   - Clone repository
   - Install dependencies
   - First build

2. **[Repository Overview](repository-overview.md)**
   - Project structure
   - Key directories
   - Build systems
   - Configuration files

## 🏗️ Development Guides

### Firmware Development
- **[Firmware Development](firmware-development.md)** - ESP32 firmware (C)
  - PlatformIO setup
  - Building main controller (ots-fw-main)
  - Building audio module (ots-fw-audiomodule)
  - CAN bus testing (ots-fw-cantest)
  - Hardware testing environments
  - Flashing and debugging

### Dashboard/Server Development
- **[Server Development](server-development.md)** - Nuxt dashboard
  - Node.js setup
  - Running dev server
  - WebSocket server
  - Hardware emulator mode
  - Building for production

### Userscript Development
- **[Userscript Development](userscript-development.md)** - Browser extension
  - TypeScript setup
  - Building userscript
  - Watch mode for development
  - Testing in browser
  - Debugging techniques

## 📐 Architecture Documentation

### System Design
- **[Architecture Overview](architecture/README.md)** - High-level system design
  - Component interaction
  - WebSocket protocol flow
  - Event-driven architecture
  - Hardware abstraction layers

- **[Protocol Specification](architecture/protocol.md)** - WebSocket message format
  - Message envelopes
  - Game events
  - Commands
  - State synchronization

- **[Firmware Architecture](architecture/firmware.md)** - ESP32-S3 design
  - Task architecture
  - Event dispatcher
  - Module system
  - I2C hardware drivers
  - CAN bus integration

- **[Shared Components](architecture/shared-components.md)** - Reusable code
  - TypeScript shared types (ots-shared)
  - ESP-IDF shared components (ots-fw-shared)
  - CAN protocol implementation

### Hardware Documentation
- **[Hardware Overview](hardware/README.md)** - Physical device design
  - Main controller specs
  - Module interface
  - I2C architecture
  - CAN bus topology
  - Power distribution

- **[Hardware Modules](hardware/modules.md)** - Individual module specs
  - Nuke module
  - Alert module
  - Troops module
  - Audio module

- **[PCB Designs](hardware/pcb-designs.md)** - Circuit board layouts
  - Schematics
  - BOM (Bill of Materials)
  - Assembly notes

## 🔧 Development Workflows

### Common Tasks
- **[Adding a Hardware Module](workflows/add-hardware-module.md)**
- **[Adding a Game Event](workflows/add-game-event.md)**
- **[Protocol Changes](workflows/protocol-changes.md)**
- **[Testing Hardware](workflows/testing-hardware.md)**
- **[Over-The-Air Updates](workflows/ota-updates.md)**

### Code Standards
- **[Coding Standards](standards/coding-standards.md)**
  - C/C++ style (firmware)
  - TypeScript style (server/userscript)
  - Naming conventions
  - Documentation requirements

- **[Git Workflow](standards/git-workflow.md)**
  - Branch strategy
  - Commit messages
  - Pull request process
  - Release tagging

## 🧪 Testing

- **[Testing Guide](testing/README.md)**
  - Unit tests
  - Integration tests
  - Hardware testing
  - End-to-end testing

- **[Hardware Test Plan](testing/hardware-test-plan.md)**
  - I2C bus testing
  - Module testing
  - LED/button testing
  - CAN bus validation

## 📦 Deployment

- **[Release Process](deployment/release-process.md)**
  - Version numbering
  - Building releases
  - Firmware flashing
  - Distribution

- **[Firmware Updates](deployment/firmware-updates.md)**
  - OTA update procedure
  - Update verification
  - Rollback strategy

## 🤝 Contributing

- **[CONTRIBUTING.md](CONTRIBUTING.md)** - How to contribute
  - Code of conduct
  - Issue reporting
  - Pull request guidelines
  - Development setup

- **[FAQ.md](FAQ.md)** - Developer FAQ
  - Build issues
  - Common errors
  - Platform-specific notes

## 📚 Reference

- **[API Reference](reference/api/)** - Code documentation
  - Firmware APIs
  - Server APIs
  - Userscript APIs

- **[Component Reference](reference/components/)** - Hardware drivers
  - MCP23017 I/O expander
  - ADS1015 ADC
  - HD44780 LCD
  - WS2812 RGB LED
  - CAN/TWAI driver

## 🔗 External Resources

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [PlatformIO Docs](https://docs.platformio.org/)
- [Nuxt Documentation](https://nuxt.com/)
- [Tampermonkey API](https://www.tampermonkey.net/documentation.php)
