# CAN Bus Fresh Start Verification

## Date: January 6, 2026

Complete this checklist step-by-step to verify CAN bus setup from scratch.

---

## Phase 1: Hardware Physical Verification

### 1.1 Power Supply
```
[ ] ESP32-S3: Powered via USB, 3.3V stable
[ ] ESP32-A1S: Powered via USB, 3.3V stable
[ ] TJA1050 Module 1: VCC = 3.3V measured
[ ] TJA1050 Module 2: VCC = 3.3V measured
[ ] GND connected between all devices
```

**Measure with multimeter:**
- VCC on each TJA1050 module: Expected **3.3V ±0.1V**

### 1.2 GPIO Connections (ESP32 → TJA1050)

**ESP32-S3:**
```
[ ] GPIO 5 (ESP32) → TXD pin (TJA1050 Module 1)
[ ] GPIO 4 (ESP32) → RXD pin (TJA1050 Module 1)
[ ] Connections are solid (no breadboard if possible)
```

**ESP32-A1S:**
```
[ ] GPIO 18 (ESP32) → TXD pin (TJA1050 Module 2)
[ ] GPIO 19 (ESP32) → RXD pin (TJA1050 Module 2)
[ ] Connections are solid (no breadboard if possible)
```

**IMPORTANT:** ESP32 TX goes to TJA1050 TXD (NOT crossed)

### 1.3 CAN Bus Wiring (Between Modules)
```
[ ] CANH (Module 1) → CANH (Module 2)
[ ] CANL (Module 1) → CANL (Module 2)
[ ] Cable length < 20cm for testing
[ ] Wires twisted together or shielded cable
[ ] No swaps between CANH and CANL
```

### 1.4 Termination Verification
```
[ ] Measure resistance between CANH-CANL with power OFF
    Module 1 alone: _____ Ω (should be ~120Ω)
    Module 2 alone: _____ Ω (should be ~120Ω)

[ ] Measure resistance with BOTH modules connected, power OFF
    Total: _____ Ω (should be ~60Ω = 120Ω || 120Ω)

[ ] Measure resistance with BOTH modules connected, power ON
    Total: _____ Ω (should be ~60Ω)
```

**Expected Values:**
- Single module: **120Ω** (built-in termination)
- Both modules: **60Ω** (correct for CAN bus!)

---

## Phase 2: Firmware Verification

### 2.1 Driver Configuration Check

Run this to check current driver settings:
```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-shared/components/can_driver
grep -A 5 "TWAI_MODE" can_driver.c | head -20
```

**Verify:**
```
[ ] Line ~58: Uses TWAI_MODE_NO_ACK (loopback) or TWAI_MODE_NORMAL
[ ] NOT using TWAI_MODE_LISTEN_ONLY
```

### 2.2 GPIO Pin Configuration

Check GPIO pins in firmware:
```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-cantest
grep -E "(tx_gpio|rx_gpio|GPIO)" src/main.c | grep -v "//"
```

**Verify:**
```
[ ] ESP32-S3: tx_gpio=5, rx_gpio=4
[ ] ESP32-A1S: tx_gpio=18, rx_gpio=19
```

### 2.3 Bitrate Configuration

```bash
grep "bitrate" src/main.c
```

**Current Setting:**
```
[ ] Bitrate = 125000 (125 kbps)
```

### 2.4 Loopback Mode Setting

```bash
grep "loopback" src/main.c
```

**Current Setting:**
```
[ ] loopback = false (normal mode, for two devices)
[ ] loopback = true (NO_ACK mode, single device - will fail with 120Ω modules)
```

---

## Phase 3: Build & Flash Verification

### 3.1 Clean Build Both Boards

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-cantest

# Clean build ESP32-S3
pio run -e esp32-s3-devkit -t clean
pio run -e esp32-s3-devkit

# Clean build ESP32-A1S
pio run -e esp32-a1s-audiokit -t clean
pio run -e esp32-a1s-audiokit
```

**Verify:**
```
[ ] Both builds succeed without errors
[ ] Flash size warnings OK (not critical)
```

### 3.2 Flash Both Boards

```bash
# Flash ESP32-S3
pio run -e esp32-s3-devkit -t upload

# Flash ESP32-A1S
pio run -e esp32-a1s-audiokit -t upload
```

**Verify:**
```
[ ] ESP32-S3 flashed successfully
[ ] ESP32-A1S flashed successfully
```

### 3.3 Verify Serial Ports

```bash
ls -la /dev/tty* | grep -E "(ACM|USB)"
```

**Expected:**
```
[ ] /dev/ttyACM0 or /dev/ttyACM1 (ESP32-S3)
[ ] /dev/ttyUSB0 or /dev/ttyUSB1 (ESP32-A1S)
```

---

## Phase 4: Initial Boot Check

### 4.1 Monitor ESP32-S3 Boot

```bash
pio device monitor --port /dev/ttyACM1 --baud 115200 --filter direct
```

**Look for these logs:**
```
[ ] "OTS CAN Test Firmware v0.2.0"
[ ] "Initializing CAN driver..."
[ ] "Using 125 kbps bitrate"
[ ] "Normal mode" or "Loopback mode ENABLED"
[ ] "CAN driver initialized"
[ ] NO "Failed to install TWAI driver"
[ ] NO "Falling back to MOCK mode"
```

**Press 't' to check status:**
```
[ ] Shows statistics table
[ ] Shows TWAI Peripheral Status
[ ] State: RUNNING (not BUS_OFF or STOPPED)
[ ] TX Error Counter: 0
[ ] RX Error Counter: 0
```

### 4.2 Monitor ESP32-A1S Boot

```bash
pio device monitor --port /dev/ttyUSB1 --baud 115200 --filter direct
```

**Verify same logs as above**

---

## Phase 5: Single Device Test (Will Fail - Expected!)

### 5.1 Test One Device Alone

With only ESP32-A1S connected:
```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-cantest
python3 tools/test_loopback_single.py /dev/ttyUSB1
```

**Expected Result (FAILURE):**
```
[ ] State: RUNNING initially
[ ] After sending message:
    State: BUS_OFF
    TX Error Counter: 128+
    Bus Errors: 16+
```

**This is CORRECT behavior!** Single device with 120Ω termination cannot work in loopback.

**Why it fails:**
- TWAI_MODE_NO_ACK still uses physical bus
- 120Ω alone is wrong impedance
- Signal reflections cause bit errors
- Error counter exceeds 256 → BUS_OFF

---

## Phase 6: Two Device Test (Should Work!)

### 6.1 Verify Both Devices Connected

```bash
ls -la /dev/tty* | grep -E "(ACM|USB)"
```

**Must see both ports:**
```
[ ] /dev/ttyACM1 (ESP32-S3)
[ ] /dev/ttyUSB1 (ESP32-A1S)
```

### 6.2 Check TWAI Status on Both

**Device 1:**
```bash
# Connect to ESP32-S3, press 't' for status
pio device monitor --port /dev/ttyACM1
```

**Device 2:**
```bash
# Connect to ESP32-A1S, press 't' for status
pio device monitor --port /dev/ttyUSB1
```

**Verify BOTH show:**
```
[ ] State: RUNNING (not BUS_OFF)
[ ] TX Error Counter: 0
[ ] RX Error Counter: 0
[ ] Bus Errors: 0
```

If either shows BUS_OFF at boot, something is wrong with wiring.

### 6.3 Run Two-Device Communication Test

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-cantest
python3 tools/test_two_device.py --verbose
```

**Expected Results:**
```
[ ] Both devices detected
[ ] Discovery phase:
    - S3 sends MODULE_QUERY (0x411)
    - A1S responds with MODULE_ANNOUNCE (0x410)
[ ] Audio protocol test:
    - S3 sends PLAY_SOUND (0x420)
    - A1S responds with PLAY_SOUND_ACK (0x423)
[ ] RX count > 0 on BOTH devices
[ ] TX count > 0 on BOTH devices
[ ] State: RUNNING on both (no BUS_OFF)
[ ] TX/RX Error Counters stay at 0
```

### 6.4 Manual Interactive Test

**Terminal 1 (ESP32-S3):**
```bash
pio device monitor --port /dev/ttyACM1
# Press 'c' for controller mode
# Press '1' to send discovery
# Watch for RX messages
```

**Terminal 2 (ESP32-A1S):**
```bash
pio device monitor --port /dev/ttyUSB1
# Press 'a' for audio module simulator
# Watch for discovery query
# Should auto-respond with MODULE_ANNOUNCE
```

**Success Indicators:**
```
[ ] S3 shows: "TX: MODULE_QUERY" and "RX: MODULE_ANNOUNCE"
[ ] A1S shows: "RX: MODULE_QUERY" and "TX: MODULE_ANNOUNCE"
[ ] Messages RX count increments on both
[ ] No BUS_OFF state
[ ] No error counter increases
```

---

## Phase 7: Signal Quality Check (Optional)

### 7.1 Voltage Measurements

**With bus idle (no transmission):**
```
Measure CANH: _____ V (should be ~2.5V recessive)
Measure CANL: _____ V (should be ~2.5V recessive)
Differential: _____ V (should be ~0V)
```

**During transmission (hard to catch without scope):**
```
CANH dominant: Should go to ~3.5V
CANL dominant: Should go to ~1.5V
Differential: ~2V
```

### 7.2 With Oscilloscope (If Available)

```
[ ] Trigger on TXD pin going low
[ ] Probe CANH and CANL
[ ] Verify clean square waves
[ ] No excessive ringing or overshoot
[ ] Rise/fall times < 100ns
```

---

## Troubleshooting Decision Tree

### If BUS_OFF occurs with TWO devices:

**Check:**
1. Wiring: CANH-CANH, CANL-CANL (not swapped)
2. Termination: Should measure 60Ω between CANH-CANL
3. Power: 3.3V stable on both TJA1050 modules
4. Cable: < 20cm, no breaks
5. Bitrate: Both devices same (125 kbps)

### If No RX Messages (but State: RUNNING):

**Check:**
1. GPIO RX pin connected correctly
2. TJA1050 RXD pin has signal (measure with scope/multimeter)
3. Filter configuration (should be ACCEPT_ALL)
4. Both devices in compatible modes (both NORMAL)

### If TX Error Counter Increases:

**Indicates:**
- Physical bus problem (reflections, termination)
- Other device not acknowledging (check it's running)
- Bitrate mismatch between devices

### If RX Error Counter Increases:

**Indicates:**
- Receiving garbled data
- Bitrate mismatch
- Noise on bus

---

## Success Criteria

### ✅ System is Working When:

```
✓ Both devices show State: RUNNING
✓ TX/RX Error Counters: 0 on both
✓ Messages successfully sent AND received
✓ RX count > 0 on BOTH devices after test
✓ Discovery protocol works (QUERY → ANNOUNCE)
✓ Audio protocol works (PLAY_SOUND → ACK)
✓ No BUS_OFF state transitions
```

---

## Quick Command Reference

```bash
# Check serial ports
ls -la /dev/tty* | grep -E "(ACM|USB)"

# Monitor device
pio device monitor --port /dev/ttyACM1

# Get TWAI status (press 't' in monitor)

# Flash device
pio run -e esp32-s3-devkit -t upload
pio run -e esp32-a1s-audiokit -t upload

# Run tests
python3 tools/test_loopback_single.py /dev/ttyUSB1
python3 tools/test_two_device.py --verbose

# Check driver code
grep "TWAI_MODE" ../ots-fw-shared/components/can_driver/can_driver.c
```

---

## Current Configuration Summary

**Bitrate:** 125 kbps  
**Mode:** Normal (loopback=false)  
**Termination:** 120Ω per module = 60Ω total (correct)  
**Power:** 3.3V (out of TJA1050 spec but should work)  
**Cable:** 10cm twisted pair  

**Known Limitations:**
- Single device testing NOT possible with WWZMDiB modules
- TJA1050 designed for 5V but works at 3.3V (marginal)
- ESP32 TWAI has NO true internal loopback

**Next Test:** Two-device communication test with both boards connected
