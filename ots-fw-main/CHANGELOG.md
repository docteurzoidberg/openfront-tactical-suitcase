# Changelog

All notable changes to the OTS Firmware Main Controller project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed - 2025-12-24 (Maintenance / Cleanup)
- **Removed Improv Serial provisioning**
  - Deleted Improv implementation files from firmware build
  - Kept historical documentation in `docs/IMPROV_SERIAL.md` (marked removed)
- **More robust boot without optional hardware**
  - Firmware no longer aborts startup when MCP23017 expanders (0x20/0x21) are missing
  - Allows WiFi/WebSocket test builds to run without I/O boards attached
- **Reduced event noise and improved userscript detection**
  - Improved WebSocket server-side userscript presence detection
  - Reduced noisy/flood events that could contribute to event queue pressure
- **Build/test reliability improvements**
  - Updated test build selection to prefer PlatformIO `-DTEST_*` build flags
  - Updated PlatformIO board config to `esp32-s3-devkitc1-n16r8` (16MB flash)
- **Docs/prompts organization**
  - Long-form docs live in `docs/`
  - AI prompt guides live in `prompts/`

### Added - 2025-12-19 (Game End Screen)
- **Victory/Defeat display on LCD**
  - Dedicated game end screen in `system_status_module`
  - Shows "VICTORY!" or "DEFEAT" based on game outcome
  - Displays for 5 seconds after game ends
  - Responds to `GAME_EVENT_WIN` and `GAME_EVENT_LOOSE` events
  - Automatic return to system status after timeout
- **Userscript game outcome detection**
  - Added `didPlayerWin()` method to GameAPI
  - Checks `myPlayer.eliminated()` and `game.gameOver()` state
  - Sends specific `WIN` or `LOOSE` events instead of generic `GAME_END`
  - Falls back to `GAME_END` if outcome cannot be determined

### Added - 2025-12-19 (I/O Expander Error Recovery & WebSocket Disconnect Feedback)
- **Comprehensive error recovery system for MCP23017 I/O expanders**
  - Retry logic with exponential backoff (5 attempts, 100ms → 5000ms)
  - Health monitoring with error tracking (error count, consecutive errors, recovery count)
  - Automatic recovery via periodic health checks (every 10 seconds)
  - Board reinitalization API (`io_expander_reinit_board()`)
  - Manual recovery API (`io_expander_attempt_recovery()`)
  - Recovery callback system for notification (`io_expander_set_recovery_callback()`)
  - Health status queries (`io_expander_get_health()`, `io_expander_health_check()`)
  - Error counter reset (`io_expander_reset_errors()`)
  - Boards marked unhealthy after 3 consecutive errors
  - Integrated into I/O task loop with automatic recovery attempts
  - Module I/O reconfiguration after recovery (`module_io_reinit()`)
  - Detailed logging for debugging and monitoring

- **Visual feedback for WebSocket connection state during gameplay**
  - Nuke module: Fast blink (200ms) on active nuke LEDs when WebSocket disconnects
  - Alert module: Fast blink (100ms) on warning LED when disconnected with active threats
  - Alert module: Slow blink (500ms) on warning LED when disconnected without threats
  - Automatic LED state restoration on WebSocket reconnection
  - Clear visual distinction between connected (solid/normal) and disconnected (fast blink) states

- **Smart LCD display system with dedicated status module**
  - **NEW MODULE**: `system_status_module` handles all non-game LCD screens:
    - Splash screen on boot: "OTS Firmware / Booting..."
    - Waiting for connection: "Waiting for / Connection..."
    - Lobby screen: "Connected! / Waiting Game..."
  - **REFACTORED**: `troops_module` simplified to only handle in-game troop display
    - Shows troop counts only during active gameplay (IN_GAME phase)
    - Slider monitoring disabled when not in game
    - Clean separation: system status module owns LCD during boot/lobby, troops module during game
  - Automatic LCD control handoff between modules based on game phase
  - Clear user feedback about connection and game state at all times
  - Helps players identify connection issues vs. actual game state

### Changed - 2025-12-17 (Phase 4.5: Button Handling Encapsulation)
- **Refactored button handling for proper module encapsulation**
  - Added `INTERNAL_EVENT_BUTTON_PRESSED` internal event type
  - button_handler.c now posts internal events instead of using callbacks
  - Moved button->nuke mapping logic from main.c into nuke_module.c
  - nuke_module now handles complete button press workflow (detect → create event → send to WebSocket → LED feedback)
  - Removed 50+ lines of button-specific logic from main.c
  - Each module now owns its entire feature set without leaking logic to main.c
  - Improved link LED feedback: added `led_controller_link_blink()` helper function
  - Fixed main_power_module to properly show connection states via blinking:
    * Network connected → BLINK 500ms (waiting for WebSocket)
    * WebSocket connected → SOLID ON (fully connected)
    * WebSocket disconnected → BLINK 500ms (network OK, no WS)
    * WebSocket error → FAST BLINK 200ms (error state)

### Changed - 2025-12-16 (Phase 4: Architecture Refactoring & Code Quality)
- **Refactored firmware for cleaner module separation and protocol alignment**
  - Created hardware module abstraction layer mirroring physical PCB modules
  - Implemented module manager for centralized hardware module coordination
  - Created dedicated modules: nuke_module, alert_module, main_power_module
  - Each hardware module implements standardized interface (init, update, handle_event, get_status, shutdown)

- **Fixed missing and consolidated event types**
  - Added missing event types: `GAME_EVENT_NUKE_EXPLODED`, `GAME_EVENT_NUKE_INTERCEPTED`
  - Added internal-only events: `INTERNAL_EVENT_NETWORK_CONNECTED`, `INTERNAL_EVENT_NETWORK_DISCONNECTED`, `INTERNAL_EVENT_WS_CONNECTED`, `INTERNAL_EVENT_WS_DISCONNECTED`, `INTERNAL_EVENT_WS_ERROR`
  - Fixed alert_module to use correct `GAME_EVENT_ALERT_*` events (was using non-existent `*_INBOUND` events)
  - Consolidated `game_phase_t` enum to single source of truth in game_state.h
  - Updated protocol.c with string conversions for all event types

- **Simplified event structures**
  - Unified `internal_event_t` field sizes to match `game_event_t` (128 for message, 256 for data)
  - Removed confusion between game events and internal events
  - Alert module now properly handles game lifecycle events

- **Reduced code duplication**
  - Created button mapping lookup table in main.c (eliminated repetitive if/else chains)
  - Button handler now uses `button_map[]` array for maintainability
  - Reduced `handle_button_press()` from 20 lines to 15 lines

- **Cleaned up unused code**
  - Removed unused `hw_state_t`, `game_state_t`, and related module state structures from protocol.h
  - Removed 200+ bytes of unused struct definitions
  - Kept only essential `game_event_t` structure

- **Improved event routing**
  - network_manager now posts `INTERNAL_EVENT_NETWORK_CONNECTED/DISCONNECTED` to event dispatcher
  - ws_client now posts `INTERNAL_EVENT_WS_CONNECTED/DISCONNECTED/ERROR` to event dispatcher
  - main_power_module receives these events and controls link LED accordingly
  - Alert module properly triggers warning LED on any alert event

- **Global I/O board configuration**
  - Board 0 (0x20): ALL 16 pins configured as INPUT with pullup (buttons, sensors)
  - Board 1 (0x21): ALL 16 pins configured as OUTPUT (LEDs, relays)
  - Simplified module_io.c initialization to configure entire boards globally

### Technical Details - Phase 4
- **Module Architecture**: Each physical PCB = one software module with standardized interface
- **Module Manager**: Supports up to 8 hardware modules with registration and coordination
- **Event Flow**: Button → Event Dispatcher → Hardware Modules → LED Controller
- **Protocol Alignment**: Event types now match protocol-context.md specification
- **Code Quality**: No compilation errors, cleaner architecture, better maintainability

### Added - 2025-12-15 (Troops Module)
- **Created Troops Module firmware implementation prompt**
  - Added `prompts/TROOPS_MODULE_PROMPT.md` with complete ESP-IDF implementation guide
  - Hardware specification for I2C 1602A APKLVSR LCD (yellow-green backlight)
  - ADS1115 16-bit ADC for potentiometer slider input
  - Protocol integration for troops state display and deployment percentage control
  - Custom PCF8574 I2C LCD control functions (HD44780 via I2C expander)
  - Custom ADS1115 I2C ADC functions for slider reading
  - 2×16 character display format with K/M/B unit scaling
  - Debounced slider polling (100ms) with 1% change threshold
  - ESP-IDF I2C master driver implementation examples
  - FreeRTOS timing functions (vTaskDelay, xTaskGetTickCount)
  - Comprehensive testing checklist for I2C, display, slider, protocol, and integration

### Added - 2025-12-14 (Phase 3: OTA Implementation)
- **Implemented ESP-IDF native OTA (Over-The-Air) updates**
  - Added HTTP server on port 3232 for receiving firmware updates
  - Implemented dual-partition OTA (ota_0 and ota_1) for safe updates
  - Added mDNS service for hostname discovery (`ots-fw-main.local`)
  - Created custom partition table (`partitions.csv`) supporting OTA
  - Added visual feedback: LINK LED blinks during OTA upload showing progress
  - Progress logging every 5% during firmware upload
  - Automatic reboot after successful update
  - Safe error handling: failed updates don't brick the device
  - All module LEDs turn off during OTA update process
- **Added OTA configuration**
  - `OTA_HOSTNAME`: "ots-fw-main" for mDNS discovery
  - `OTA_PORT`: 3232 (standard OTA port)
  - `OTA_PASSWORD`: "ots2025" (placeholder, basic auth not fully implemented yet)
- **Updated dependencies**
  - Added `espressif__mdns` component for hostname resolution
  - Added `esp_http_server` for OTA HTTP endpoint
  - Added `app_update` for OTA partition management
- **Updated documentation**
  - Refreshed `docs/OTA_GUIDE.md` with ESP-IDF specific instructions
  - Added curl-based OTA upload examples
  - Documented partition table layout and boot process
  - Added troubleshooting section for ESP-IDF OTA

### Technical Details - OTA Implementation
- **Upload Method**: HTTP POST to `http://ots-fw-main.local:3232/update`
- **Partition Scheme**: Factory (2MB) + OTA_0 (2MB) + OTA_1 (2MB)
- **Current Size**: ~1MB firmware (48% of 2MB partition)
- **Safety**: Dual partition with automatic rollback on boot failure
- **Components**: esp_ota_ops, esp_http_server, mdns
- **Progress Feedback**: Visual (LED) + Serial logging every 5%

### Changed - 2025-12-14 (Phase 2: Build System & Dependencies)
- **Fixed ESP-IDF build configuration** for successful compilation
  - Restructured CMake configuration with proper project initialization
  - Added ESP-IDF component dependencies (esp_websocket_client, json, esp_timer, driver)
  - Created `idf_component.yml` to manage ESP-IDF managed components
  - Fixed component registration in `src/CMakeLists.txt` with REQUIRES clause
- **Updated GPIO configuration for ESP32-S3**
  - Changed I2C pins from GPIO 21/22 to GPIO 8/9 (ESP32-S3 compatible)
  - Added I2C pin definitions in `config.h`
- **Fixed missing header includes**
  - Added FreeRTOS headers to `io_expander.c` for `portTICK_PERIOD_MS`
  - Added `esp_timer.h` to `main.c` for `esp_timer_get_time()`
  - Added `config.h` include to `io_expander.c` for pin definitions
- **Cleaned up PlatformIO configuration**
  - Removed conflicting `CONFIG_LOG_DEFAULT_LEVEL` from build_flags
  - Added proper sdkconfig defaults reference
  - Removed all legacy `.bak` files from Arduino migration
- **Fixed Python dependency issues**
  - Resolved Click library compatibility with esptoolpy

### Build Results - 2025-12-14
- ✅ **Firmware compiles successfully for ESP32-S3**
- Bootloader: 21KB
- Firmware: 906KB (11.1% of 8MB flash)
- RAM usage: 36,484 bytes (11.1% of 320KB)
- All components properly linked

### Changed - 2025-12-14 (Phase 1: Framework Migration)
- **[BREAKING]** Migrated from Arduino framework to ESP-IDF framework
- **[BREAKING]** Converted all C++ code to C for better ESP-IDF compatibility
- Rewritten MCP23017 driver using native ESP-IDF I2C master driver
  - Removed dependency on Adafruit MCP23017 library
  - Implemented direct I2C register read/write operations
  - Added support for input pull-ups
- Rewritten WebSocket client using ESP-IDF's `esp_websocket_client` component
  - Removed dependency on AsyncWebServer and AsyncTCP libraries
  - Implemented client connection to server (was server in Arduino version)
  - Added automatic reconnection handling
  - Added handshake protocol to identify firmware client type
- Converted protocol handling to use cJSON (ESP-IDF native) instead of ArduinoJson
- Replaced `setup()` and `loop()` with `app_main()` and FreeRTOS tasks
- Implemented proper FreeRTOS task for button monitoring
- Implemented FreeRTOS timers for LED blinking and alert duration
- Updated WiFi initialization to use ESP-IDF native WiFi stack
- Simplified configuration in `config.h` for ESP-IDF

### Added - 2025-12-14
- CMakeLists.txt for ESP-IDF component registration
- Component dependency management via idf_component.yml
- Proper event handling for WiFi connection/disconnection
- LED link status indicator for WiFi/WebSocket connection
- Button debouncing in dedicated monitoring task
- Timer-based LED blinking for nuke buttons (10 seconds)
- Timer-based alert LED management (10s for nukes, 15s for invasions)
- Automatic warning LED management (on when any alert active)
- Structured logging using ESP-IDF log system

### Removed - 2025-12-14
- Arduino framework dependencies
- Adafruit MCP23017 Arduino Library
- AsyncWebServer library
- AsyncTCP library
- ArduinoJson library
- Legacy .bak backup files
- OTA update functionality (to be re-implemented with ESP-IDF OTA)

### Technical Details - 2025-12-14
- **Platform**: ESP-IDF v5.4.2 via PlatformIO
- **Board**: ESP32-S3-DevKitC-1 (8MB Flash, 320KB RAM)
- **Toolchain**: xtensa-esp-elf-gcc 14.2.0
- **I2C Driver**: ESP-IDF I2C master (driver/i2c_master.h)
- **I2C Pins**: SDA=GPIO8, SCL=GPIO9
- **WebSocket**: ESP-IDF WebSocket client via managed component
- **JSON**: cJSON (native ESP-IDF component)
- **RTOS**: FreeRTOS (native ESP-IDF)
- **WiFi**: ESP-IDF WiFi stack (esp_wifi.h)

### Migration Notes
- Protocol types synchronized with server and shared types
- WebSocket now connects as client to server instead of acting as server
- All hardware functionality preserved (buttons, LEDs, I/O expanders)
- Build system fully functional with proper dependency management

## [0.1.0] - Initial Release

### Added
- Basic Arduino framework implementation
- MCP23017 I/O expander support
- WebSocket server for local communication
- Nuke module button and LED control
- Alert module LED control
- Main power module link LED
- WiFi connectivity
- Protocol message handling
