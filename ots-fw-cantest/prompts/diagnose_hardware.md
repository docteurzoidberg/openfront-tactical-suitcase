# CAN Bus Hardware Diagnostic Guide

## Symptom

- Controller TWAI: RUNNING, TX count increases
- Audio TWAI: RUNNING, RX count stays at 0
- No bus errors on either device
- Both devices report healthy TWAI peripheral state

This indicates a **physical layer issue** - the TWAI peripherals work, but signals aren't reaching between devices.

## Physical Layer Checklist

### 1. CAN Transceiver Power (Most Common Issue)

**Check:**
- Measure VCC pin on both transceivers with multimeter
- Should read 3.3V (for SN65HVD230) or 5V (for TJA1050/MCP2551)

**If 0V:**
- Transceiver is not powered
- Check power wiring from ESP32 to transceiver VCC
- Check GND connection

### 2. CAN Bus Physical Connections

**Check:**
- CAN_H from controller transceiver connected to CAN_H on audio transceiver
- CAN_L from controller transceiver connected to CAN_L on audio transceiver
- Both transceivers share common GND

**Test with multimeter (continuity mode):**
```
Controller CAN_H <---> Audio CAN_H (should beep)
Controller CAN_L <---> Audio CAN_L (should beep)
Controller GND   <---> Audio GND   (should beep)
```

### 3. GPIO Pin Connections

**Controller (ESP32-S3):**
- GPIO5 (TX) → CAN Transceiver TXD pin
- GPIO4 (RX) ← CAN Transceiver RXD pin

**Audio (ESP32-A1S):**
- GPIO5 (TX) → CAN Transceiver TXD pin
- GPIO18 (RX) ← CAN Transceiver RXD pin

**IMPORTANT:** TX goes TO transceiver TXD (NOT crossed). RX comes FROM transceiver RXD (NOT crossed).

**Test with multimeter (continuity mode with device powered off):**
```
ESP32-S3 GPIO5  <---> Transceiver TXD (should beep)
ESP32-S3 GPIO4  <---> Transceiver RXD (should beep)
ESP32-A1S GPIO5  <---> Transceiver TXD (should beep)
ESP32-A1S GPIO18 <---> Transceiver RXD (should beep)
```

### 4. Bus Termination

**Required:**
- 120Ω resistor across CAN_H and CAN_L at EACH end of the bus
- For 2-device setup: One 120Ω at controller end, one at audio end

**Test with multimeter (resistance mode, devices powered off):**
```
Measure between CAN_H and CAN_L: Should read ~60Ω (two 120Ω in parallel)
```

**If reading 120Ω:** Only one terminator present  
**If reading ∞ (open):** No terminators present  
**If reading 60Ω:** Correct! ✓

### 5. Transceiver Type Mismatch

**Common Issue:** 3.3V transceiver (SN65HVD230) powered from 5V rail

**Check:**
- SN65HVD230: Must use 3.3V supply
- TJA1050/MCP2551: Must use 5V supply

**If mismatch:** Transceiver may appear to work (TWAI sees it) but won't communicate

### 6. Signal Quality Check (Advanced)

**With oscilloscope:**
- Probe CAN_H and CAN_L during transmission
- Should see differential signal (CAN_H high when CAN_L low)
- Voltage swing: ~2V differential

**Without oscilloscope:**
- Measure CAN_H and CAN_L with multimeter in DC voltage mode during idle
- Typical idle: CAN_H ~2.5V, CAN_L ~2.5V
- During transmission: voltages should change

## Quick Hardware Test Procedure

1. **Power Off Both Devices**

2. **Check Continuity:**
   ```
   Controller CAN_H <-> Audio CAN_H
   Controller CAN_L <-> Audio CAN_L
   CAN_H <-> CAN_L (should NOT beep - they're NOT shorted)
   ```

3. **Check Termination:**
   ```
   Measure resistance across CAN_H and CAN_L: ~60Ω
   ```

4. **Power On, Check Voltages:**
   ```
   Controller Transceiver VCC: 3.3V or 5V (depending on type)
   Audio Transceiver VCC: 3.3V or 5V
   CAN_H idle: ~2.5V
   CAN_L idle: ~2.5V
   ```

5. **Check GPIO Connections (Power Off First):**
   ```
   ESP32-S3 GPIO5 <-> Controller Transceiver TXD
   ESP32-S3 GPIO4 <-> Controller Transceiver RXD
   ESP32-A1S GPIO5 <-> Audio Transceiver TXD
   ESP32-A1S GPIO18 <-> Audio Transceiver RXD
   ```

## Common Fixes

### Issue: Transceiver Not Powered
**Solution:** Connect VCC pin to 3.3V (SN65HVD230) or 5V (TJA1050) rail

### Issue: CAN_H/CAN_L Not Connected
**Solution:** Wire CAN_H to CAN_H and CAN_L to CAN_L between transceivers

### Issue: No Termination
**Solution:** Add 120Ω resistor across CAN_H and CAN_L at each device

### Issue: TX/RX Swapped
**Solution:** 
- ESP32 TX (GPIO5) must connect to Transceiver TXD
- ESP32 RX (GPIO4 or GPIO18) must connect to Transceiver RXD
- Do NOT cross TX and RX between ESP32 and transceiver

### Issue: Wrong GPIO Pins in Hardware
**Solution:** Either:
- Rewire to match firmware (GPIO5=TX, GPIO4/18=RX), OR
- Change firmware pin definitions to match physical wiring

## Still Not Working?

If all checks pass but communication still fails:

1. **Try loopback mode test:**
   - Edit `ots-fw-cantest/src/main.c` line 119: `can_config.loopback = true;`
   - Rebuild and flash
   - Each device should receive its own transmissions
   - If loopback works: Confirms TWAI and transceiver work, issue is bus wiring
   - If loopback fails: Issue with TWAI or transceiver connection

2. **Swap devices:**
   - Flash controller firmware to audio device and vice versa
   - If problem follows the device: Hardware fault in that device
   - If problem stays with location: Wiring issue

3. **Check transceiver datasheet:**
   - Verify pinout matches your wiring
   - Some modules have SILENT or STANDBY pins that must be driven low/high

## Need More Help?

Document your findings:
```
Transceiver VCC voltage (controller): ___ V
Transceiver VCC voltage (audio): ___ V
CAN_H to CAN_L resistance: ___ Ω
CAN_H idle voltage: ___ V
CAN_L idle voltage: ___ V
GPIO continuity tests: PASS / FAIL
Bus wire continuity: PASS / FAIL
```

This will help identify the exact issue.
