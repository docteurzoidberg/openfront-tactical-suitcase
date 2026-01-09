# TJA1050 Hardware Debug Checklist (3.3V Operation)

**Date**: January 6, 2026  
**Problem**: TWAI State = BUS_OFF (controller disabled due to excessive errors)  
**Root Cause**: Physical CAN bus layer not working properly

## TJA1050 @ 3.3V Specific Issues

### Pin Verification (CRITICAL)

#### Pin 8 - S (Standby) ⚠️ **MOST COMMON ISSUE**
```
[ ] Pin 8 (S) is connected to GND (0V)
[ ] Pin 8 is NOT floating
[ ] Pin 8 is NOT connected to VCC

Measurement: Use multimeter on pin 8
Expected: 0V (ground)
If HIGH or floating: Transceiver is in STANDBY mode and won't work!
```

**Why it matters**: When S pin is HIGH or floating, the transceiver enters low-power standby mode and disables TX/RX. This is the #1 reason for BUS_OFF with TJA1050.

#### Pin 1 - TXD (ESP32 → TJA1050)
```
[ ] Connected to ESP32 TX GPIO
[ ] Pin shows activity when transmitting (oscilloscope: toggles between 0V-3.3V)
[ ] NOT swapped with RXD

Board: ESP32-S3 → GPIO 5
Board: ESP32-A1S → GPIO 18
```

#### Pin 4 - RXD (TJA1050 → ESP32)
```
[ ] Connected to ESP32 RX GPIO
[ ] Pin shows activity when CAN traffic present
[ ] NOT swapped with TXD

Board: ESP32-S3 → GPIO 4
Board: ESP32-A1S → GPIO 19
```

#### Pin 3 - VCC (3.3V Power)
```
[ ] Connected to 3.3V supply
[ ] Measure voltage: Should be 3.25V - 3.35V
[ ] Check with multimeter while transmitting (no voltage drop)

⚠️ TJA1050 datasheet specifies 4.75V-5.25V typical
   3.3V operation is out of spec but often works
   May have reduced noise margins
```

#### Pin 2 - GND
```
[ ] Connected to common ground with ESP32
[ ] Low resistance path to ESP32 GND (< 1Ω)
[ ] No floating ground
```

#### Pin 5 - VREF (Reference Voltage Output)
```
[ ] NOT CONNECTED to anything (leave floating or 100nF cap to GND)
[ ] Measure voltage: Should be ~1.65V (VCC/2)
[ ] If 0V or 3.3V: Transceiver likely damaged

Expected at 3.3V: ~1.65V output
```

#### Pin 6 - CANL (CAN Low)
```
[ ] Connected to other device's CANL
[ ] 120Ω termination resistor between CANH and CANL
[ ] Measure idle voltage: ~2.5V (mid-rail)
[ ] Check with oscilloscope: Should show differential signals during TX
```

#### Pin 7 - CANH (CAN High)
```
[ ] Connected to other device's CANH
[ ] 120Ω termination resistor between CANH and CANL
[ ] Measure idle voltage: ~2.5V (mid-rail)
[ ] Check with oscilloscope: Should show differential signals during TX
```

### Bus Termination (REQUIRED)

```
[ ] 120Ω resistor between CANH and CANL on ESP32-S3 board
[ ] 120Ω resistor between CANH and CANL on ESP32-A1S board
[ ] Total bus resistance: ~60Ω (two 120Ω in parallel)

Without termination: Reflections → transmission errors → BUS_OFF
```

**How to test without resistors**:
- Temporarily short CANH and CANL together
- Check if TWAI stays in RUNNING state (won't work for communication but tests transceiver TX)

### 3.3V Operation Considerations

#### Logic Level Compatibility
```
TJA1050 TXD input (pin 1):
  - VIL (low): < 0.3 × VCC = < 0.99V  ✓ ESP32 outputs 0V
  - VIH (high): > 0.7 × VCC = > 2.31V  ✓ ESP32 outputs 3.3V

TJA1050 RXD output (pin 4):
  - VOL (low): < 0.2 × VCC = < 0.66V  ✓ ESP32 reads as LOW
  - VOH (high): > 0.8 × VCC = > 2.64V  ✓ ESP32 reads as HIGH
```

At 3.3V, logic levels are marginal but should work. If issues persist, consider:
- Using SN65HVD230 (designed for 3.3V operation)
- Adding level shifters
- Using 5V-tolerant TJA1050 variant with proper level translation

#### Power Supply Quality
```
[ ] 3.3V rail stable (< 100mV ripple)
[ ] 100nF ceramic cap close to VCC pin
[ ] Check voltage doesn't drop during TX bursts
[ ] If using 5V→3.3V regulator, ensure adequate current (>100mA)
```

### Wiring Verification

```
ESP32-S3 Controller:
  GPIO 5 → TJA1050 Pin 1 (TXD)
  GPIO 4 → TJA1050 Pin 4 (RXD)
  3.3V   → TJA1050 Pin 3 (VCC)
  GND    → TJA1050 Pin 2 (GND)
  GND    → TJA1050 Pin 8 (S - STANDBY) ⚠️ CRITICAL

ESP32-A1S Audio:
  GPIO 18 → TJA1050 Pin 1 (TXD)
  GPIO 19 → TJA1050 Pin 4 (RXD)
  3.3V    → TJA1050 Pin 3 (VCC)
  GND     → TJA1050 Pin 2 (GND)
  GND     → TJA1050 Pin 8 (S - STANDBY) ⚠️ CRITICAL

Bus Connection:
  ESP32-S3 CANH → ESP32-A1S CANH
  ESP32-S3 CANL → ESP32-A1S CANL
  120Ω between CANH-CANL on BOTH boards
```

## Diagnostic Tests

### Test 1: Check Transceiver Active Mode
```bash
# Measure pin 8 (S) voltage with multimeter
Expected: 0V (GND)
If 3.3V or floating: TRANSCEIVER IS IN STANDBY - THIS IS THE PROBLEM!

Fix: Connect pin 8 to GND with wire
```

### Test 2: Check VREF Output
```bash
# Measure pin 5 (VREF) voltage
Expected: ~1.65V (VCC/2)
If 0V: Transceiver may be damaged or in standby
If 3.3V: Transceiver may be damaged
```

### Test 3: Check Bus Idle Voltages
```bash
# Measure CANH and CANL when idle (no transmission)
Expected: Both ~2.5V (pulled to mid-rail by termination)
If 0V or 3.3V: Check termination resistors
If different voltages: Possible short or wrong connection
```

### Test 4: Check TX Activity
```bash
# Use oscilloscope on TXD pin (pin 1) while transmitting
Expected: Square wave toggles between 0V-3.3V at bitrate (500kbps)
If no activity: ESP32 GPIO not connected or not configured
If constant HIGH or LOW: Check ESP32 TX GPIO configuration
```

### Test 5: Check Differential Bus Signals
```bash
# Use oscilloscope on CANH and CANL while transmitting
Expected: Complementary signals, ~2V peak-to-peak differential
CANH: Toggles between ~2.5V (recessive) and ~3.5V (dominant)
CANL: Toggles between ~2.5V (recessive) and ~1.5V (dominant)

If no signals: Check standby pin (8) is grounded
If small signals (< 0.5V): Check termination resistors
```

## Quick Fix Steps (Priority Order)

### Step 1: Ground Standby Pin (CRITICAL - 2 minutes)
```
1. Locate pin 8 (S) on TJA1050
2. Connect pin 8 directly to GND with wire
3. Verify with multimeter: Pin 8 = 0V
4. Retest - if BUS_OFF clears, this was the problem!
```

### Step 2: Add Termination Resistors (5 minutes)
```
1. Get two 120Ω resistors (1/4W or 1/8W)
2. Solder/connect between CANH and CANL on EACH board
3. Verify with multimeter: ~60Ω between any CANH-CANL pair
4. Retest
```

### Step 3: Verify Power Supply (2 minutes)
```
1. Measure VCC pin (pin 3): Should be 3.25-3.35V
2. Measure VREF pin (pin 5): Should be ~1.65V
3. If wrong, check 3.3V regulator or power wiring
```

### Step 4: Verify GPIO Connections (5 minutes)
```
1. Continuity test: ESP32 GPIO → TJA1050 TXD pin
2. Continuity test: ESP32 GPIO → TJA1050 RXD pin
3. Verify no swaps (TX↔RX)
4. Check no shorts between pins
```

## Expected Results After Fixes

Once hardware is correct:
```
TWAI State: RUNNING (not BUS_OFF)
TX count: Increases when sending
RX count: Increases when receiving
Bus errors: 0 or very low
```

If still BUS_OFF after all fixes:
- Try shorter cable (< 1m)
- Try slower bitrate (250kbps instead of 500kbps)
- Check for noise/interference on bus
- Consider replacing transceivers (may be damaged from incorrect wiring)

## Alternative: Use 3.3V-Native Transceiver

If TJA1050 @ 3.3V continues to have issues:
```
Recommended: SN65HVD230 (TI)
- Designed for 3.3V operation
- Pin-compatible with TJA1050
- Better noise margins at 3.3V
- 1Mbps capable at 3.3V

Cost: ~$1-2 per chip
Availability: DigiKey, Mouser, AliExpress
```

## Common Mistakes

❌ **Standby pin (8) floating** → Transceiver disabled  
❌ **No termination resistors** → Reflections cause errors  
❌ **TX/RX pins swapped** → No communication  
❌ **5V on VCC pin** → May damage ESP32 GPIOs via RXD  
❌ **VREF connected to something** → Transceiver malfunction  
❌ **Poor ground connection** → Random errors  
❌ **Long untwisted wires** → Noise pickup at 3.3V  

## Success Criteria

✅ Pin 8 (S) = 0V (grounded)  
✅ Pin 5 (VREF) = ~1.65V  
✅ Termination = 60Ω total (two 120Ω resistors)  
✅ TWAI State = RUNNING  
✅ Bus errors = 0  
✅ TX and RX counts increasing  

---

**Next Step**: Focus on verifying Pin 8 (S) is grounded - this is the #1 cause of BUS_OFF with TJA1050.
