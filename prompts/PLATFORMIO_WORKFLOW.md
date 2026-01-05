# PlatformIO Workflow - AI Assistant Guidelines

## Purpose

This prompt guides AI assistants on PlatformIO build system operations for ESP32 firmware projects in the OTS repository.

## Critical Rule: Always Specify Environment

**NEVER run PlatformIO commands without the `-e` flag.**

❌ **WRONG:**
```bash
pio run
pio run -t upload
pio run -t clean
```

✅ **CORRECT:**
```bash
pio run -e esp32-s3-dev
pio run -e esp32-s3-dev -t upload
pio run -e esp32-s3-dev -t clean
```

**Why:** Running without `-e` causes build failures or uses wrong configuration.

## Project Environments

### ots-fw-main

**Main Production Firmware:**
- `esp32-s3-dev` - Main firmware with all modules

**Hardware Test Environments:**
- `test-i2c` - I2C bus scan test
- `test-outputs` - Output board LED test (Board 1)
- `test-inputs` - Input board button test (Board 0)
- `test-adc` - ADC slider test (ADS1015)
- `test-lcd` - LCD display test (HD44780 via PCF8574)
- `test-websocket` - WebSocket connection test

**Default environment:** `esp32-s3-dev`

### ots-fw-audiomodule

**Audio Module Firmware:**
- `esp32-a1s-espidf` - Main audio module firmware

**Default environment:** `esp32-a1s-espidf`

### ots-fw-cantest

**CAN Bus Testing Tool:**
- `esp32-s3-devkit` - CAN bus sniffer and simulator

**Default environment:** `esp32-s3-devkit`

## Common Commands

### Build Firmware

```bash
cd ots-fw-main
pio run -e esp32-s3-dev
```

**When to suggest:** User asks to "build", "compile", or "check if code compiles"

### Clean Build

```bash
pio run -e esp32-s3-dev -t clean
pio run -e esp32-s3-dev
```

**When to suggest:** User reports build errors, wants fresh build, or changed configuration

### Upload to Device

```bash
pio run -e esp32-s3-dev -t upload
```

**When to suggest:** User asks to "flash", "upload", "deploy to device"

**Note:** Device must be connected via USB. Auto-detects serial port.

### Build and Upload (Combined)

```bash
pio run -e esp32-s3-dev -t upload && pio device monitor
```

**When to suggest:** User wants to test changes on hardware immediately

### Monitor Serial Output

```bash
pio device monitor
```

**When to suggest:** User asks to "see logs", "check serial output", "debug"

**To exit:** Press `Ctrl+C`

## Test Environment Workflow

### Running Hardware Tests

**Example: Test I2C bus**
```bash
cd ots-fw-main
pio run -e test-i2c -t upload && pio device monitor
```

**Key point:** Test environments are standalone firmware (no menus). Flash the test, monitor output, verify hardware.

**Available tests:**
- `test-i2c` → Scans I2C bus, lists detected devices
- `test-outputs` → Blinks all LEDs on Board 1 sequentially
- `test-inputs` → Shows button states on Board 0 in real-time
- `test-adc` → Reads slider value continuously
- `test-lcd` → Displays test patterns on LCD
- `test-websocket` → Tests connection to WebSocket server

### When to Suggest Test Environments

**User says "I2C isn't working":**
```bash
cd ots-fw-main
pio run -e test-i2c -t upload && pio device monitor
```
Then check if devices at expected addresses (0x20, 0x21, 0x27, 0x48).

**User says "buttons not responding":**
```bash
pio run -e test-inputs -t upload && pio device monitor
```
Watch for button state changes.

**User says "LEDs not working":**
```bash
pio run -e test-outputs -t upload && pio device monitor
```
Check which LEDs blink (if any).

## Build Validation

### Ignoring Non-Critical Warnings

**This warning is safe to ignore:**
```
esp_idf_size: error: unrecognized arguments: --ng
```

**Explanation:** Non-blocking warning from ESP-IDF size tool. Does not affect build success.

**Build is successful if:**
- Compile + link completes
- `pio run -e <env>` exits with code 0
- `.pio/build/<env>/firmware.bin` exists

### Checking Build Output

**Success indicators:**
```
Linking .pio/build/esp32-s3-dev/firmware.elf
Building .pio/build/esp32-s3-dev/firmware.bin
RAM:   [==        ]  18.2% (used 59680 bytes from 327680 bytes)
Flash: [====      ]  42.1% (used 551345 bytes from 1310720 bytes)
```

**Failure indicators:**
```
fatal error: <header>.h: No such file or directory
undefined reference to <function>
region 'iram0_0_seg' overflowed
```

## Multi-Project Workflow

### When User Works Across Projects

**Example: Protocol change affecting firmware + userscript**

```bash
# 1. Update shared types
cd ots-shared
npm run build

# 2. Build userscript
cd ../ots-userscript
npm run build

# 3. Build firmware
cd ../ots-fw-main
pio run -e esp32-s3-dev

# 4. Flash if needed
pio run -e esp32-s3-dev -t upload && pio device monitor
```

## Common Errors and Solutions

### Error: "Please specify environment"

**Symptom:**
```
Error: Please specify `upload_port` for environment or use global `--upload-port` option.
```

**Solution:** Add `-e` flag:
```bash
pio run -e esp32-s3-dev -t upload
```

### Error: "No such file or directory"

**Symptom:** Missing header or source file

**Solution:** Check if file was added to `CMakeLists.txt`:
```cmake
# In src/CMakeLists.txt
set(SRCS
    "main.c"
    "new_file.c"  # Must be added here
)
```

### Error: "Port not found"

**Symptom:**
```
Error: Serial port not found
```

**Solution:**
```bash
# List available ports
pio device list

# Specify port manually
pio run -e esp32-s3-dev -t upload --upload-port /dev/ttyUSB0
```

### Build Succeeds but Upload Fails

**Solution:** Device might be in use by monitor or another process:
```bash
# Kill monitor
pkill -f "pio device monitor"

# Try upload again
pio run -e esp32-s3-dev -t upload
```

## Important: PlatformIO Only

**This project uses PlatformIO exclusively.** Do not suggest `idf.py` commands or direct ESP-IDF toolchain usage.

**Why:**
- PlatformIO manages ESP-IDF toolchain automatically
- Consistent build environment across developers
- No manual ESP-IDF setup required
- Test environments only available via PlatformIO

**If user asks about ESP-IDF:** Explain that PlatformIO uses ESP-IDF under the hood and provides all necessary functionality through `pio` commands.

## ESP Tools Access

**esptool is NOT installed on the host system** but is available through PlatformIO's managed toolchain.

### Using esptool via PlatformIO

**Read flash:**
```bash
cd ots-fw-main
pio run -e esp32-s3-dev -t read_flash
```

**Erase flash:**
```bash
cd ots-fw-main
pio run -e esp32-s3-dev -t erase
```

**Get chip info:**
```bash
cd ots-fw-main
pio pkg exec -p tool-esptoolpy -- esptool.py --port /dev/ttyUSB0 chip_id
```

**Read MAC address:**
```bash
cd ots-fw-main
pio pkg exec -p tool-esptoolpy -- esptool.py --port /dev/ttyUSB0 read_mac
```

**Generic esptool command:**
```bash
cd ots-fw-main
pio pkg exec -p tool-esptoolpy -- esptool.py <arguments>
```

**When to suggest esptool:**
- User needs to erase NVS/flash completely
- User asks for chip information
- User wants to read/verify flash contents
- User reports corrupted firmware and needs factory reset

## Suggesting Build Commands

### User Context → Command Suggestion

| User Says | Suggest |
|-----------|---------|
| "Build the firmware" | `pio run -e esp32-s3-dev` |
| "Flash to device" | `pio run -e esp32-s3-dev -t upload` |
| "Test and monitor" | `pio run -e esp32-s3-dev -t upload && pio device monitor` |
| "Clean build" | `pio run -e esp32-s3-dev -t clean && pio run -e esp32-s3-dev` |
| "Test I2C" | `pio run -e test-i2c -t upload && pio device monitor` |
| "Check compilation" | `pio run -e esp32-s3-dev` (no upload) |
| "Build audio module" | `cd ots-fw-audiomodule && pio run -e esp32-a1s-espidf` |
| "Build CAN test tool" | `cd ots-fw-cantest && pio run -e esp32-s3-devkit` |

## Best Practices When Assisting Users

1. **Always specify `-e` flag** - Never omit environment
2. **Suggest upload + monitor together** - User usually wants to see output immediately
3. **Use test environments for hardware debugging** - Faster than full firmware
4. **Check CMakeLists.txt when adding files** - Common mistake to forget
5. **Ignore `esp_idf_size --ng` warning** - Mention it's non-blocking if user asks
6. **Use project root as working directory** - Commands assume `cd ots-fw-main` first
7. **Suggest clean build on weird errors** - Often fixes stale build issues

## Quick Reference Card

```bash
# Build main firmware
cd ots-fw-main && pio run -e esp32-s3-dev

# Flash and monitor
cd ots-fw-main && pio run -e esp32-s3-dev -t upload && pio device monitor

# Test I2C devices
cd ots-fw-main && pio run -e test-i2c -t upload && pio device monitor

# Clean build
cd ots-fw-main && pio run -e esp32-s3-dev -t clean && pio run -e esp32-s3-dev

# Build audio module
cd ots-fw-audiomodule && pio run -e esp32-a1s-espidf

# Build CAN test
cd ots-fw-cantest && pio run -e esp32-s3-devkit
```
