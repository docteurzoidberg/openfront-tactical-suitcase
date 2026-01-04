# Working Audio Configuration - Quick Reference

**Date Verified**: 2025-01-04  
**Status**: ✅ AUDIO WORKING AT FULL VOLUME

## Critical Configuration Summary

### 1. I2S MCLK - MOST IMPORTANT!

```c
// ❌ WRONG - No audio output
.mclk = I2S_GPIO_UNUSED

// ✅ CORRECT - Audio works
.mclk = I2S_MCLK_IO  // GPIO 0
```

**Why**: ES8388 REQUIRES MCLK to generate internal audio clocks. Without it, I2S data is sent but no sound is produced.

### 2. Complete I2S Configuration

```c
#define I2S_MCLK_IO    GPIO_NUM_0   // CRITICAL - Must be present!
#define I2S_BCK_IO     GPIO_NUM_27  // NOT GPIO 5!
#define I2S_WS_IO      GPIO_NUM_25
#define I2S_DO_IO      GPIO_NUM_26
#define I2S_DI_IO      GPIO_NUM_35

i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = I2S_MCLK_IO,  // GPIO 0 - MANDATORY!
        .bclk = I2S_BCK_IO,
        .ws = I2S_WS_IO,
        .dout = I2S_DO_IO,
        .din = I2S_DI_IO,
    }
};
std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
```

### 3. ES8388 Initialization Sequence

```c
// 1. Mute first
es8388_write_reg(ES8388_DACCONTROL3, 0x04);

// 2. Power management
es8388_write_reg(ES8388_CONTROL2, 0x50);
es8388_write_reg(ES8388_CHIPPOWER, 0x00);

// 3. Disable internal DLL
es8388_write_reg(0x35, 0xA0);
es8388_write_reg(0x37, 0xD0);
es8388_write_reg(0x39, 0xD0);

// 4. Slave mode
es8388_write_reg(ES8388_MASTERMODE, 0x00);

// 5. Power down DAC during config
es8388_write_reg(ES8388_DACPOWER, 0xC0);
es8388_write_reg(ES8388_CONTROL1, 0x12);

// 6. DAC I2S format (16-bit)
es8388_write_reg(ES8388_DACCONTROL1, 0x18);
es8388_write_reg(ES8388_DACCONTROL2, 0x02);

// 7. DAC digital volume (0dB max)
es8388_write_reg(ES8388_DACCONTROL4, 0x00);
es8388_write_reg(ES8388_DACCONTROL5, 0x00);

// 8. Keep muted
es8388_write_reg(ES8388_DACCONTROL3, 0x04);

// 9. Mixer routing
es8388_write_reg(ES8388_DACCONTROL16, 0x00);
es8388_write_reg(ES8388_DACCONTROL17, 0x90);
es8388_write_reg(ES8388_DACCONTROL20, 0x90);

// 10. Output stage volumes (0dB) - SET ONCE!
es8388_write_reg(ES8388_DACCONTROL24, 0x1E);
es8388_write_reg(ES8388_DACCONTROL25, 0x1E);
es8388_write_reg(ES8388_DACCONTROL26, 0x1E);
es8388_write_reg(ES8388_DACCONTROL27, 0x1E);
```

### 4. ES8388 Start Sequence (Unmute)

```c
// Reset state machine
es8388_write_reg(ES8388_CHIPPOWER, 0xF0);
es8388_write_reg(ES8388_CHIPPOWER, 0x00);

// Power up all DAC outputs
es8388_write_reg(ES8388_DACPOWER, 0x3C);
vTaskDelay(pdMS_TO_TICKS(50));

// Soft-ramp unmute
es8388_write_reg(ES8388_DACCONTROL3, 0x20);
es8388_write_reg(ES8388_DACCONTROL3, 0x00);
```

### 5. Volume Control (Runtime)

```c
// Use DACCONTROL4/5 for volume (NOT 24/25!)
uint8_t vol_reg = 0xC0 - (volume_percent * 192 / 100);
es8388_write_reg(ES8388_DACCONTROL4, vol_reg);  // Left
es8388_write_reg(ES8388_DACCONTROL5, vol_reg);  // Right

// NEVER write to DACCONTROL24/25/26/27 after init!
```

### 6. Power Amplifier (GPIO 21)

```c
// Enable once during init and keep ON
gpio_set_level(PA_ENABLE_GPIO, 1);
// Use DAC mute for audio control, not PA toggling
```

## Verification Checklist

Serial monitor should show:
- [ ] `Power amplifier enabled (GPIO21)`
- [ ] `ES8388 I2C device added`
- [ ] `ES8388 codec initialized`
- [ ] `DAC started and unmuted`
- [ ] **`I2S0, MCLK output by GPIO0`** ← CRITICAL!
- [ ] Audio bytes streamed without errors
- [ ] You hear sound at full volume

If you don't see "MCLK output by GPIO0", check your I2S configuration!

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| No audio at all | MCLK not enabled | Set `.mclk = I2S_MCLK_IO` |
| I2S underflow/overflow | Wrong BCK pin | Use GPIO 27, not GPIO 5 |
| Very low volume | Output stage not at 0dB | Set DACCONTROL24/25/26/27 to 0x1E |
| Volume function breaks audio | Writing to DACCONTROL24/25 | Only write to DACCONTROL4/5 |

## Files Modified

Main firmware files with working configuration:
- `src/hardware/i2s.c` - MCLK enabled on GPIO 0
- `src/hardware/es8388.c` - Complete init sequence from test firmware
- `src/hardware/gpio.c` - PA enabled during init
- `src/board_config.h` - Pin definitions

Test firmware (reference):
- `/ots-test-audio/src/es8388.c` - Original working code
- `/ots-test-audio/src/i2s_audio.c` - I2S with MCLK

## Reference Documentation

- Full details: `ESP32_A1S_AUDIOKIT_BOARD.md`
- Test firmware: `/ots-test-audio/`
- ESP-ADF: https://github.com/espressif/esp-adf
