# Hardware Testing Integration Plan

## Overview

Integrate hardware tests directly into main firmware using compile-time flags. Each test mode replaces normal operation with a specific hardware test. Tests run continuously and output results to serial console for manual verification.

**Key Principle**: No test framework, no menus, no automation. Just simple, focused test code that you can visually verify.

## Architecture

### Build Environments (PlatformIO)

Each test gets its own build environment. When you flash a test build, the ESP32 runs ONLY that test in an infinite loop:

```bash
pio run -e test-i2c -t upload      # Flash I2C scan test
pio run -e test-outputs -t upload  # Flash output board test
pio run -e test-inputs -t upload   # Flash input board test
```

**Note**: Each test environment uses `build_src_filter` to replace `main.c` with the test file. This means:
- Only ONE `app_main()` function is compiled
- No runtime test selection needed
- Simple standalone test programs

## PlatformIO Configuration
build_src_filter = 
    +<*>
    -<main.c>
    +<tests/test_rgb_led.c>

# Test 7: Full System Integration
[env:test-integration]
board = esp32-s3-devkitc-1
framework = espidf
build_flags = 
    -DTEST_MODE_INTEGRATION=1
build_src_filter = 
    +<*>
    -<main.c>
    +<tests/test_integration.c>
```

### Build Commands

```bash
# Production firmware
pio run -e esp32-s3-dev -t upload && pio device monitor

# Run specific test
pio run -e test-i2c -t upload && pio device monitor
pio run -e test-outputs -t upload && pio device monitor
pio run -e test-inputs -t upload && pio device monitor
pio run -e test-lcd -t upload && pio device monitor
```

## Implementation Structure

### File Organization

```
ots-fw-main/
├── src/
│   ├── main.c                       # Production firmware
│   └── tests/                       # Test files (replace main)
│       ├── test_i2c_scan.c
│       ├── test_output_board.c
│       ├── test_input_board.c
│       ├── test_lcd.c
│       ├── test_adc.c
│       ├── test_rgb_led.c
│       └── test_integration.c
```

**Key Point**: Each test file has its own `app_main()` function. The `build_src_filter` in platformio.ini ensures only ONE main function is compiled.

## Implemented Tests

### ✅ Test 1: I2C Bus Scan (`test_i2c_scan.c`)
- Scans I2C bus every 5 seconds
- Identifies known devices (0x20, 0x21, 0x27, 0x48)
- Reports total device count
- **Status**: Complete and tested

### ✅ Test 2: Output Board (`test_output_board.c`)
- Walking bit pattern (one LED at a time)
- All on/off test
- Alternating pattern (even/odd pins)
- **Status**: Complete and tested

### ✅ Test 3: Input Board (`test_input_board.c`)
- Continuous input reading (10 Hz)
- Real-time display of all 16 pins
- Active-low detection (pullup enabled)
- **Status**: Complete and tested

### ✅ Test 4: ADC (`test_adc.c`)
- All 4 channels snapshot
- Continuous AIN0 reading
- Voltage and percentage conversion
- 12-bit resolution (0-4095)
- **Status**: Complete and tested

### ✅ Test 5: LCD Display (`test_lcd.c`)
- Backlight verification
- Text display patterns
- Troop format examples
- Alignment tests (left/right/center)
- Cursor positioning
- Scrolling counter
- **Status**: Complete and tested

## Example Test Implementations

### Test 1: I2C Bus Scan (test_i2c_scan.c)

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000

static const char *TAG = "TEST_I2C";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
}

void app_main(void) {
    ESP_LOGI(TAG, "=== I2C Bus Scan Test ===");
    i2c_master_init();
    
    while (1) {
        ESP_LOGI(TAG, "Scanning I2C bus...");
        int found = 0;
        
        for (uint8_t addr = 1; addr < 127; addr++) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            
            esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
            i2c_cmd_link_delete(cmd);
            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  [0x%02X] Found", addr);
                found++;
            }
        }
        
        ESP_LOGI(TAG, "Scan complete: %d devices", found);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

**Expected Output:**
```
I (1234) TEST_I2C: === I2C Bus Scan Test ===
I (1235) TEST_I2C: Scanning I2C bus...
I (1236) TEST_I2C:   [0x20] Found
I (1237) TEST_I2C:   [0x21] Found
I (1238) TEST_I2C:   [0x27] Found
I (1239) TEST_I2C:   [0x48] Found
I (1240) TEST_I2C: Scan complete: 4 devices
```

### Test 2: Output Board Walking Bit (test_output_board.c)

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define MCP23017_ADDR   0x21
#define REG_IODIRA      0x00
#define REG_IODIRB      0x01
#define REG_GPIOA       0x12
#define REG_GPIOB       0x13

static const char *TAG = "TEST_OUTPUTS";

void mcp23017_write(uint8_t reg, uint8_t val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== Output Board Test ===");
    
    // I2C init (reuse from test_i2c_scan.c)
    // ...
    
    // Configure all pins as outputs
    mcp23017_write(REG_IODIRA, 0x00);
    mcp23017_write(REG_IODIRB, 0x00);
    ESP_LOGI(TAG, "Configured as outputs");
    
    while (1) {
        ESP_LOGI(TAG, "Walking bit pattern - Port A");
        for (int i = 0; i < 8; i++) {
            mcp23017_write(REG_GPIOA, (1 << i));
            mcp23017_write(REG_GPIOB, 0x00);
            ESP_LOGI(TAG, "  Pin A%d HIGH", i);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        ESP_LOGI(TAG, "Walking bit pattern - Port B");
        for (int i = 0; i < 8; i++) {
            mcp23017_write(REG_GPIOA, 0x00);
            mcp23017_write(REG_GPIOB, (1 << i));
            ESP_LOGI(TAG, "  Pin B%d HIGH", i);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        ESP_LOGI(TAG, "All ON");
        mcp23017_write(REG_GPIOA, 0xFF);
        mcp23017_write(REG_GPIOB, 0xFF);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "All OFF");
        mcp23017_write(REG_GPIOA, 0x00);
        mcp23017_write(REG_GPIOB, 0x00);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

**Manual Verification:**
1. Connect LEDs to output pins
2. Watch walking bit pattern (one LED at a time)
3. Verify all LEDs turn on together
4. Verify all LEDs turn off together

### Test 3: Input Board Button Test (test_input_board.c)

```c
void app_main(void) {
    ESP_LOGI(TAG, "=== Input Board Test ===");
    // Configure MCP23017 @ 0x20 as inputs with pullups
    // ...
    
    while (1) {
        uint8_t porta = mcp23017_read(REG_GPIOA);
        uint8_t portb = mcp23017_read(REG_GPIOB);
        
        printf("Inputs: A=0x%02X B=0x%02X | ", porta, portb);
        
        // Show individual button states
        for (int i = 0; i < 8; i++) {
            printf("A%d=%d ", i, (porta >> i) & 1);
        }
        for (int i = 0; i < 8; i++) {
            printf("B%d=%d ", i, (portb >> i) & 1);
        }
        printf("\r");
        fflush(stdout);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Manual Verification:**
1. Press each button one at a time
2. See corresponding bit change from 1 (not pressed) to 0 (pressed)
3. Verify all 16 inputs independently

## Usage Workflow

### Building and Running Tests

```bash
cd ots-fw-main

# Build and flash specific test
pio run -e test-i2c -t upload && pio device monitor

# Or separate commands
pio run -e test-outputs -t upload
pio device monitor
```

### What Happens

1. Test firmware flashes to ESP32
2. Device boots and runs test in infinite loop
3. Serial output shows test results continuously
4. Watch serial monitor to verify hardware behavior
5. To run different test: flash different environment

### Typical Serial Output

```
I (1234) TEST_I2C: === I2C Bus Scan Test ===
I (1235) TEST_I2C: Scanning I2C bus...
I (1236) TEST_I2C:   [0x20] Found
I (1237) TEST_I2C:   [0x21] Found
I (1238) TEST_I2C:   [0x27] Found
I (1239) TEST_I2C:   [0x48] Found
I (1240) TEST_I2C: Scan complete: 4 devices
I (6240) TEST_I2C: Scanning I2C bus...
...
```

### Manual Verification Steps

**For each test:**
1. Flash test environment
2. Open serial monitor
3. Watch output AND physical hardware (LEDs, LCD, etc.)
4. Verify expected behavior matches actual behavior
5. Take notes on any failures
6. Flash next test or production firmware when done
## Benefits

### Development Phase
✅ **Isolated Testing**: Test individual modules without full system  
✅ **Quick Iteration**: Flash test, watch output, verify hardware  
✅ **Debugging Aid**: Isolate hardware vs software issues  
✅ **Assembly Verification**: Check each board after soldering  

### Production Phase
✅ **Factory Testing**: Separate test environments for QA  
✅ **Field Diagnostics**: Flash test build to diagnose issues  
✅ **No Extra Tools**: Everything in PlatformIO, no special hardware  

## Code Size Impact

Test builds have minimal overhead since they replace production code:

| Configuration | Flash Usage |
|---------------|-------------|
| Production firmware | 1046 KB |
| Test build (any test) | ~1050-1070 KB |

**Note**: Test builds don't ADD to production firmware - they replace it. You flash either production OR test, not both.

## Implementation Steps

### Phase 1: Create Test Files
1. Create `src/tests/` directory
2. Port `test_i2c_scan.c` from docs/HARDWARE_TEST_PLAN.md
3. Port `test_output_board.c`
4. Port `test_input_board.c`
5. Add PlatformIO test environments

### Phase 2: Test and Verify
6. Flash each test environment
7. Verify serial output
8. Verify physical hardware behavior
9. Document any issues found
10. Fix hardware/firmware bugs

### Phase 3: Advanced Tests
11. Implement LCD test (scrolling text, patterns)
12. Implement ADC test (continuous reading)
13. Implement RGB LED test (color cycle)
14. Implement integration test (all modules together)

## Next Steps

**Immediate:**
1. Create `src/tests/` directory
2. Create first test file: `test_i2c_scan.c`
3. Add `[env:test-i2c]` to platformio.ini
4. Build and test: `pio run -e test-i2c -t upload && pio device monitor`

**Once working:**
5. Port remaining tests from docs/HARDWARE_TEST_PLAN.md
6. Create simple docs/TESTING.md with usage instructions
7. Test on real hardware during assembly

---

**Key Takeaway**: Simple is better. No frameworks, no menus, just standalone test programs that run continuously and output to serial for manual verification.
