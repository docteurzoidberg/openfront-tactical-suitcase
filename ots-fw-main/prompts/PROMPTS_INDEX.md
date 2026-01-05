# OTS Firmware Prompts (ots-fw-main)

This folder contains **AI prompt files** used when working on the firmware.

## Core Prompts (Start Here)

### System-Level
- **Architecture Overview**: `ARCHITECTURE_PROMPT.md` - Component structure and design patterns
- **Main Firmware**: `FIRMWARE_MAIN_PROMPT.md` - Application entry point and initialization
- **Protocol Changes**: `PROTOCOL_CHANGE_PROMPT.md` - Synchronizing protocol across components

### Development Workflows
- **Build & Test**: `BUILD_AND_TEST_PROMPT.md` - PlatformIO environments and testing
- **Debug & Recovery**: `DEBUGGING_AND_RECOVERY_PROMPT.md` - Boot loops and error recovery
- **Module Development**: `MODULE_DEVELOPMENT_PROMPT.md` - Creating new hardware modules
- **Automated Tests**: `AUTOMATED_TESTS_PROMPT.md` - Test automation and CI/CD

## Hardware Module Prompts

Each hardware module has a dedicated prompt file with implementation details:

- **Main Power**: `MAIN_POWER_MODULE_PROMPT.md` - LINK LED and connection status
- **Nuke Control**: `NUKE_MODULE_PROMPT.md` - Nuke launch buttons and LEDs
- **Alert System**: `ALERT_MODULE_PROMPT.md` - Incoming threat indicators
- **Troops Display**: `TROOPS_MODULE_PROMPT.md` - LCD + slider + ADC
- **Sound System**: `SOUND_MODULE_PROMPT.md` - Audio feedback and alerts

## System Component Prompts

Core system functionality with dedicated implementation guides:

- **System Status Module**: `SYSTEM_STATUS_MODULE_PROMPT.md` - LCD display screens and game state visualization
- **I2C Components**: `I2C_COMPONENTS_PROMPT.md` - Creating reusable I2C device drivers
- **Network Provisioning**: `NETWORK_PROVISIONING_PROMPT.md` - WiFi setup and captive portal
- **OTA Updates**: `OTA_UPDATE_PROMPT.md` - Over-the-air firmware update implementation

## Documentation Files (../docs/)

**User Guides:**
- `TESTING.md` - How to run hardware tests
- `OTA_GUIDE.md` - OTA update procedures
- `OTS_DEVICE_TOOL.md` - CLI tool usage
- `WSS_TEST_GUIDE.md` - WebSocket testing

**Developer Reference:**
- `NAMING_CONVENTIONS.md` - Code style guide
- `RGB_LED_STATUS.md` - Status LED behavior
- `ERROR_RECOVERY.md` - Boot failure recovery
- `HARDWARE_TEST_PLAN.md` - Test strategy

**Implementation Notes:**
- `HARDWARE_DIAGNOSTIC_IMPLEMENTATION.md`
- `HARDWARE_TEST_INTEGRATION.md`
- `WIFI_PROVISIONING.md`
- `WIFI_WEBAPP.md`
- `PROTOCOL_ALIGNMENT_CHECK.md`
- `OTA_VERIFICATION_REPORT.md`
- `CERTIFICATE_INSTALLATION_CHROME.md`
- `TEST_WIFI_PROVISIONING_FLOW.md`
- `SOUND_MODULE_IMPLEMENTATION.md`

For a quick overview, start at `../README.md`.

## How to Add a New Prompt

- Keep prompts **task-oriented** (what to do next, what to avoid), not a full spec.
- Prefer **links to existing docs** over duplicating the same instructions.
- If a change impacts protocol, start at the repo root `prompts/WEBSOCKET_MESSAGE_SPEC.md`.
