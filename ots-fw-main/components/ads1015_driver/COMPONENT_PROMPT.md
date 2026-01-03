# ADS1015 Driver Component - ESP-IDF Component

## Hardware Overview

**Device**: ADS1015 12-bit Analog-to-Digital Converter (ADC)
**Manufacturer**: Texas Instruments
**Interface**: I2C
**Purpose**: High-precision analog input reading for ESP32

**Key specifications**:
- Resolution: 12-bit (0-4095 range)
- Channels: 4 single-ended or 2 differential inputs
- Sample Rate: 128 SPS to 3300 SPS
- Voltage Range: ±256mV to ±6.144V (programmable gain)
- I2C Speed: Up to 3.4 MHz (high-speed mode)
- Address: 0x48-0x4B (configurable via ADDR pin)
- Power: 2.0V to 5.5V, 150μA typical

**Pin configuration**:
- AIN0-AIN3: Analog inputs
- ADDR: I2C address select (GND/VDD/SDA/SCL)
- ALERT/RDY: Alert/data ready output (not used in current implementation)
- SCL/SDA: I2C interface

**Datasheet**: https://www.ti.com/lit/ds/symlink/ads1015.pdf

## Component Scope

**This component handles**:
- ✅ ADS1015 register read/write via I2C
- ✅ Single-shot ADC conversions
- ✅ Channel selection (AIN0-AIN3)
- ✅ Programmable gain amplifier (PGA) configuration
- ✅ Sample rate configuration
- ✅ 12-bit result conversion

**This component does NOT handle**:
- ❌ I2C bus initialization (fw-main does this globally)
- ❌ Signal filtering or smoothing
- ❌ Calibration or linearization
- ❌ Multi-channel scanning
- ❌ Continuous conversion mode (uses single-shot only)
- ❌ Comparator/alert functionality

**Architecture separation**:
- **ads1015_driver component**: Low-level ADC access (this component)
- **adc_driver.c wrapper**: Board-level abstraction (in fw-main)
- **troops_module.c**: Application logic using ADC (slider reading)

## Public API

### Data Types

```c
/**
 * @brief ADS1015 ADC channel selection
 */
typedef enum {
    ADS1015_CHANNEL_AIN0 = 0,  ///< Single-ended input on AIN0
    ADS1015_CHANNEL_AIN1 = 1,  ///< Single-ended input on AIN1
    ADS1015_CHANNEL_AIN2 = 2,  ///< Single-ended input on AIN2
    ADS1015_CHANNEL_AIN3 = 3   ///< Single-ended input on AIN3
} ads1015_channel_t;

/**
 * @brief ADS1015 programmable gain amplifier settings
 */
typedef enum {
    ADS1015_GAIN_TWOTHIRDS = 0,  ///< ±6.144V range
    ADS1015_GAIN_ONE       = 1,  ///< ±4.096V range (default)
    ADS1015_GAIN_TWO       = 2,  ///< ±2.048V range
    ADS1015_GAIN_FOUR      = 3,  ///< ±1.024V range
    ADS1015_GAIN_EIGHT     = 4,  ///< ±0.512V range
    ADS1015_GAIN_SIXTEEN   = 5   ///< ±0.256V range
} ads1015_gain_t;

/**
 * @brief ADS1015 sample rate (data rate) settings
 */
typedef enum {
    ADS1015_RATE_128SPS  = 0,  ///< 128 samples per second
    ADS1015_RATE_250SPS  = 1,  ///< 250 SPS
    ADS1015_RATE_490SPS  = 2,  ///< 490 SPS
    ADS1015_RATE_920SPS  = 3,  ///< 920 SPS
    ADS1015_RATE_1600SPS = 4,  ///< 1600 SPS (default)
    ADS1015_RATE_2400SPS = 5,  ///< 2400 SPS
    ADS1015_RATE_3300SPS = 6   ///< 3300 SPS
} ads1015_rate_t;
```

### Initialization

```c
/**
 * @brief Initialize ADS1015 device
 * 
 * Configures the device with default settings:
 * - Single-shot conversion mode
 * - Gain: ±4.096V (ADS1015_GAIN_ONE)
 * - Sample rate: 1600 SPS
 * - Single-ended inputs
 * 
 * @param i2c_addr I2C address (0x48-0x4B)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ads1015_init(uint8_t i2c_addr);
```

**I2C address selection** (via ADDR pin):
```
ADDR Pin | Address
---------+---------
GND      | 0x48  ← Default (OTS uses this)
VDD      | 0x49
SDA      | 0x4A
SCL      | 0x4B
```

### ADC Reading

```c
/**
 * @brief Read single-ended ADC value from channel
 * 
 * Performs single-shot conversion and returns 12-bit result.
 * Blocking function - waits for conversion to complete (~1ms at 1600 SPS).
 * 
 * @param i2c_addr I2C device address
 * @param channel Channel to read (ADS1015_CHANNEL_AIN0-3)
 * @return ADC value (0-4095 for 12-bit), or negative error code
 */
int16_t ads1015_read_adc(uint8_t i2c_addr, ads1015_channel_t channel);
```

**Return value**:
- **Positive (0-4095)**: Valid 12-bit ADC result
- **Negative**: Error code (e.g., -1 for I2C failure)

**Conversion time** (depends on sample rate):
```
Sample Rate | Conversion Time
------------+----------------
128 SPS     | ~7.8 ms
250 SPS     | ~4.0 ms
490 SPS     | ~2.0 ms
920 SPS     | ~1.1 ms
1600 SPS    | ~625 μs (default)
2400 SPS    | ~417 μs
3300 SPS    | ~303 μs
```

### Configuration

```c
/**
 * @brief Set programmable gain amplifier (PGA) setting
 * 
 * Changes the input voltage range. Lower ranges provide better resolution
 * for small signals.
 * 
 * @param i2c_addr I2C device address
 * @param gain Gain setting (ADS1015_GAIN_*)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ads1015_set_gain(uint8_t i2c_addr, ads1015_gain_t gain);

/**
 * @brief Set ADC sample rate (data rate)
 * 
 * Higher rates = faster conversions but potentially more noise.
 * 
 * @param i2c_addr I2C device address
 * @param rate Sample rate (ADS1015_RATE_*)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ads1015_set_sample_rate(uint8_t i2c_addr, ads1015_rate_t rate);
```

**Gain selection guide**:
```
Input Signal | Recommended Gain | Range
-------------+------------------+---------
0-3.3V       | GAIN_ONE         | ±4.096V
0-2.0V       | GAIN_TWO         | ±2.048V
0-1.0V       | GAIN_FOUR        | ±1.024V
0-0.5V       | GAIN_EIGHT       | ±0.512V
```

## Implementation Notes

### Register Map

**Conversion register (0x00)**:
- 16-bit signed result (12-bit ADC in upper 12 bits)
- Right-aligned format: `[D11 D10 ... D0 x x x x]`
- Read to get conversion result

**Config register (0x01)**:
- Bit 15 (OS): Operational status / single-shot conversion start
  - Write 1: Start single conversion
  - Read 1: Conversion complete
- Bits 14-12 (MUX): Input multiplexer config
  - 100: AIN0 to GND (single-ended)
  - 101: AIN1 to GND
  - 110: AIN2 to GND
  - 111: AIN3 to GND
- Bits 11-9 (PGA): Programmable gain amplifier config
- Bit 8 (MODE): Operating mode (0=continuous, 1=single-shot)
- Bits 7-5 (DR): Data rate
- Bit 4 (COMP_MODE): Comparator mode (not used)
- Bits 3-0: Comparator config (not used)

**Default config value**: 0x8583
```
1 000 010 1 100 00011
│  │   │  │  │    │└─ Comparator config (disabled)
│  │   │  │  │    └── Comparator config
│  │   │  │  └─────── Data rate (100 = 1600 SPS)
│  │   │  └────────── Single-shot mode
│  │   └───────────── PGA (010 = ±4.096V)
│  └───────────────── MUX (000 = differential, changed per read)
└──────────────────── Start conversion
```

### Single-Shot Conversion Sequence

```c
int16_t ads1015_read_adc(uint8_t i2c_addr, ads1015_channel_t channel) {
    // 1. Build config word
    uint16_t config = 0x8000 |          // OS: Start conversion
                      (channel << 12) | // MUX: Select channel
                      (gain << 9) |     // PGA: Gain setting
                      (1 << 8) |        // MODE: Single-shot
                      (rate << 5) |     // DR: Sample rate
                      0x03;             // Comparator disabled
    
    // 2. Write config register (starts conversion)
    write_register(i2c_addr, ADS1015_REG_CONFIG, config);
    
    // 3. Wait for conversion (poll or delay)
    vTaskDelay(pdMS_TO_TICKS(2));  // Conservative 2ms delay
    
    // Or poll OS bit:
    // while (!(read_register(i2c_addr, ADS1015_REG_CONFIG) & 0x8000)) {
    //     vTaskDelay(pdMS_TO_TICKS(1));
    // }
    
    // 4. Read conversion result
    int16_t raw = read_register(i2c_addr, ADS1015_REG_CONVERSION);
    
    // 5. Right-shift 4 bits (12-bit result in upper 12 bits of 16-bit register)
    return raw >> 4;
}
```

### I2C Communication

**Read 16-bit register**:
```c
int16_t read_register(uint8_t i2c_addr, uint8_t reg) {
    uint8_t data[2];
    i2c_master_write_read_device(I2C_NUM_0, i2c_addr, 
                                  &reg, 1, 
                                  data, 2, 
                                  pdMS_TO_TICKS(100));
    return (data[0] << 8) | data[1];  // Big-endian
}
```

**Write 16-bit register**:
```c
esp_err_t write_register(uint8_t i2c_addr, uint8_t reg, uint16_t value) {
    uint8_t data[3] = {
        reg,
        (value >> 8) & 0xFF,  // MSB first (big-endian)
        value & 0xFF
    };
    return i2c_master_write_to_device(I2C_NUM_0, i2c_addr, 
                                      data, 3, 
                                      pdMS_TO_TICKS(100));
}
```

**Note**: ADS1015 uses **big-endian** byte order (MSB first), unlike ESP32 which is little-endian.

### Conversion to Voltage

```c
float ads1015_code_to_volts(int16_t code, ads1015_gain_t gain) {
    // Voltage ranges per gain setting (LSB size in mV)
    const float lsb_mv[] = {
        3.0,    // GAIN_TWOTHIRDS: ±6.144V / 4096 = 3.0 mV/bit
        2.0,    // GAIN_ONE:       ±4.096V / 4096 = 2.0 mV/bit
        1.0,    // GAIN_TWO:       ±2.048V / 4096 = 1.0 mV/bit
        0.5,    // GAIN_FOUR:      ±1.024V / 4096 = 0.5 mV/bit
        0.25,   // GAIN_EIGHT:     ±0.512V / 4096 = 0.25 mV/bit
        0.125   // GAIN_SIXTEEN:   ±0.256V / 4096 = 0.125 mV/bit
    };
    
    return (code * lsb_mv[gain]) / 1000.0;  // Convert mV to V
}
```

**Example**:
```c
int16_t code = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
float volts = ads1015_code_to_volts(code, ADS1015_GAIN_ONE);
ESP_LOGI(TAG, "AIN0: %d (%.3fV)", code, volts);
```

### Error Handling

**Common errors**:
- `ads1015_read_adc()` returns **-1**: I2C communication failure
- `ads1015_init()` returns `ESP_FAIL`: Device not responding

**Device detection**:
```c
esp_err_t ret = ads1015_init(0x48);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "ADS1015 found at 0x48");
} else {
    ESP_LOGE(TAG, "ADS1015 not responding - check wiring");
}
```

### Performance Optimization

**Polling frequency**:
```c
// Don't poll faster than ADC conversion time
// At 1600 SPS: conversion takes ~625μs
// Safe polling: every 1-2ms

void adc_poll_task(void *arg) {
    while (1) {
        int16_t value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
        process_adc_value(value);
        vTaskDelay(pdMS_TO_TICKS(2));  // Poll every 2ms
    }
}
```

**Avoid unnecessary conversions**:
```c
// Bad: Read ADC every loop iteration (wasteful)
while (1) {
    int16_t value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
    // ... process ...
    vTaskDelay(pdMS_TO_TICKS(1));  // Too fast!
}

// Good: Read at reasonable rate (e.g., 100ms for slider)
static uint64_t last_read = 0;
uint64_t now = esp_timer_get_time() / 1000;  // ms
if (now - last_read >= 100) {
    int16_t value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
    // ... process ...
    last_read = now;
}
```

## Usage Example

### Basic Initialization and Reading

```c
#include "ads1015_driver.h"

void app_main(void) {
    // Assume I2C bus already initialized
    
    // Initialize ADS1015 at 0x48
    esp_err_t ret = ads1015_init(0x48);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADS1015");
        return;
    }
    
    // Optional: Configure gain for 0-3.3V input
    ads1015_set_gain(0x48, ADS1015_GAIN_ONE);  // ±4.096V range
    
    // Read ADC value from channel 0
    int16_t adc_value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
    if (adc_value >= 0) {
        ESP_LOGI(TAG, "ADC: %d (%.2fV)", 
                 adc_value, 
                 ads1015_code_to_volts(adc_value, ADS1015_GAIN_ONE));
    } else {
        ESP_LOGE(TAG, "ADC read failed");
    }
}
```

### Reading Multiple Channels

```c
void read_all_channels(void) {
    const char *ch_names[] = {"AIN0", "AIN1", "AIN2", "AIN3"};
    
    for (int ch = 0; ch < 4; ch++) {
        int16_t value = ads1015_read_adc(0x48, (ads1015_channel_t)ch);
        if (value >= 0) {
            ESP_LOGI(TAG, "%s: %d", ch_names[ch], value);
        }
        vTaskDelay(pdMS_TO_TICKS(2));  // Allow conversion time
    }
}
```

### Slider/Potentiometer Reading

```c
// Map ADC value to percentage (0-100%)
uint8_t read_slider_percent(void) {
    int16_t adc = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
    
    if (adc < 0) return 0;  // Error
    
    // Map 0-4095 to 0-100%
    uint8_t percent = (adc * 100) / 4095;
    if (percent > 100) percent = 100;
    
    return percent;
}
```

## Integration with fw-main

### Current Usage

**troops_module.c**:
- Reads slider position via ADS1015
- Channel AIN0 connected to potentiometer
- Polls every 100ms
- Maps ADC value (0-4095) to percentage (0-100%)
- Sends `set-troops-percent` command when value changes ≥1%

**Wiring**:
```
Potentiometer       ADS1015        ESP32-S3
-------------       -------        --------
Pin 1 (VCC) -----> 3.3V
Pin 2 (Wiper) ---> AIN0
Pin 3 (GND) -----> GND
                   VDD --------> 3.3V
                   GND --------> GND
                   SCL --------> GPIO 9 (via I2C bus)
                   SDA --------> GPIO 8 (via I2C bus)
                   ADDR -------> GND (address 0x48)
```

### Build Integration

In `/ots-fw-main/src/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c"
         "troops_module.c"
         "adc_driver.c"
         # ...
    INCLUDE_DIRS "../include"
    REQUIRES 
        driver
        ads1015_driver  # This component
        # ...
)
```

### Initialization Sequence

```c
// In troops_module.c init function
static esp_err_t troops_module_init(void) {
    // Initialize ADS1015
    esp_err_t ret = ads1015_init(ADS1015_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADS1015");
        return ret;
    }
    
    // Configure for 0-3.3V potentiometer
    ads1015_set_gain(ADS1015_I2C_ADDR, ADS1015_GAIN_ONE);
    ads1015_set_sample_rate(ADS1015_I2C_ADDR, ADS1015_RATE_1600SPS);
    
    return ESP_OK;
}
```

### Update Loop

```c
// In troops_module.c update function
static void troops_module_update(void) {
    static uint64_t last_read = 0;
    uint64_t now = esp_timer_get_time() / 1000;  // ms
    
    // Poll every 100ms
    if (now - last_read < 100) return;
    last_read = now;
    
    // Read slider
    int16_t adc = ads1015_read_adc(ADS1015_I2C_ADDR, ADS1015_CHANNEL_AIN0);
    if (adc < 0) return;  // Error
    
    // Map to percentage
    uint8_t percent = (adc * 100) / 4095;
    
    // Check for ≥1% change
    if (abs(percent - last_percent) >= 1) {
        send_troops_percent_command(percent);
        last_percent = percent;
    }
}
```

## Testing Strategy

### Hardware Tests

**Test 1: Device detection**
```bash
pio run -e test-i2c -t upload
# Expected: ADS1015 detected at 0x48
```

**Test 2: Fixed voltage test**
```c
// Connect AIN0 to 3.3V via voltage divider
// Expected: ~4095 (full scale)
int16_t value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
ESP_LOGI(TAG, "3.3V input: %d (expected ~4095)", value);
```

**Test 3: Ground test**
```c
// Connect AIN0 to GND
// Expected: ~0
int16_t value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
ESP_LOGI(TAG, "GND input: %d (expected ~0)", value);
```

**Test 4: Slider sweep**
```c
// Move potentiometer from 0% to 100%
// Expected: ADC values from 0 to 4095
while (1) {
    int16_t value = ads1015_read_adc(0x48, ADS1015_CHANNEL_AIN0);
    ESP_LOGI(TAG, "Slider: %d", value);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

### Integration Tests

**Test 1: Troops module integration**
```c
// Verify slider affects troop deployment percentage
// Move slider, check that set-troops-percent commands are sent
```

**Test 2: Noise test**
```c
// Read ADC 100 times with no input change
// Calculate standard deviation
// Expected: Low noise (<5 LSB for stable input)
```

## Dependencies

**ESP-IDF Components**:
- `driver` - I2C master driver (`i2c.h`)
- `esp_log` - Logging macros
- `esp_timer` - High-resolution timer (for polling)

**External Components**: None

**I2C bus initialization** (done by fw-main):
```c
// Already initialized for MCP23017 and LCD
// ADS1015 shares same I2C bus
```

## Future Enhancements

### Short Term
- [ ] Continuous conversion mode support
- [ ] Multi-channel scanning (round-robin)
- [ ] Moving average filter (reduce noise)

### Medium Term
- [ ] Differential input mode (AIN0-AIN1, AIN2-AIN3)
- [ ] Comparator/alert functionality
- [ ] Data ready interrupt (ALERT/RDY pin)

### Long Term
- [ ] Support for ADS1115 (16-bit version)
- [ ] Calibration and offset correction
- [ ] DMA-based continuous sampling

## Performance Characteristics

**Conversion time** (1600 SPS default):
- Single conversion: ~625μs
- Including I2C overhead: ~1ms total

**Noise performance**:
- Typical: ±1-2 LSB (12-bit)
- With filtering: ±0.5 LSB

**Power consumption**:
- Active conversion: 150μA
- Idle: 0.5μA

**Accuracy**:
- INL (Integral Non-Linearity): ±0.125% FSR
- Offset error: ±3mV

## References

- **ADS1015 Datasheet**: https://www.ti.com/lit/ds/symlink/ads1015.pdf
- **ESP-IDF I2C Driver**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- **Application Note**: SBAA228 (Single-Ended vs Differential ADC Inputs)

## Troubleshooting

### Issue: Always reads 0 or 4095

**Symptom**: ADC stuck at min or max value

**Causes**:
- Input voltage out of range
- Wrong PGA gain setting
- Input not connected (floating)

**Solution**:
1. Verify input voltage is within PGA range (0-4.096V for GAIN_ONE)
2. Check wiring to AINx pin
3. Add pull-down resistor if input is floating

### Issue: Noisy readings

**Symptom**: ADC values jump around ±10-20 LSB

**Causes**:
- Long wires acting as antenna
- No input decoupling capacitor
- Power supply noise

**Solution**:
1. Add 0.1μF capacitor across AIN0 to GND (near chip)
2. Use twisted-pair or shielded cable for analog input
3. Add 10nF capacitor on VDD pin
4. Implement software moving average filter

### Issue: Negative values

**Symptom**: `ads1015_read_adc()` returns negative values unexpectedly

**Causes**:
- Incorrect bit shifting (result is signed 16-bit in register)
- Voltage below ground (negative input)

**Solution**:
1. Ensure result is right-shifted 4 bits: `raw >> 4`
2. Verify input voltage is ≥0V (device cannot measure negative voltages in single-ended mode)
3. If using differential mode, negative values are valid

### Issue: I2C communication fails

**Symptom**: `ads1015_init()` returns error

**Causes**:
- Wrong I2C address (check ADDR pin)
- Device not powered
- I2C bus conflict with other devices

**Solution**:
1. Run I2C scan: `pio run -e test-i2c -t upload`
2. Verify ADDR pin grounded (0x48 address)
3. Check VDD has 3.3V
4. Verify SDA/SCL pull-up resistors present
