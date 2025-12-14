# Changelog

All notable changes to the OTS Firmware Main Controller project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
