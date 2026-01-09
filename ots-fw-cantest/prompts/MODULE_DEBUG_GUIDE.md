# TJA1050 Module Hardware Debug (BUS_OFF Issue)

**Date**: January 6, 2026  
**Setup**: Pre-made TJA1050 module (not bare chip)  
**Problem**: TWAI State = BUS_OFF (excessive transmission errors)  
**Voltage**: 3.3V operation

## Pre-Made Module Considerations

Your module likely has:
- S (standby) pin internally pulled to GND → ✓ Already handled
- Built-in decoupling capacitors → ✓ Already handled  
- 3.3V/5V level compatibility (check datasheet) → ⚠️ Verify

## Root Cause Analysis for BUS_OFF

With module (S pin handled), BUS_OFF is caused by:

### 1. **Missing Termination Resistors** ← 95% probability

**Why it causes BUS_OFF**:
```
Without termination:
- Signal reflections on bus
- TWAI controller sends frame
- Reflection causes bit errors
- Error counter increments
- After 255+ errors → BUS_OFF state
```

**Required**:
```
120Ω resistor between CANH and CANL on EACH board
(Total: Two resistors, one per device)

Wiring:
  ESP32-S3: CANH ──┬── 120Ω resistor ──┬── CANL
  ESP32-A1S: CANH ──┴── 120Ω resistor ──┴── CANL
```

**How to verify**:
```bash
# Disconnect one device
# Measure resistance between CANH and CANL on remaining device
Expected: 120Ω
If infinity (open): Missing resistor
If ~60Ω: Two resistors in parallel (correct for 2-node network)
```

**Quick test without resistors**:
```
1. Disconnect CANH and CANL between devices
2. On ONE device only, short CANH to CANL with wire
3. Check TWAI state
   - If RUNNING: Confirms termination is the issue
   - If BUS_OFF: Other problem exists
```

### 2. **3.3V Operation Issues** ← 30% probability

Some TJA1050 modules have onboard voltage regulators or level shifters:

**Check module specifications**:
```
[ ] Module designed for 3.3V operation?
[ ] Module has 5V→3.3V regulator onboard?
[ ] Module has level shifters for TXD/RXD?

Common modules:
- Waveshare CAN module: Requires 5V input
- Generic blue PCB modules: Usually 5V only
- MCP2515 + TJA1050: Often 5V only
```

**If module requires 5V**:
```
Option A: Power module with 5V, use level shifters for TXD/RXD
  ESP32 TX (3.3V) → Level shifter → TJA1050 TXD (5V tolerant)
  ESP32 RX (3.3V) ← Level shifter ← TJA1050 RXD (5V output)

Option B: Replace with 3.3V-compatible module
  Recommended: SN65HVD230 modules (designed for 3.3V)
```

### 3. **Cable Length / Quality** ← 10% probability

At 500kbps without termination, even short cables cause issues:

```
[ ] Cable length < 50cm (recommended < 20cm for testing)
[ ] Using twisted pair or shielded cable
[ ] Solid connections (not breadboard jumpers)
[ ] CANH and CANL not swapped
```

**For testing**:
- Use shortest possible cables (10-20cm)
- Twist CANH/CANL wires together
- Avoid breadboard connections if possible

## Diagnostic Procedure

### Test 1: Verify Bus Differential Signals (Oscilloscope)

If you have an oscilloscope:

```
1. Trigger on TXD pin activity (ESP32 TX GPIO)
2. Probe CANH and CANL simultaneously
3. Send CAN frame (enter controller mode, send discovery)

Expected WITH termination:
  CANH: Toggles between ~2.5V (recessive) and ~3.5V (dominant)
  CANL: Toggles between ~2.5V (recessive) and ~1.5V (dominant)
  Clean square waves, ~2V peak-to-peak differential

Expected WITHOUT termination:
  Ringing/overshoots on signals
  Reflections visible
  Poor signal quality → bit errors → BUS_OFF
```

### Test 2: Measure Bus Resistance

```bash
# With both devices connected but powered OFF:
Measure resistance between CANH and CANL

Expected results:
- Two 120Ω resistors: ~60Ω
- One 120Ω resistor: ~120Ω
- No resistors: Very high (MΩ range)

If high resistance: ADD TERMINATION RESISTORS
```

### Test 3: Single Device Test with Loopback

```bash
# On ONE device only (ESP32-S3):
1. Disconnect CAN bus from other device
2. Add 120Ω resistor between CANH and CANL
3. Power on device
4. Run: python3 test_hardware.sh /dev/ttyACM1
5. Check TWAI state

Expected: Should stay in RUNNING (might not RX/TX but won't go BUS_OFF)
If BUS_OFF: Module itself has issues
```

## Quick Fixes (Priority Order)

### Fix 1: Add Termination Resistors (5 minutes)

**Required materials**:
- Two 120Ω resistors (1/4W or 1/8W, ±5% tolerance OK)
- Or: One 60Ω resistor if testing single device

**Installation**:
```
Device 1 (ESP32-S3):
  Solder/connect 120Ω resistor between:
    - TJA1050 module CANH pin
    - TJA1050 module CANL pin

Device 2 (ESP32-A1S):
  Solder/connect 120Ω resistor between:
    - TJA1050 module CANH pin
    - TJA1050 module CANL pin
```

**Alternative for testing** (if no 120Ω resistors available):
- Use two 220-270Ω resistors in parallel (~110-135Ω equivalent)
- Use 100Ω resistors (not ideal but works for short cables)

### Fix 2: Reduce Bitrate (2 minutes)

If termination doesn't help, try slower speed:

```c
// In can_driver.c or cantest config
can_config.bitrate = 250000;  // Change from 500000 to 250000

Lower bitrate = more tolerance for poor signal quality
```

### Fix 3: Shorten Cables (2 minutes)

```
Replace 30cm cables with 10cm cables
Twist CANH and CANL together
Use solid core wire instead of jumper wires
```

### Fix 4: Check Module Power (1 minute)

```bash
# Measure module VCC pin with multimeter while transmitting
Expected: 3.25-3.35V (stable, no drops)
If drops below 3.0V: Add 100µF capacitor near module VCC
```

## Module-Specific Checks

### Unknown Module Type

Take a photo of your module and check for:

```
[ ] Chip markings on module (TJA1050, SN65HVD230, MCP2551?)
[ ] Voltage regulator IC present? (indicates 5V input required)
[ ] Level shifter ICs present? (indicates voltage translation)
[ ] Jumpers or solder bridges? (may need configuration)
[ ] Pin labels (some modules have EN/STBY pins exposed)
```

### Common Module Configurations

**Type A: Simple 6-pin module**
```
Pins: VCC, GND, TX, RX, CANH, CANL
Usually: Direct connection to TJA1050
S pin: Hardwired to GND on PCB
Works at: 5V typically, 3.3V marginal
```

**Type B: MCP2515 + TJA1050 combo**
```
Pins: VCC, GND, MOSI, MISO, SCK, CS, INT, CANH, CANL
This is SPI-based CAN controller + transceiver
Different from TWAI (won't work with our code)
```

**Type C: SN65HVD230 module**
```
Pins: VCC, GND, TX, RX, CANH, CANL
Native 3.3V operation
Best choice for ESP32
```

## Expected Results After Fix

Once termination added:

```
✅ TWAI State: RUNNING (not BUS_OFF)
✅ No state changes after 10+ seconds
✅ TX count increases when sending
✅ RX count increases when receiving
✅ Bus error count: 0 or very low
```

## If Still BUS_OFF After Termination

1. **Try slower bitrate**: 250kbps or 125kbps
2. **Measure bus signals**: Use oscilloscope to check quality
3. **Try different cables**: Shorter, twisted pair
4. **Check module compatibility**: May need 5V or different transceiver
5. **Test each module individually**: One might be damaged

## Module Replacement Recommendation

If TJA1050 @ 3.3V continues to have issues:

**Best option**: SN65HVD230 module (~$2-5)
- Native 3.3V operation
- Drop-in replacement (same pinout)
- Better signal quality at 3.3V
- Up to 1Mbps capable at 3.3V

**Where to buy**:
- AliExpress: "SN65HVD230 CAN module"
- Amazon: "SN65HVD230 CAN transceiver"
- DigiKey/Mouser: Bare chips (~$1) + breakout board

---

## Next Steps

**Immediate** (now):
1. ⚡ **Add 120Ω resistors** between CANH-CANL on both boards
2. ⚡ Power cycle both devices
3. ⚡ Check TWAI state with `python3 test_hardware.sh /dev/ttyACM1`

**If still BUS_OFF**:
1. 📋 Try 250kbps bitrate
2. 📋 Test one device alone with termination
3. 📋 Check module specifications (5V vs 3.3V)

**Success = TWAI stays in RUNNING state** ✓
