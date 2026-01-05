# ESP32-A1S AudioKit v2.2 Board Reference

## Board Overview

**Model**: Ai-Thinker ESP32-A1S AudioKit v2.2  
**Codec**: ES8388 (newer version, on sale)  
**Amplifier**: NS4150 (8W Class D)  
**MCU**: ESP32-WROOM-32E  
**Purchase**: https://s.click.aliexpress.com/e/_DerSMXn  

**Board Variants**:
- **ES8388 version** (newer, current, on sale) ✅ This board
- **AC101 version** (older, halted production)

**Note**: User reported "some variant with different pinouts" - be aware of undocumented hardware variations.

## Pin Configuration

### Expansion Header (2×7 Pin Header)

The board includes a 2×7 (14-pin) expansion header for accessing additional GPIOs:

```
Top Row (left to right):
GND  |  IO0  |  RST  |  TX0  |  RX0  |  3V3  |  3V3

Bottom Row (left to right):  
IO21 |  IO22 |  IO19 |  IO23 |  IO18 |  IO5  |  GND
```

**Pin Functions**:
- **GND** - Ground (2 pins: top-left, bottom-right)
- **3V3** - 3.3V power supply (2 pins: top row, positions 6-7)
- **IO0** - Boot mode selection (pulled up, used during programming)
- **RST** - Reset pin
- **TX0/RX0** - UART0 for programming and serial debugging
- **IO5, IO18, IO19, IO21, IO22, IO23** - General purpose I/O

**GPIO Usage Notes**:
- **IO21**: Used for power amplifier enable (PA_ENABLE_GPIO) - avoid using
- **IO22**: Used for green LED (LED_D4) - can be repurposed if LED not needed
- **IO19**: Used for red LED (LED_D5) and button KEY3 - can be repurposed
- **IO18, IO23**: Generally available for custom use
- **IO5**: Shared with headphone detect and KEY6 - avoid if using audio jack

**Recommended for CAN Bus**: GPIO18 (TX) and GPIO19 (RX) - minimal conflicts with audio functionality.

### I2C Bus
```c
#define I2C_MASTER_SCL_IO    GPIO_NUM_32
#define I2C_MASTER_SDA_IO    GPIO_NUM_33
#define I2C_MASTER_FREQ_HZ   100000
#define ES8388_ADDR          0x10  // 7-bit address (0x20 for R/W)
```

**I2C Devices**:
- ES8388 codec: 0x10 (7-bit) / 0x20 (8-bit with R/W)
- Shared bus with codec only

### I2S Audio Interface

```c
#define I2S_NUM              I2S_NUM_0

// Critical: These pins were incorrect in initial schematics
#define I2S_MCLK_GPIO        GPIO_NUM_0   // Master clock (CRITICAL - must be present)
#define I2S_BCK_GPIO         GPIO_NUM_27  // Bit clock (was GPIO_5 in some docs - WRONG!)
#define I2S_WS_GPIO          GPIO_NUM_25  // Word select (LRCK)
#define I2S_DOUT_GPIO        GPIO_NUM_26  // Data out (to codec)
#define I2S_DIN_GPIO         GPIO_NUM_35  // Data in (from codec)
```

**CRITICAL PIN CORRECTIONS**:
- **BCK was documented as GPIO 5** → Actually GPIO 27 (GPIO 5 is headphone detect)
- **MCLK MUST be enabled on GPIO 0** → ES8388 REQUIRES MCLK to generate audio clocks
  - Setting MCLK to `I2S_GPIO_UNUSED` causes silent playback (I2S data sent but no audio)
  - Without MCLK, codec cannot generate internal audio clocks
  - This is the most common cause of "no audio" despite correct initialization
- Incorrect BCK pin causes I2S underflow errors

### Power Amplifier Control

```c
#define PA_ENABLE_GPIO       GPIO_NUM_21  // NS4150 amplifier enable (active HIGH)
```

**PA Control Strategy** (from ESP-ADF):
- Enable PA **once during init** and keep it ON
- Use DAC MUTE (DACCONTROL3) for audio on/off control
- Do NOT toggle PA rapidly - causes clicks

### Other GPIOs

```c
#define HEADPHONE_DETECT_GPIO  GPIO_NUM_5   // Headphone insertion detection
#define KEY1_GPIO             GPIO_NUM_36  // Button 1
#define KEY2_GPIO             GPIO_NUM_13  // Button 2
#define KEY3_GPIO             GPIO_NUM_19  // Button 3
#define KEY4_GPIO             GPIO_NUM_23  // Button 4
#define KEY5_GPIO             GPIO_NUM_18  // Button 5
#define KEY6_GPIO             GPIO_NUM_5   // Button 6 (shared with headphone detect)
```

**LED Indicators**:
```c
#define LED_D4_GPIO          GPIO_NUM_22  // Green LED
#define LED_D5_GPIO          GPIO_NUM_19  // Red LED
```

### CAN Bus Configuration (Audio Module Variant)

**Current Configuration** (default):
```c
#define CAN_TX_GPIO          GPIO_NUM_18  // CAN transmit (expansion header)
#define CAN_RX_GPIO          GPIO_NUM_19  // CAN receive (expansion header)
```

**Pin Selection Rationale**: GPIO18/19 were chosen to minimize conflicts with audio functionality:
- ✅ Avoids Audio I2S pins (GPIO 0, 25, 26, 27, 35)
- ✅ Avoids Audio I2C pins (GPIO 32, 33)
- ✅ Avoids Power amplifier control (GPIO 21)
- ✅ Avoids Headphone detection (GPIO 5)
- ⚠️ Minor conflict: GPIO19 shared with LED_D5 (red LED)

**Alternative Pin Configurations**:

If you need the red LED (GPIO19) or prefer different pins, use one of these alternatives:

| Config | CAN TX | CAN RX | Trade-offs |
|--------|--------|--------|------------|
| **Option 1** (default) | GPIO18 | GPIO19 | Red LED (D5) disabled, KEY3 button disabled |
| **Option 2** | GPIO18 | GPIO23 | Both LEDs work, KEY4 button disabled |
| **Option 3** | GPIO22 | GPIO23 | Red LED works, green LED (D4) disabled, KEY3/KEY4 disabled |

**To change configuration**: Edit `CAN_TX_GPIO` and `CAN_RX_GPIO` in `src/board_config.h`.

## ES8388 Codec Configuration

### Critical Register Settings

**Volume Architecture** (discovered through debugging):

1. **DAC Digital Volume** (DACCONTROL4/5):
   - Controls digital volume before DAC conversion
   - Range: 0x00 (0dB max) to 0xC0 (mute)
   - Formula: `reg = 0xC0 - (volume_percent * 192 / 100)`
   - **Use this for runtime volume control**

2. **Output Stage Volume** (DACCONTROL24/25/26/27):
   - Controls analog output amplifier gain
   - Range: 0x00 (-30dB) to 0x1E (0dB) to 0x21 (+3dB)
   - **Set once during init to 0x1E (0dB), never change**
   - Channels: 24=LOUT1, 25=ROUT1, 26=LOUT2, 27=ROUT2

3. **Mixer Gain** (DACCONTROL17/20):
   - Set to 0x90 (0dB) for proper routing
   - Do not modify unless experimenting

**CRITICAL BUG TO AVOID**:
```c
// ❌ WRONG - This overwrites output stage volume and causes low audio:
es8388_set_volume(int vol) {
    uint8_t reg = (100 - vol) * 33 / 100;
    es8388_write_reg(ES8388_DACCONTROL24, reg);  // Overwrites init!
    es8388_write_reg(ES8388_DACCONTROL25, reg);
}

// ✅ CORRECT - Write to DAC digital volume instead:
es8388_set_volume(int vol) {
    uint8_t reg = 0xC0 - (vol * 192 / 100);
    es8388_write_reg(ES8388_DACCONTROL4, reg);
    es8388_write_reg(ES8388_DACCONTROL5, reg);
}
```

### Working Initialization Sequence

```c
// 1. Power down codec
es8388_write_reg(ES8388_CHIPPOWER, 0xFF);

// 2. Reset state machine
es8388_write_reg(ES8388_CONTROL1, 0x80);
es8388_write_reg(ES8388_CHIPPOWER, 0x00);

// 3. Set to slave mode (ESP32 is I2S master)
es8388_write_reg(ES8388_MASTERMODE, 0x00);

// 4. Power down DAC during configuration
es8388_write_reg(ES8388_DACPOWER, 0xC0);

// 5. Configure DAC I2S interface - 16-bit I2S format
es8388_write_reg(ES8388_DACCONTROL1, 0x18);
es8388_write_reg(ES8388_DACCONTROL2, 0x02);

// 6. Set DAC digital volume to max (0dB)
es8388_write_reg(ES8388_DACCONTROL4, 0x00);
es8388_write_reg(ES8388_DACCONTROL5, 0x00);

// 7. Keep muted during init
es8388_write_reg(ES8388_DACCONTROL3, 0x04);

// 8. Configure mixer routing
es8388_write_reg(ES8388_DACCONTROL16, 0x00);
es8388_write_reg(ES8388_DACCONTROL17, 0x90);  // Left DAC to left mixer, 0dB
es8388_write_reg(ES8388_DACCONTROL20, 0x90);  // Right DAC to right mixer, 0dB

// 9. Set output stage volumes to 0dB (CRITICAL!)
es8388_write_reg(ES8388_DACCONTROL24, 0x1E);  // LOUT1: 0dB
es8388_write_reg(ES8388_DACCONTROL25, 0x1E);  // ROUT1: 0dB
es8388_write_reg(ES8388_DACCONTROL26, 0x1E);  // LOUT2: 0dB
es8388_write_reg(ES8388_DACCONTROL27, 0x1E);  // ROUT2: 0dB

// 10. Enable power amplifier (GPIO 21 HIGH)
gpio_set_level(PA_ENABLE_GPIO, 1);
```

### Working Start Sequence

```c
// Reset state machine
es8388_write_reg(ES8388_CHIPPOWER, 0xF0);
es8388_write_reg(ES8388_CHIPPOWER, 0x00);

// Power up DAC and all outputs (0x3C = LOUT1/ROUT1/LOUT2/ROUT2)
es8388_write_reg(ES8388_DACPOWER, 0x3C);
vTaskDelay(pdMS_TO_TICKS(50));  // Let outputs stabilize

// Unmute with soft-ramp to reduce click
es8388_write_reg(ES8388_DACCONTROL3, 0x20);  // Enable soft-ramp
es8388_write_reg(ES8388_DACCONTROL3, 0x00);  // Unmute
```

### Working Stop Sequence

```c
// Mute DAC
es8388_write_reg(ES8388_DACCONTROL3, 0x04);

// Power down DAC
es8388_write_reg(ES8388_DACPOWER, 0xC0);

// Note: PA stays enabled (ESP-ADF approach)
```

### Output Routing

**DACPOWER Register Values** (from ESP-ADF esxxx_common.h):
```c
DAC_OUTPUT_LOUT1 = 0x04  // Headphone left
DAC_OUTPUT_LOUT2 = 0x08  // Speaker left
DAC_OUTPUT_ROUT1 = 0x10  // Headphone right
DAC_OUTPUT_ROUT2 = 0x20  // Speaker right
DAC_OUTPUT_ALL   = 0x3C  // All outputs (0x04 | 0x08 | 0x10 | 0x20)
```

**Ai-Thinker Board Default**: `AUDIO_HAL_DAC_OUTPUT_ALL` (0x3C)

**Physical Connections** (likely):
- LOUT1/ROUT1 → 3.5mm headphone jack
- LOUT2/ROUT2 → NS4150 amplifier → speakers

## I2S Configuration

```c
i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // MCLK = 256 × sample_rate
};

i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_MCLK_GPIO,    // GPIO 0 (CRITICAL!)
    .bck_io_num = I2S_BCK_GPIO,     // GPIO 27 (not GPIO 5!)
    .ws_io_num = I2S_WS_GPIO,       // GPIO 25
    .data_out_num = I2S_DOUT_GPIO,  // GPIO 26
    .data_in_num = I2S_DIN_GPIO,    // GPIO 35
};
```

**Key Settings**:
- **mclk_multiple = 256** (not 128) for ES8388 compatibility
- **use_apll = false** - Standard PLL works fine
- **DMA buffers**: 8 × 1024 bytes = smooth playback without underruns

## Known Issues & Solutions (Verified 2025-01-04)

### Issue 1: No Audio Output (MOST COMMON - Fixed!)
**Symptoms**: 
- Codec initializes successfully (I2C communication OK)
- I2S data streaming works (bytes written to I2S)
- Volume settings correct (digital volume at 0x00, output stage at 0x1E)
- PA enabled (GPIO 21 HIGH)
- Serial logs show no errors
- **But absolutely NO sound output**

**Root Cause**: MCLK (Master Clock) not enabled in I2S configuration

**Why MCLK is Critical**:
The ES8388 codec REQUIRES MCLK on GPIO 0 to generate internal audio clocks. Without MCLK:
- I2S data is transmitted and received by codec
- Codec cannot generate audio processing clocks internally
- No audio signal reaches the DAC/amplifier
- Results in completely silent playback

**Solution**:
```c
// ❌ WRONG - Causes silent playback (no audio at all)
i2s_std_config_t std_cfg = {
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,  // DO NOT DO THIS!
        .bclk = I2S_BCK_IO,
        .ws = I2S_WS_IO,
        .dout = I2S_DO_IO,
        .din = I2S_DI_IO,
    }
};

// ✅ CORRECT - Audio works perfectly
i2s_std_config_t std_cfg = {
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

**Verification**:
```c
ESP_LOGI(TAG, "I2S MCLK GPIO: %d", I2S_MCLK_IO);
// Should print: "I2S MCLK GPIO: 0"
// If you see -1 or "unused", audio will NOT work!
```

**Also verify BCK pin**:
- Some documentation incorrectly shows BCK on GPIO 5
- Correct BCK pin is GPIO 27
- Wrong BCK causes I2S underflow/overflow errors

### Issue 2: Very Low Volume
**Symptoms**: Audio plays but extremely quiet  
**Root Cause**: Volume function overwrites DACCONTROL24/25  
**Solution**:
- Set DACCONTROL24/25/26/27 to 0x1E during init
- **Never write to them again**
- Use DACCONTROL4/5 for runtime volume control
- Formula: `reg = 0xC0 - (volume_percent * 192 / 100)`

### Issue 3: Loud Click at Playback Start
**Symptoms**: Pop/click when starting playback  
**Solutions Attempted**:
- ✅ Enable PA once during init (don't toggle)
- ✅ Use soft-ramp feature (DACCONTROL3 bit 5)
- ✅ Add 50ms delay after powering DAC
- ⚠️ Some residual click may remain (hardware limitation)

### Issue 4: I2S Underflow/Overflow
**Symptoms**: Serial logs show `I2S (I2S_EVENT_TX_Q_OVF)` or underflow  
**Root Cause**: Wrong BCK pin (GPIO 5 instead of 27)  
**Solution**: Use GPIO 27 for BCK

## Audio Format Support

**Tested & Working**:
- 44.1kHz, 16-bit, stereo WAV files
- Embedded audio data (no SD card needed)

**Example Embedded Audio**:
```c
// In header file:
extern const uint8_t quack_44100_wav_start[] asm("_binary_quack_44100_wav_start");
extern const uint8_t quack_44100_wav_end[] asm("_binary_quack_44100_wav_end");

// CMakeLists.txt:
target_add_binary_data(${COMPONENT_TARGET} "quack-44100.wav" BINARY)

// In code:
const uint8_t* audio_data = quack_44100_wav_start + 44;  // Skip WAV header
size_t audio_size = (quack_44100_wav_end - quack_44100_wav_start) - 44;
i2s_write(I2S_NUM_0, audio_data, audio_size, &bytes_written, portMAX_DELAY);
```

## Power Amplifier (NS4150)

**Specs**:
- Class D amplifier, 8W output
- Enable via GPIO 21 (active HIGH)
- Disable via GPIO 21 (active LOW)

**Best Practices** (from ESP-ADF):
- Enable during codec initialization
- Keep enabled during operation
- Use codec MUTE for audio control (not PA enable/disable)
- Rapid toggling causes clicks

## ESP-ADF Reference

**Repository**: https://github.com/espressif/esp-adf  
**Board Definition**: `/components/audio_board/ai_thinker_audio_kit_v2_2/`  
**ES8388 Driver**: `/components/audio_hal/driver/es8388/es8388.c`

**Key ESP-ADF Files Referenced**:
- `board_def.h` - Pin definitions and default config
- `es8388.c` - Codec driver (lines 297-298 for volume comment)
- `esxxx_common.h` - DAC output bit definitions

## Testing Notes (Verified Working Setup - 2025-01-04)

**Test Firmware**: `/home/drzoid/dev/openfront/ots/ots-test-audio/`  
**Production Firmware**: `/home/drzoid/dev/openfront/ots/ots-fw-audiomodule/`  
**Build System**: PlatformIO + ESP-IDF 5.4.2

**Serial Output Validation** (working configuration):
```
I (xxx) gpio: Power amplifier enabled (GPIO21) - keeping ON per ESP-ADF
I (xxx) i2c: I2C master initialized successfully
I (xxx) ES8388: Initializing ES8388 codec @ 44100 Hz
I (xxx) ES8388: ES8388 I2C device added
I (xxx) ES8388: ES8388 codec initialized (muted, call es8388_start to unmute)
I (xxx) ES8388: Starting DAC output
I (xxx) ES8388: DAC started and unmuted (with soft-ramp)
I (xxx) i2s: DMA Malloc info, datalen=blocksize=1024, dma_buf_count=8
I (xxx) i2s: I2S0, MCLK output by GPIO0  ← CRITICAL! Must see this!
I (xxx) AUDIO_PLAYER: Playing embedded WAV (83034 bytes)
I (xxx) DECODER: Memory source 0 complete: 82956 bytes streamed
```

**Critical Success Indicators**:
- ✅ `MCLK output by GPIO0` appears in logs (if missing, no audio!)
- ✅ Audio plays at full volume
- ✅ No I2S underflow/overflow errors
- ✅ Volume control works (0-100% range)
- ✅ Minimal startup click (soft-ramp reduces it)
- ✅ Clean playback and shutdown

**Common Failure Indicators**:
- ❌ No "MCLK output by GPIO0" log → Check I2S config, MCLK might be disabled
- ❌ I2S underflow/overflow → Wrong BCK pin (should be GPIO 27, not 5)
- ❌ Silent playback with successful I2S writes → MCLK not enabled!
- ❌ Very low volume → Output stage (DACCONTROL24/25) not set to 0x1E

## Debugging Tips

1. **Logic Analyzer**: Check MCLK, BCK, WS, DOUT signals
   - MCLK should be ~11.2896 MHz (256 × 44100 Hz)
   - BCK should be ~1.4112 MHz (32 × 44100 Hz)
   - WS should be 44.1 kHz

2. **I2C Scanner**: Verify ES8388 at address 0x10
   ```c
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (0x10 << 1) | I2C_MASTER_WRITE, true);
   i2c_master_stop(cmd);
   esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
   ```

3. **Register Read-Back**: Verify writes took effect
   ```c
   uint8_t value;
   es8388_read_reg(ES8388_DACCONTROL24, &value);
   ESP_LOGI(TAG, "DACCONTROL24 = 0x%02x (should be 0x1E)", value);
   ```

4. **Volume Debugging**:
   - Check DACCONTROL4/5 change with volume
   - Verify DACCONTROL24/25 stay at 0x1E
   - If volume has no effect: check which registers are being written

## Version History

**2025-01-04**: 
- Fixed BCK pin (GPIO 5 → GPIO 27)
- Added MCLK (GPIO 0)
- Fixed volume control (DACCONTROL4/5 vs 24/25)
- Implemented soft-ramp unmute
- Audio now working at full volume ✅

**Initial Version**:
- Audio played but very low volume
- Startup clicks
- Wrong pin configuration from schematics

## Related Documentation

- ESP32-A1S AudioKit Schematic: (if available)
- ES8388 Datasheet: (search online)
- ESP-ADF Documentation: https://docs.espressif.com/projects/esp-adf/

---

**Last Updated**: January 4, 2025  
**Status**: ✅ WORKING (full volume, minimal click)  
**Validated By**: Extensive debugging session with ESP-ADF source code analysis
