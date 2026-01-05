# I2C Components - Development Prompt

## Overview

This prompt guides the development of **reusable I2C device drivers** as ESP-IDF components. These components provide clean abstractions for I2C devices (LCD controllers, I/O expanders, ADCs, etc.) that can be shared across projects.

## Purpose

Creating I2C components allows:
- **Code reusability**: Use the same driver in multiple projects
- **Clean separation**: Device logic separate from application code
- **Easy testing**: Test components independently
- **Better maintenance**: Updates in one place benefit all projects

## ESP-IDF Component Structure

```
components/
  device_name/
    include/
      device_name.h          # Public API header
    src/
      device_name.c          # Implementation
    CMakeLists.txt           # Component build configuration
    idf_component.yml        # Component metadata (optional)
    README.md                # Component documentation
```

## Example: Existing Components

The firmware has several working examples:

### 1. HD44780 LCD Driver (hd44780_pcf8574)
```
components/hd44780_pcf8574/
  include/lcd_driver.h       # Public API
  src/lcd_driver.c           # PCF8574 + HD44780 implementation
  CMakeLists.txt
```

### 2. ADS1015 ADC Driver (ads1015_driver)
```
components/ads1015_driver/
  include/adc_driver.h       # Public API
  src/adc_driver.c           # ADS1015 I2C ADC implementation
  CMakeLists.txt
```

### 3. MCP23017 I/O Expander (mcp23017_driver)
```
components/mcp23017_driver/
  include/io_expander.h      # Public API
  src/io_expander.c          # MCP23017 I2C expander implementation
  CMakeLists.txt
```

## Component Development Steps

### Step 1: Create Component Directory

```bash
cd ots-fw-main/components
mkdir device_name
cd device_name
mkdir include src
```

### Step 2: Create CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/device_name.c"
    INCLUDE_DIRS "include"
    REQUIRES driver  # I2C driver dependency
)
```

**Key points:**
- `SRCS`: List all `.c` implementation files
- `INCLUDE_DIRS`: Public headers (usually just `include`)
- `REQUIRES`: Dependencies (always need `driver` for I2C)
- `PRIV_REQUIRES`: Private dependencies (not exposed in public API)

### Step 3: Create Public Header (include/device_name.h)

```c
#ifndef DEVICE_NAME_H
#define DEVICE_NAME_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the device
 * 
 * @param i2c_port I2C port number (I2C_NUM_0, etc.)
 * @param i2c_addr I2C device address (7-bit)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t device_name_init(i2c_port_t i2c_port, uint8_t i2c_addr);

/**
 * @brief Read data from device
 * 
 * @param data Pointer to buffer for read data
 * @param len Number of bytes to read
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t device_name_read(uint8_t* data, size_t len);

/**
 * @brief Write data to device
 * 
 * @param data Pointer to data to write
 * @param len Number of bytes to write
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t device_name_write(const uint8_t* data, size_t len);

/**
 * @brief Check if device is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool device_name_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_NAME_H
```

### Step 4: Implement Driver (src/device_name.c)

```c
#include "device_name.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char* TAG = "DEVICE_NAME";

// Module state
static struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    bool initialized;
} state = {
    .initialized = false
};

esp_err_t device_name_init(i2c_port_t i2c_port, uint8_t i2c_addr) {
    if (state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    state.i2c_port = i2c_port;
    state.i2c_addr = i2c_addr;
    
    // Verify device is present on I2C bus
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device not found at 0x%02X", i2c_addr);
        return ret;
    }
    
    // Device-specific initialization here
    
    state.initialized = true;
    ESP_LOGI(TAG, "Device initialized at 0x%02X", i2c_addr);
    return ESP_OK;
}

esp_err_t device_name_read(uint8_t* data, size_t len) {
    if (!state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (state.i2c_addr << 1) | I2C_MASTER_READ, true);
    
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(state.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

esp_err_t device_name_write(const uint8_t* data, size_t len) {
    if (!state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (state.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(state.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

bool device_name_is_initialized(void) {
    return state.initialized;
}
```

## I2C Best Practices

### 1. Bus Initialization

The I2C bus should be initialized **once** by the main application, not by components:

```c
// In main.c or dedicated i2c_handler.c
esp_err_t ots_i2c_handler_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) return ret;
    
    return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}
```

Components should assume the bus is already initialized.

### 2. Error Handling

Always check return values:

```c
esp_err_t ret = device_name_init(I2C_NUM_0, 0x27);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize device: %s", esp_err_to_name(ret));
    return ret;
}
```

### 3. Timeouts

Use reasonable timeouts (typically 1 second):

```c
esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
```

### 4. Address Validation

Devices typically use 7-bit addresses (0x00-0x7F). Verify addresses are correct:

```c
if (i2c_addr > 0x7F) {
    ESP_LOGE(TAG, "Invalid I2C address: 0x%02X", i2c_addr);
    return ESP_ERR_INVALID_ARG;
}
```

### 5. State Management

Track initialization state to prevent misuse:

```c
static bool initialized = false;

esp_err_t device_init(...) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    // ... init code ...
    initialized = true;
    return ESP_OK;
}
```

## Common I2C Patterns

### Register Read

```c
esp_err_t device_read_register(uint8_t reg, uint8_t* value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    
    // Read register value
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}
```

### Register Write

```c
esp_err_t device_write_register(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}
```

### Multi-Byte Read

```c
esp_err_t device_read_bytes(uint8_t reg, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    
    // Read multiple bytes
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}
```

## Component Documentation (README.md)

```markdown
# Device Name Component

ESP-IDF component for interfacing with [Device Name] over I2C.

## Features

- Feature 1
- Feature 2
- Feature 3

## Hardware

- **Device**: [Device Name] ([Model Number])
- **Interface**: I2C (7-bit address: 0xXX)
- **Voltage**: 3.3V / 5V

## Usage

```c
#include "device_name.h"

void app_main(void) {
    // Initialize I2C bus (once)
    ots_i2c_handler_init();
    
    // Initialize device
    esp_err_t ret = device_name_init(I2C_NUM_0, 0x27);
    if (ret != ESP_OK) {
        ESP_LOGE("APP", "Device init failed");
        return;
    }
    
    // Use device
    // ...
}
```

## API Reference

See `include/device_name.h` for full API documentation.

## Dependencies

- ESP-IDF v5.0 or later
- `driver` component (I2C driver)

## License

[License info]
```

## Adding Component to Project

### 1. Update Main CMakeLists.txt (if not auto-detected)

ESP-IDF automatically detects components in `components/` directory. No changes needed unless using external components.

### 2. Include in Application Code

```c
#include "device_name.h"

void app_main(void) {
    device_name_init(I2C_NUM_0, 0x27);
}
```

### 3. Build

```bash
cd ots-fw-main
pio run
```

PlatformIO/ESP-IDF will automatically compile the component.

## Testing Components

### Unit Testing (Standalone)

Create a test environment in `platformio.ini`:

```ini
[env:test-device-name]
extends = env:esp32-s3-dev
build_flags = 
    ${env:esp32-s3-dev.build_flags}
    -DTEST_DEVICE_NAME=1
```

Create test code in `tests/test_device_name.c`:

```c
#include "device_name.h"
#include "esp_log.h"

static const char* TAG = "TEST_DEVICE";

void app_main(void) {
    ESP_LOGI(TAG, "Testing Device Name Component");
    
    // Initialize I2C bus
    ots_i2c_handler_init();
    
    // Test 1: Initialization
    ESP_LOGI(TAG, "Test 1: Initialization");
    esp_err_t ret = device_name_init(I2C_NUM_0, 0x27);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Initialization successful");
    } else {
        ESP_LOGE(TAG, "✗ Initialization failed: %s", esp_err_to_name(ret));
    }
    
    // Test 2: Device detection
    ESP_LOGI(TAG, "Test 2: Device detection");
    if (device_name_is_initialized()) {
        ESP_LOGI(TAG, "✓ Device detected");
    } else {
        ESP_LOGE(TAG, "✗ Device not detected");
    }
    
    // Test 3: Read/Write operations
    // ...
    
    ESP_LOGI(TAG, "Tests complete");
}
```

Run test:
```bash
pio run -e test-device-name -t upload && pio device monitor
```

## Common Issues & Solutions

### Issue: Device Not Found

**Symptoms**: `ESP_ERR_TIMEOUT` or `ESP_FAIL` during init

**Solutions**:
1. Check I2C address with scanner tool
2. Verify pull-up resistors (2.2kΩ - 4.7kΩ)
3. Check wiring (SDA/SCL not swapped)
4. Verify device power supply

### Issue: Bus Lockup

**Symptoms**: All I2C operations fail after first error

**Solutions**:
1. Reset I2C bus: `i2c_driver_delete()` then reinstall
2. Check for clock stretching issues
3. Add delays between operations

### Issue: Intermittent Failures

**Symptoms**: Operations work sometimes, fail randomly

**Solutions**:
1. Increase timeout values
2. Add retry logic
3. Check for electromagnetic interference
4. Verify stable power supply

## Real-World Examples

### MCP23017 I/O Expander Pattern

```c
// Initialize multiple devices on same bus
mcp23017_init(I2C_NUM_0, 0x20);  // Device 1
mcp23017_init(I2C_NUM_0, 0x21);  // Device 2

// Configure pins
mcp23017_set_direction(0x20, MCP23017_PORTA, 0xFF);  // All inputs
mcp23017_set_direction(0x21, MCP23017_PORTA, 0x00);  // All outputs

// Read inputs
uint8_t inputs = mcp23017_read_port(0x20, MCP23017_PORTA);

// Write outputs
mcp23017_write_port(0x21, MCP23017_PORTA, 0x55);
```

### ADS1015 ADC Pattern

```c
// Initialize ADC
ads1015_init(I2C_NUM_0, 0x48);

// Configure channel
ads1015_set_gain(ADS1015_GAIN_4_096V);
ads1015_set_channel(ADS1015_CHANNEL_AIN0);

// Read value
int16_t adc_value = ads1015_read_adc();
float voltage = (adc_value / 2048.0) * 4.096;  // Convert to voltage
```

## Component Checklist

When creating a new I2C component:

- [ ] Create component directory structure
- [ ] Write CMakeLists.txt with correct dependencies
- [ ] Create public header with API documentation
- [ ] Implement initialization function with device detection
- [ ] Implement state tracking (initialized flag)
- [ ] Add error handling for all I2C operations
- [ ] Write README.md with usage examples
- [ ] Create test environment in platformio.ini
- [ ] Write standalone test code
- [ ] Test on real hardware
- [ ] Document I2C address and configuration
- [ ] Add logging for debugging

## References

- ESP-IDF I2C Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- ESP-IDF Component System: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html
- Existing components: `components/hd44780_pcf8574/`, `components/ads1015_driver/`, `components/mcp23017_driver/`
- I2C specification: https://www.nxp.com/docs/en/user-guide/UM10204.pdf
