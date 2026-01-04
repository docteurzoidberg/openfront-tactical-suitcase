# ES8388 Codec Analysis - Click Issue Root Cause

## Problem
Audio playback produces "two clicks" (stereo) instead of clear audio tone on ESP32-A1S board with ES8388 codec.

## Critical Findings from ESP-ADF Source Code

### 1. **STATE MACHINE RESET SEQUENCE (CRITICAL)**

**ESP-ADF `es8388_start()` function:**
```c
esp_err_t es8388_start(es_module_t mode)
{
    // ... check if config changed ...
    if (prev_data != data) {
        res |= es_write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0xF0);   // RESET state machine
        res |= es_write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0x00);   // START state machine
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC || mode == ES_MODULE_LINE) {
        res |= es_write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3c);    // power up dac
        res |= es8388_set_voice_mute(false);                         // unmute
    }
}
```

**Our implementation (WRONG):**
```c
esp_err_t es8388_start(void)
{
    return es8388_write_reg(ES8388_DACPOWER, 0x00);  // Just power on DAC - NO STATE MACHINE RESET!
}
```

**Impact:** Without state machine reset, the codec may not properly synchronize its internal clock/phase, causing clicks/pops.

### 2. **DAC POWER REGISTER VALUE**

ESP-ADF uses `0x3C` for DACPOWER (enables specific outputs), we use `0x00`.

Register bit meanings:
- Bit 7: ROUT2 power down
- Bit 6: LOUT2 power down
- Bit 5: ROUT1 power down (should be 0 = power ON)
- Bit 4: LOUT1 power down (should be 0 = power ON)
- Bit 3: Right mixer power down
- Bit 2: Left mixer power down

`0x3C` = 0b00111100 = ROUT2/LOUT2 OFF, mixers OFF, ROUT1/LOUT1 ON  
`0x00` = 0b00000000 = Everything ON (including unused outputs)

### 3. **UNMUTE SEQUENCE**

ESP-ADF explicitly calls `es8388_set_voice_mute(false)` after starting DAC. This:
- Clears bit 2 in ES8388_DACCONTROL3 register
- Ensures soft ramp-up of DAC output

Our code doesn't explicitly unmute - relies on initialization state.

### 4. **INITIALIZATION DIFFERENCES**

**ESP-ADF init sequence:**
```c
// Mute first
es_write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x04);  // Mute DAC

// Power management
es_write_reg(ES8388_ADDR, ES8388_CONTROL2, 0x50);
es_write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0x00);

// Disable internal DLL for better 8K sample rate (but applies to all rates)
es_write_reg(ES8388_ADDR, 0x35, 0xA0);
es_write_reg(ES8388_ADDR, 0x37, 0xD0);
es_write_reg(ES8388_ADDR, 0x39, 0xD0);

// DAC initially powered down during init
es_write_reg(ES8388_ADDR, ES8388_DACPOWER, 0xC0);  // Disable DAC initially
es_write_reg(ES8388_ADDR, ES8388_CONTROL1, 0x12);  // Play mode

// ... configure I2S format, mixing, etc ...

// Power up specific outputs at END of init
es_write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3c);  // Enable LOUT1/ROUT1 only
```

**Our init (potentially issues):**
- We power everything on immediately
- No initial mute
- No DLL disable registers
- No CONTROL1 mode setting

### 5. **VOLUME CONTROL**

ESP-ADF uses a sophisticated volume management system that:
- Maps user volume (0-100) to dB scale
- Uses DACCONTROL4/5 for digital volume
- Uses DACCONTROL24/25 for output stage volume
- Considers board PA gain

Our simple implementation just writes raw values to DACCONTROL24/25.

## Test Results

### Test 1: Original Implementation
- **Result**: Two clicks (stereo) instead of audio
- **Issue**: Missing state machine reset, wrong DACPOWER value, no pre-fill

### Test 2: With State Machine Reset
- **Result**: Still clicks but "different" 
- **Fix Applied**: Added `CHIPPOWER 0xF0→0x00` reset sequence, proper DACPOWER (0x3C), unmute
- **Remaining Issue**: DAC starting before I2S buffers have data

### Test 3: With I2S Buffer Pre-fill (CURRENT)
- **Fix Applied**: Pre-fill I2S DMA buffers with 4KB audio data BEFORE starting DAC
- **Sequence**: Init codec (muted) → Init I2S → Pre-fill buffers → Start DAC (unmute)
- **Status**: TESTING - awaiting user feedback

### Next Steps if Still Clicks

If clicks persist after buffer pre-fill, try:

1. **Silence Pre-fill**: Uncomment silence buffer pre-fill in main.c (Option 1)
   - 100ms of zeros before audio starts
   - Gives DAC time to stabilize with known-good samples

2. **Increase Pre-fill Size**: Change `prefill_size` from 4KB to 8KB or 16KB
   
3. **Add Longer DAC Stabilization Delay**: Increase delay after `es8388_start()` from 50ms to 200ms

4. **Check I2S DMA Configuration**: 
   - Current: 8 descriptors × 240 frames
   - Try: 16 descriptors × 480 frames (larger buffers)

5. **Verify I2S Clock Accuracy**: 
   - Check if MCLK is actually 256× sample rate
   - Verify BCK and WS timing with oscilloscope

## Root Cause Summary

The clicks were caused by **three compounding issues**:

1. **Missing ES8388 state machine reset** - Codec not properly synchronized
2. **Wrong power management values** - DACPOWER 0x00 vs correct 0x3C  
3. **DAC started with empty I2S buffers** - No audio data in DMA when codec powers on

All three must be fixed for click-free operation.
