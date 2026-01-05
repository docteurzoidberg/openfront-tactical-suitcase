# Hardware Testing Guide

Quick reference for running hardware tests on the OTS firmware.

## Overview

Each hardware test is a standalone firmware that runs continuously. Flash a test, watch serial output, and verify physical hardware behavior.

**No menus, no frameworks - just simple test code.**

## Available Tests Firmwares

| Test | Command | What It Tests |
|------|---------|---------------|
| **I2C Scan** | `pio run -e test-i2c -t upload` | Detects all I2C devices on bus |
| **Output Board** | `pio run -e test-outputs -t upload` | Tests MCP23017 @ 0x21 (16 output pins) |
| **Input Board** | `pio run -e test-inputs -t upload` | Tests MCP23017 @ 0x20 (16 input pins) |
| **ADC** | `pio run -e test-adc -t upload` | Tests ADS1015 @ 0x48 (4-channel ADC) |
| **LCD** | `pio run -e test-lcd -t upload` | Tests 16x2 LCD @ 0x27 (display patterns) |
| **WebSocket** | `pio run -e test-websocket -t upload` | Tests WiFi + WebSocket + RGB LED status |

## Quick Start

### 1. Build and Flash Test

```bash
cd ots-fw-main

# Flash specific test
pio run -e test-i2c -t upload && pio device monitor

# Or separate commands
pio run -e test-outputs -t upload
pio device monitor
```

### 2. Watch Serial Output

- Open serial monitor: `pio device monitor`
- Baud rate: 115200
- Tests run in infinite loop
- Press `Ctrl+]` to exit monitor

### 3. Return to Production Firmware

```bash
pio run -e esp32-s3-dev -t upload
```

---

## Test Details

### Test 1: I2C Bus Scan

**Purpose**: Verify I2C bus and detect connected devices

**Expected Output:**
```
I (1234) TEST_I2C: === Starting I2C scan ===
I (1235) TEST_I2C:   [0x20] Input Board (MCP23017)
I (1236) TEST_I2C:   [0x21] Output Board (MCP23017)
I (1237) TEST_I2C:   [0x27] LCD Backpack (PCF8574)
I (1238) TEST_I2C:   [0x48] ADC (ADS1015)
I (1239) TEST_I2C: Scan complete: 4 devices found
```

**Manual Verification:**
- All expected devices appear
- No unexpected addresses
- Repeats every 5 seconds

**Troubleshooting:**
- 0 devices: Check I2C connections (SDA=GPIO8, SCL=GPIO9)
- Missing device: Check board power, verify I2C address
- Extra devices: Unknown hardware on bus

---

### Test 2: Output Board (MCP23017 @ 0x21)

**Purpose**: Test all 16 output pins individually

**Hardware Needed:**
- LEDs connected to output pins (or multimeter)

**Test Sequence:**
1. **Walking Bit** - One LED at a time (16 pins × 500ms)
2. **All On/Off** - All LEDs on, then all off (2s each)
3. **Alternating** - Even/odd pins alternate (5 cycles × 500ms)

**Expected Output:**
```
I (1234) TEST_OUTPUTS: === Walking Bit Test ===
I (1235) TEST_OUTPUTS:   A0 HIGH (0x01, 0x00)
I (1736) TEST_OUTPUTS:   A1 HIGH (0x02, 0x00)
...
I (1234) TEST_OUTPUTS: === All Pins On/Off Test ===
I (1235) TEST_OUTPUTS: All outputs HIGH (0xFF, 0xFF)
```

**Manual Verification:**
- Each LED lights individually during walking bit
- All LEDs turn on together
- All LEDs turn off together
- Alternating pattern visible

**Troubleshooting:**
- Pin stuck: Check solder joints, ULN2008A driver
- Multiple pins: Check for shorts on PCB
- No response: Verify MCP23017 at 0x21, check power

---

### Test 3: Input Board (MCP23017 @ 0x20)

**Purpose**: Read all 16 input pins with pullups enabled

**Hardware Needed:**
- Jumper wires to connect pins to GND

**Expected Output:**
```
Inputs: A=0xFF B=0xFF | A0=1 A1=1 A2=1 ... B0=1 B1=1 B2=1 ...
```

**Manual Verification:**
1. All pins show `1` (HIGH) with nothing connected
2. Connect pin to GND → pin changes to `0` (LOW)
3. Disconnect → pin returns to `1`
4. Test each of 16 pins individually

**Output Updates:**
- Continuous (10 Hz)
- Single line that refreshes in place
- Shows hex values + individual bit states

**Troubleshooting:**
- All pins LOW: Missing pullups, check configuration
- Pin stuck: Check solder joints, shorts to GND
- No changes: Verify MCP23017 at 0x20

---

### Test 4: ADC (ADS1015 @ 0x48)

**Purpose**: Read analog values from 4-channel 12-bit ADC

**Hardware Needed:**
- Potentiometer connected to AIN0 (primary test)
- Optional: Additional inputs on AIN1-3

**Test Sequence:**
1. **All Channels Snapshot** - Read all 4 channels once
2. **Continuous Reading** - Watch AIN0 change in real-time (10 samples)
3. **Voltage Range** - Interpret voltage level

**Expected Output:**
```
I (1234) TEST_ADC: === All Channels Snapshot ===
I (1235) TEST_ADC:   AIN0: 2048 (2.048V,  50%)
I (1236) TEST_ADC:   AIN1:    0 (0.000V,   0%)
I (1237) TEST_ADC:   AIN2:    0 (0.000V,   0%)
I (1238) TEST_ADC:   AIN3:    0 (0.000V,   0%)
I (1240) TEST_ADC: === Continuous Reading (10 samples) ===
[10] AIN0: 2048 (2.048V,  50%)
```

**Manual Verification:**
1. Connect potentiometer between 0V and 3.3V
2. Rotate potentiometer slowly
3. Watch ADC value change from 0 to ~3300
4. Verify voltage calculation is correct
5. Check percentage scales properly (0-100%)

**ADC Specifications:**
- **Resolution**: 12-bit (0-4095 counts)
- **Voltage Range**: 0 to 4.096V
- **Channels**: 4 single-ended inputs (AIN0-AIN3)
- **Sample Rate**: 1600 SPS
- **Conversion**: 1 count ≈ 1mV

**Troubleshooting:**
- Read error: Check ADS1015 at 0x48, verify I2C
- Always 0: Check input connection, verify 3.3V available
- Always max: Check for short to VCC
- Noisy readings: Add 0.1µF capacitor across input

---

### Test 5: LCD Display (16x2 @ 0x27)

**Purpose**: Test LCD display with various text patterns

**Hardware Needed:**
- 16x2 LCD with I2C backpack (PCF8574)
- 5V power supply

**Test Sequence:**
1. **Backlight Test** - Verify display visible
2. **Text Display** - Welcome message, full width, symbols
3. **Troop Format** - Production display format examples
4. **Alignment** - Left, right, center aligned text
5. **Cursor Position** - Border pattern with centered text
6. **Scrolling Counter** - Rapid count 0-100

**Expected Output (Serial):**
```
I (1234) TEST_LCD: === Text Display Test ===
I (1235) TEST_LCD: Test 1: Welcome message
I (1236) TEST_LCD: Test 2: Full width (16 chars)
I (1237) TEST_LCD: Test 3: Numbers and symbols
```

**Expected Output (LCD Screen):**
```
Line 1: "  LCD TEST OK  "
Line 2: "  Hardware v1  "
```

**Manual Verification:**
1. Check backlight is ON
2. Verify all characters are visible
3. Check contrast (adjust pot on LCD if needed)
4. Watch counter increment smoothly
5. Verify troop format displays correctly

**LCD Specifications:**
- **Size**: 16 columns × 2 rows
- **Controller**: HD44780 compatible
- **I2C Backpack**: PCF8574 expander
- **Address**: 0x27 (or 0x3F on some models)
- **Power**: 5V

**Troubleshooting:**
- Blank screen: Check 5V power, adjust contrast pot
- Garbled text: Check I2C connections, verify address
- Missing characters: Check LCD initialization sequence
- No backlight: Check PCF8574 backlight jumper/solder
- Wrong address: Try 0x3F if 0x27 doesn't work

---

### Test 6: WebSocket + RGB Status LED

**Purpose**: Verify complete networking stack functionality

**Hardware Needed:**
- ESP32-S3 with onboard RGB LED (GPIO48)
- WiFi network (credentials in config.h)
- OTS server running at configured URL

**Test Sequence:**
1. **RGB LED Cycle** - Test all LED colors manually (8 seconds)
2. **WiFi Connection** - Connect to configured network
3. **WebSocket Connection** - Connect to OTS server
4. **Message Exchange** - Send test events every 10 seconds
5. **Statistics Display** - Show connection uptime and message counts

**Expected Output (Serial):**
```
I (1234) TEST_WS: ╔═══════════════════════════════════════╗
I (1235) TEST_WS: ║    OTS WebSocket + LED Test           ║
I (1236) TEST_WS: ║    Network Stack Verification         ║
I (1237) TEST_WS: ╚═══════════════════════════════════════╝
I (1240) TEST_WS: Initializing RGB status LED (GPIO48)...
I (1245) TEST_WS: RGB LED initialized successfully
I (1250) TEST_WS: Configuration:
I (1251) TEST_WS:   WiFi SSID: IOT
I (1252) TEST_WS:   Server URL: ws://192.168.77.121:3000/ws
I (1260) TEST_WS: === RGB LED Manual Test ===
I (1261) TEST_WS:   OFF (black) - 2 seconds
I (3261) TEST_WS:   Orange - 2 seconds
I (5261) TEST_WS:   Green - 2 seconds
I (7261) TEST_WS:   Red - 2 seconds
I (9270) TEST_WS: ╔═══════════════════════════════════════╗
I (9271) TEST_WS: ║ WiFi Connected                        ║
I (9272) TEST_WS: ╚═══════════════════════════════════════╝
I (9273) TEST_WS: RGB LED: Orange (WiFi only)
I (9280) TEST_WS: IP Address: 192.168.77.123
I (9285) TEST_WS: ╔═══════════════════════════════════════╗
I (9286) TEST_WS: ║ WebSocket Connected                   ║
I (9287) TEST_WS: ╚═══════════════════════════════════════╝
I (9288) TEST_WS: RGB LED: Green (fully connected)
```

**Manual Verification:**

1. **LED Color Cycle** (first 8 seconds):
   - OFF (black/dark)
   - Orange (WiFi only status)
   - Green (fully connected status)
   - Red (error status)

2. **Connection Sequence** (watch RGB LED):
   - **Stage 1**: OFF → Device booting
   - **Stage 2**: Orange → WiFi connected, waiting for WebSocket
   - **Stage 3**: Green → Fully connected (WiFi + WebSocket)

3. **Connection Statistics** (every 10 seconds):
   - WiFi status and IP address
   - WebSocket connection uptime
   - Messages sent/received count
   - Current RGB LED status

4. **Message Exchange**:
   - INFO event sent
   - HARDWARE_TEST event sent
   - Watch message counter increment

**RGB LED States:**
- **OFF (black)**: No WiFi connection
- **Orange**: WiFi connected, WebSocket pending
- **Green**: Fully connected (WiFi + WebSocket)
- **Red**: Error state (shown during manual test)

**Network Requirements:**
- **WiFi SSID**: Must match `WIFI_SSID` in config.h
- **WiFi Password**: Must match `WIFI_PASSWORD` in config.h
- **Server**: OTS server must be running at `WS_SERVER_URL`
- **Server Port**: Default 3000 (configurable in config.h)

**Troubleshooting:**

**LED stays OFF:**
- Check WiFi credentials in config.h
- Verify network SSID is in range
- Check serial output for WiFi error messages
- Try: `nvs_flash_erase()` to clear saved credentials

**LED stays Orange:**
- WiFi OK, but WebSocket failed
- Check OTS server is running: `cd ots-simulator && npm run dev`
- Verify server URL in config.h matches server address
- Check firewall/network routing

**LED stays Red:**
- Manual test cycle showing error state
- Should return to OFF/Orange/Green after test
- If persistent: Check RGB LED GPIO48 connection

**No LED at all:**
- Check RGB LED GPIO pin (should be GPIO48)
- Verify LED is WS2812B type
- Check power to LED
- Review initialization logs for errors

**Connection keeps dropping:**
- Check WiFi signal strength
- Verify server stability
- Look for network interference
- Check power supply stability

**Messages not sent:**
- LED must be Green (fully connected)
- Check WebSocket connection uptime > 0
- Review serial logs for send errors
- Verify server is receiving messages

---

## Development Workflow

### Testing New Hardware

1. **Start with I2C scan** - Verify all devices detected
2. **Test outputs** - Verify LEDs/drivers work
3. **Test inputs** - Verify buttons/sensors work
4. **Test ADC/LCD** - Verify analog inputs and display
5. **Test networking** - Verify WiFi, WebSocket, and status LED
6. **Flash production firmware** - Ready for use

### Debugging Hardware Issues

1. **Flash relevant test** - Isolate the problem module
2. **Watch serial + hardware** - Visual confirmation
3. **Check connections** - I2C, power, solder joints
4. **Compare expected vs actual** - Document differences

### Assembly Line Testing

```bash
# Quick 3-test sequence
pio run -e test-i2c -t upload && pio device monitor    # Check: 4 devices
# [Wait 5 seconds, verify output]
# Ctrl+] to exit

pio run -e test-outputs -t upload && pio device monitor # Check: LEDs walking
# [Watch one full cycle]
# Ctrl+] to exit

pio run -e test-inputs -t upload && pio device monitor  # Check: All HIGH
# [Press buttons, verify they go LOW]
# Ctrl+] to exit

pio run -e esp32-s3-dev -t upload  # Flash production firmware
```

---

## Adding New Tests

See [docs/HARDWARE_TEST_INTEGRATION.md](docs/HARDWARE_TEST_INTEGRATION.md) for full details.

**Quick steps:**
1. Create `src/tests/test_new_feature.c` with `app_main()` function
2. Add environment to `platformio.ini`:
   ```ini
   [env:test-new-feature]
   platform = espressif32
   board = esp32-s3-devkitc-1
   framework = espidf
   monitor_speed = 115200
   build_src_filter = 
       +<*>
       -<main.c>
       +<tests/test_new_feature.c>
   ```
3. Build: `pio run -e test-new-feature -t upload`

---

## Notes

- **Test builds replace production firmware** - You must reflash production when done
- **Tests run forever** - Ctrl+] to exit serial monitor
- **No state preservation** - Each test starts fresh on boot
- **Serial required** - No on-device UI, all output to serial
- **Manual verification** - Watch output + hardware, take notes

