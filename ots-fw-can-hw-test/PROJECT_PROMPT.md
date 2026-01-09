# CAN Hardware Test Firmware - Project Prompt

## Overview

`ots-fw-can-hw-test` is a **bare-metal TWAI/CAN hardware validation tool** for diagnosing CAN bus communication issues on ESP32 boards. It provides a systematic, stage-by-stage test suite to isolate hardware failures from software issues.

## Purpose

This firmware is **NOT** part of the main OTS hardware controller. It is a **debugging tool** used to:

1. **Validate CAN transceiver wiring** before integrating with main firmware
2. **Test TWAI peripheral functionality** at the lowest level (no protocol overhead)
3. **Diagnose hardware issues** (bad transceiver, incorrect pins, missing termination)
4. **Verify bidirectional communication** between two ESP32 devices
5. **Measure TX pin voltage** to confirm signal integrity

**Use Case:** When CAN communication fails in `ots-fw-main` or `ots-fw-audiomodule`, flash this test firmware to the problematic board to determine if the issue is hardware-related.

## Supported Boards

### ESP32-S3 DevKit
- **TX Pin**: GPIO5
- **RX Pin**: GPIO4
- **Typical Use**: Main controller testing
- **PlatformIO Environment**: `esp32-s3`

### ESP32-A1S AudioKit
- **TX Pin**: GPIO5
- **RX Pin**: GPIO18
- **Typical Use**: Audio module testing
- **PlatformIO Environment**: `esp32-a1s`

## Test Stages

The firmware runs **4 sequential test stages** with detailed diagnostics:

### Stage 1: TWAI Initialization

**Purpose**: Verify TWAI driver can initialize and start

**Tests**:
- Install TWAI driver with 125 kbps timing
- Start TWAI peripheral
- Check status registers (TX/RX error counters)

**Expected Output**:
```
=== STAGE 1: TWAI INITIALIZATION ===
Board: ESP32-S3
TX Pin: GPIO5
RX Pin: GPIO4
Installing TWAI driver...
✓ Driver installed
Starting TWAI driver...
✓ Driver started
✓ TWAI Status: RUNNING
  TX Error Counter: 0
  RX Error Counter: 0
=== STAGE 1: PASSED ===
```

**Failure Modes**:
- Driver install fails → Hardware initialization issue (check ESP-IDF config)
- Driver start fails → Peripheral conflict or misconfiguration
- Status shows BUS_OFF → CAN bus error (usually wiring issue)

### Stage 2: Loopback Test

**Purpose**: Verify TWAI peripheral can transmit and receive internally

**Tests**:
- Reconfigure TWAI in NO_ACK mode (self-test)
- Send test frame (ID 0x123, 8 bytes)
- Attempt to receive same frame
- Validate data integrity

**Expected Output**:
```
=== STAGE 2: LOOPBACK TEST ===
Reconfiguring for loopback mode...
✓ Loopback mode active
Sending test frame...
✓ Frame transmitted
Waiting for RX...
✓ Frame received!
  ID: 0x123, DLC: 8
  Data: 01 02 03 04 05 06 07 08
=== STAGE 2: PASSED ===
```

**Failure Modes**:
- TX fails in NO_ACK mode → TWAI peripheral hardware issue
- RX timeout → Internal routing issue (rare, indicates chip defect)

**Note**: This stage does NOT require external wiring or transceiver. It tests the TWAI peripheral in isolation.

### Stage 3: TX Voltage Test

**Purpose**: Verify TX pin toggles voltage when transmitting (confirms transceiver input)

**Tests**:
- Reconfigure to NORMAL mode
- Send 10 frames (will fail without ACK, but TX pin should toggle)
- Monitor TX error counter
- **Manual verification required**: User measures GPIO voltage with multimeter/oscilloscope

**Expected Output**:
```
=== STAGE 3: TX VOLTAGE TEST ===
Reconfiguring for normal mode...
✓ Normal mode active

*** MEASURE GPIO5 WITH MULTIMETER/SCOPE ***
You should see voltage toggling during transmission attempts.
Sending 10 frames (will fail without ACK, but TX pin should toggle)...

TX #1 (ID 0x100)... FAIL (ESP_ERR_TIMEOUT)
TX #2 (ID 0x101)... FAIL (ESP_ERR_TIMEOUT)
...
TX #10 (ID 0x109)... FAIL (ESP_ERR_TIMEOUT)

TWAI Status after TX test:
  State: RUNNING
  TX Error Counter: 128
  RX Error Counter: 0

Did you measure voltage toggling on GPIO5? (y/n):
=== STAGE 3: MANUAL VERIFICATION REQUIRED ===
```

**What to Measure**:
- **Without transceiver**: GPIO should toggle between 0V and 3.3V (ESP32-S3) or 0V and 5V (ESP32-A1S logic level)
- **With transceiver**: Measure at transceiver TXD input (should see logic transitions)
- **On CAN bus**: CANH/CANL should show differential signaling

**Failure Modes**:
- No voltage toggling → GPIO misconfiguration or pin conflict
- Stuck HIGH/LOW → Driver not controlling pin
- TX frames return ESP_FAIL immediately → TWAI not started

### Stage 4: Two-Device Test

**Purpose**: Verify full bidirectional CAN communication between two devices

**Tests**:
- Send 5 frames (ID 0x201-0x205) every 2 seconds
- Listen for frames from second device
- Track TX/RX success counts
- Final statistics and status check

**Expected Output** (with second device):
```
=== STAGE 4: TWO-DEVICE TEST ===
Connect this device to another ESP32 with CAN transceiver.
Ensure CANH-CANH, CANL-CANL, common GND, 120Ω termination on each.

Sending 5 test frames every 2 seconds...
Listening for frames from other device...

[1] Sending ID 0x201... ✓ TX OK
[1] ✓ Received ID 0x202, DLC 8: AA BB 01 00 00 00 00 00
[2] Sending ID 0x202... ✓ TX OK
[2] ✓ Received ID 0x203, DLC 8: AA BB 02 00 00 00 00 00
...

=== FINAL STATISTICS ===
TX Success: 5
TX Failed: 0
RX Success: 5
TWAI State: RUNNING
TX Error Counter: 0
RX Error Counter: 0
=== STAGE 4: PASSED ===
```

**Failure Modes**:
- TX timeout → No ACK from second device (wiring issue, missing termination)
- RX timeout → Second device not transmitting or wrong CAN ID filter
- BUS_OFF state → Too many errors, check wiring and termination
- High error counters → Bitrate mismatch, noise, or bad transceiver

## Hardware Setup Requirements

### Minimal Test (Stages 1-3)
- ESP32 board (S3 or A1S)
- **NO transceiver required** for Stages 1-2
- Optional: Multimeter/oscilloscope for Stage 3

### Full Test (Stage 4)
- **Two ESP32 boards** (can be different types)
- **Two CAN transceivers** (SN65HVD230, MCP2551, or TJA1050)
- **120Ω termination resistor** at EACH end of bus
- **Common ground** between both devices
- Wiring:
  - Device 1 CANH → Device 2 CANH
  - Device 1 CANL → Device 2 CANL
  - GND → GND
  - 120Ω between CANH-CANL on Device 1
  - 120Ω between CANH-CANL on Device 2

### CAN Transceiver Connection

```
ESP32 GPIO (TX) ──→ Transceiver TXD
ESP32 GPIO (RX) ←── Transceiver RXD
                    
Transceiver CANH ──→ CAN Bus CANH
Transceiver CANL ──→ CAN Bus CANL
                    
Power:
ESP32 3.3V ──→ Transceiver VCC (for SN65HVD230)
ESP32 GND ──→ Transceiver GND
```

## Build & Flash

### Using PlatformIO (Recommended)

**ESP32-S3:**
```bash
cd ots-fw-can-hw-test
pio run -e esp32-s3 -t upload && pio device monitor
```

**ESP32-A1S:**
```bash
cd ots-fw-can-hw-test
pio run -e esp32-a1s -t upload && pio device monitor
```

### Using ESP-IDF

**ESP32-S3:**
```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

**ESP32-A1S:**
```bash
idf.py set-target esp32
idf.py build flash monitor
```

## Serial Output

Baud rate: **115200**

The firmware prints detailed diagnostics for each test stage. Monitor serial output to see:
- Test progress
- Success/failure indicators (✓ / ✗)
- Error messages with ESP-IDF error codes
- Statistics (TX/RX counts, error counters)
- Manual verification prompts

## Interpreting Results

### All Stages Pass
✅ **Hardware is good** → CAN communication issues are likely software/protocol related

### Stage 1 Fails
❌ **TWAI peripheral initialization issue**
- Check ESP-IDF version (requires v5.0+)
- Verify GPIO pins not conflicting with other peripherals
- Check platformio.ini for correct build flags

### Stage 2 Fails (Stage 1 passes)
❌ **TWAI internal routing issue**
- Rare: Usually indicates chip defect
- Try different GPIO pins
- Test with known-good ESP32 board

### Stage 3 No Voltage Toggling
❌ **GPIO pin not controlled**
- Verify pin definition matches platformio.ini
- Check for GPIO conflicts (I2C, SPI, UART on same pins)
- Use multimeter to measure voltage during test

### Stage 4 Fails
❌ **External wiring or transceiver issue**
- **TX fails**: Check CANH/CANL connections, verify termination resistors
- **RX fails**: Verify second device is transmitting, check common ground
- **BUS_OFF**: Too many errors, check bitrate (125 kbps), wiring integrity
- **High error counters**: Bitrate mismatch or electrical noise

## Troubleshooting Guide

### Problem: TX always fails with ESP_ERR_TIMEOUT

**Cause**: No ACK on CAN bus (expected in NORMAL mode without second device)

**Solution**: This is normal for Stage 3. Stage 4 requires a second device.

### Problem: BUS_OFF state immediately

**Cause**: CAN bus error (usually wiring)

**Fixes**:
1. Check termination: 120Ω at BOTH ends of bus
2. Verify CANH and CANL not swapped
3. Ensure common ground between devices
4. Check transceiver power (VCC should be 3.3V or 5V)

### Problem: Stage 2 passes, Stage 4 fails

**Cause**: TWAI peripheral works, but external bus has issues

**Fixes**:
1. Verify transceiver is powered and wired correctly
2. Measure voltage at CANH/CANL with scope (should show differential signaling)
3. Check second device is also running and transmitting
4. Try shorter CAN bus cable (< 1 meter for testing)

### Problem: RX always times out (Stage 4)

**Cause**: Not receiving frames from second device

**Fixes**:
1. Verify second device is transmitting (check its serial output)
2. Ensure both devices use same bitrate (125 kbps)
3. Check CAN filter (firmware uses ACCEPT_ALL, so not the issue)
4. Verify CANH-CANH and CANL-CANL connections (not crossed)

## Configuration

All configuration is in `platformio.ini` build flags:

```ini
[env:esp32-s3]
build_flags = 
    -DCAN_TX_GPIO=5        # TX pin
    -DCAN_RX_GPIO=4        # RX pin
    -DBOARD_NAME=\"ESP32-S3\"
```

To test different GPIO pins, edit these values and rebuild.

## CAN Bus Timing

**Bitrate**: 125 kbps (configurable in `main.c`)

To change bitrate, edit `test_init()`:
```c
// Change from:
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();

// To:
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
// or TWAI_TIMING_CONFIG_1MBITS()
```

## Differences from Main Firmware

This is **NOT** the production firmware. Key differences:

| Feature | ots-fw-can-hw-test | ots-fw-main / ots-fw-audiomodule |
|---------|-------------------|----------------------------------|
| **Purpose** | Hardware validation | Production system |
| **CAN Protocol** | None (raw TWAI frames) | Full CAN audio protocol |
| **Dependencies** | TWAI driver only | Full OTS stack (WebSocket, I2C, modules) |
| **Complexity** | ~300 lines | ~5000+ lines |
| **Use Case** | Debugging hardware | Running game |
| **Auto-runs** | Yes (runs all stages) | No (waits for commands) |

## When to Use This Firmware

**Use `ots-fw-can-hw-test` when:**
- ✅ Setting up new CAN hardware for the first time
- ✅ Debugging "CAN module not found" errors
- ✅ Verifying transceiver wiring before production deployment
- ✅ Testing GPIO pin changes
- ✅ Diagnosing intermittent CAN bus errors
- ✅ Validating hardware repairs

**Do NOT use for:**
- ❌ Normal gameplay (use `ots-fw-main`)
- ❌ Audio module operation (use `ots-fw-audiomodule`)
- ❌ Protocol development (use `ots-fw-cantest` for that)

## Related Tools

- **ots-fw-cantest**: Interactive CAN protocol testing tool (monitor/simulator modes)
- **ots-fw-main**: Main controller firmware with CAN discovery
- **ots-fw-audiomodule**: Audio module firmware with CAN sound control

## Code Structure

```
ots-fw-can-hw-test/
  platformio.ini      # Build configuration (GPIO pins, board selection)
  CMakeLists.txt      # ESP-IDF project file
  src/
    main.c            # Test implementation (4 stages)
  sdkconfig.*         # ESP-IDF config per board
```

**Single-file firmware**: All logic is in `main.c` for simplicity. No external dependencies beyond ESP-IDF TWAI driver.

## Exit Behavior

After all stages complete, the firmware prints final statistics and **stops**. It does not loop or continue running.

To re-run tests:
1. Press reset button on ESP32
2. Or reflash: `pio run -e <env> -t upload`

## Success Criteria

**Minimum for "hardware OK":**
- ✅ Stage 1 passes (TWAI initializes)
- ✅ Stage 2 passes (loopback works)
- ✅ Stage 3 shows voltage toggling (manual verification)

**Full validation:**
- ✅ All above PLUS Stage 4 passes with second device

If Stages 1-3 pass but Stage 4 fails → **Check external wiring, termination, and second device**

---

**Last Updated**: January 7, 2026  
**Firmware Type**: Debugging/validation tool (non-production)  
**Platforms**: ESP32-S3, ESP32-A1S (ESP-IDF framework)  
**Complexity**: Minimal (bare-metal TWAI testing)

