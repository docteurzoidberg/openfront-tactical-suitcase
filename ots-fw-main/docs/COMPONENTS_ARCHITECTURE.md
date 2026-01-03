# OTS Firmware Components Architecture

## Overview

The OTS firmware follows a **hardware abstraction layer (HAL) pattern** where all hardware-specific logic is isolated into independent **ESP-IDF components**. This architecture provides:

- **Modularity**: Each hardware driver is self-contained
- **Reusability**: Components can be used across multiple projects
- **Testability**: Hardware can be mocked/tested independently
- **Maintainability**: Clear separation between hardware and application logic
- **Portability**: Easy to swap hardware implementations

## Component Types

### 1. Shared Components (`/ots-fw-shared/components/`)

**Location**: Repository `/ots-fw-shared/components/`

**Purpose**: Generic, reusable components that can be used by **multiple firmware projects** (fw-main, fw-audiomodule, and future firmware projects)

**Characteristics**:
- No project-specific dependencies
- Pure hardware/protocol abstraction
- Can be extracted to separate libraries
- Versioned independently
- Shared across all OTS firmware projects

**Current components**:
- `can_driver` - CAN bus (TWAI) driver with mock fallback

**Usage in projects**: Add to project CMakeLists.txt:
```cmake
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ots-fw-shared/components")
```

### 2. Project-Level Components (`/ots-fw-main/components/`)

**Location**: Inside firmware project directory

**Purpose**: Hardware drivers specific to **ots-fw-main** project only

**Characteristics**:
- May have project-specific configurations
- Tied to specific hardware modules
- Part of fw-main build system
- Not intended for external use

**Current components**:
- `ads1015_driver` - 12-bit I2C ADC (for slider/analog inputs)
- `esp_http_server_core` - HTTP server utilities
- `hd44780_pcf8574` - I2C LCD display driver (16x2 character LCD)
- `mcp23017_driver` - I2C I/O expander (16-pin GPIO over I2C)
- `ws2812_rmt` - RGB LED strip driver (WS2812B/NeoPixel)

## Component Structure

### Standard ESP-IDF Component Layout

Each hardware driver component follows this structure:

```
component_name/
├── CMakeLists.txt              # ESP-IDF build configuration
├── idf_component.yml           # Component dependencies (optional)
├── README.md                   # Component documentation
├── COMPONENT_PROMPT.md         # AI assistant guidance (THIS FILE)
├── include/
│   └── component_name.h        # Public API header
├── src/                        # Implementation files (optional)
│   └── component_name.c
└── component_name.c            # Implementation (if single file)
```

### Required Files

#### 1. CMakeLists.txt

Minimal ESP-IDF component build file:

```cmake
idf_component_register(
    SRCS "component_name.c"
    INCLUDE_DIRS "include"
    REQUIRES driver  # Add ESP-IDF component dependencies here
)
```

**Key fields**:
- `SRCS`: List of `.c` files to compile
- `INCLUDE_DIRS`: Directories with public headers (usually `"include"`)
- `REQUIRES`: Other ESP-IDF components this component needs (e.g., `driver`, `esp_timer`)
- `PRIV_REQUIRES`: Private dependencies not exposed in public API

#### 2. idf_component.yml (Optional but Recommended)

Declares external dependencies:

```yaml
version: "1.0.0"
description: "Hardware driver for XYZ"
url: https://github.com/your-org/ots
dependencies:
  idf:
    version: ">=5.0.0"
  # External components from component registry
  # example_dependency: "^1.0.0"
```

#### 3. include/component_name.h

Public API header with:
- Hardware initialization functions
- Read/write functions
- Configuration structures
- Error codes
- Doxygen-style documentation

Example:

```c
/**
 * @file component_name.h
 * @brief Hardware driver for XYZ device
 */

#ifndef COMPONENT_NAME_H
#define COMPONENT_NAME_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for XYZ device
 */
typedef struct {
    uint8_t i2c_addr;        ///< I2C address
    uint32_t freq_hz;        ///< I2C frequency
    // ... other config
} xyz_config_t;

/**
 * @brief Initialize XYZ device
 * 
 * @param config Configuration structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t xyz_init(const xyz_config_t *config);

/**
 * @brief Read data from XYZ device
 * 
 * @param[out] data Buffer to store read data
 * @param len Number of bytes to read
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t xyz_read(uint8_t *data, size_t len);

// ... other functions

#ifdef __cplusplus
}
#endif

#endif // COMPONENT_NAME_H
```

#### 4. COMPONENT_PROMPT.md

AI assistant guidance document describing:
- Hardware device overview
- Component purpose and scope
- Public API documentation
- Implementation notes
- Usage examples
- Integration with fw-main
- Testing strategy
- Future enhancements

## Component Integration in fw-main

### 1. Add Component Dependency

In `/ots-fw-main/src/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
         "module_foo.c"
         # ... other sources
    INCLUDE_DIRS "../include"
    REQUIRES 
        driver
        esp_timer
        component_name  # Add your component here
        mcp23017_driver
        ads1015_driver
        # ... other components
)
```

### 2. Include Component Header

In fw-main source files:

```c
#include "component_name.h"  // Component provides this header
```

### 3. Initialize Hardware

Typically in hardware module `_init()` function:

```c
static esp_err_t foo_module_init(void) {
    ESP_LOGI(TAG, "Initializing Foo module...");
    
    // Configure hardware
    xyz_config_t config = {
        .i2c_addr = XYZ_DEFAULT_ADDR,
        .freq_hz = 100000,
    };
    
    // Initialize component
    esp_err_t ret = xyz_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize XYZ: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Use component APIs
    uint8_t data[16];
    ret = xyz_read(data, sizeof(data));
    
    return ESP_OK;
}
```

### 4. Use in Module Update Loop

```c
static void foo_module_update(void) {
    // Poll hardware via component API
    xyz_read(buffer, size);
    
    // Process data...
}
```

## Current Components Documentation

### 1. CAN Driver (`/ots-fw-shared/components/can_driver`)

**Purpose**: Generic CAN bus (TWAI) driver with automatic hardware detection and mock fallback

**Hardware**: ESP32 TWAI controller + CAN transceiver (SN65HVD230/MCP2551)

**Features**:
- Auto-detection of physical CAN hardware
- Mock mode for development without transceiver
- Statistics tracking (TX/RX counts, errors)
- Generic frame API (no protocol logic)

**Public API**:
- `can_driver_init()` - Initialize with auto-detection
- `can_driver_send()` - Send CAN frame (blocking)
- `can_driver_receive()` - Receive CAN frame (non-blocking)
- `can_driver_get_stats()` - Get TX/RX statistics

**Usage in fw-main**:
- Used by `sound_module.c` to send audio commands
- Protocol layer in `can_protocol.{h,c}` builds on top of component

**Documentation**: See `COMPONENT_PROMPT.md` in component directory

---

### 2. MCP23017 Driver (`/ots-fw-main/components/mcp23017_driver`)

**Purpose**: I2C I/O expander driver for 16-pin GPIO expansion over I2C

**Hardware**: MCP23017 I2C GPIO expander chip

**Features**:
- 16 GPIO pins (Port A: 8 pins, Port B: 8 pins)
- Individual pin or 8-bit port operations
- Per-pin direction control (input/output)
- Internal pull-up resistors
- Interrupt support (not yet implemented)

**I2C Addressing**:
- Board 0: 0x20 (configured as INPUT board)
- Board 1: 0x21 (configured as OUTPUT board)

**Public API**:
- `mcp23017_init()` - Initialize device
- `mcp23017_pin_mode()` - Set pin direction (input/output)
- `mcp23017_digital_write()` - Write single pin
- `mcp23017_digital_read()` - Read single pin
- `mcp23017_write_port()` - Write entire 8-bit port
- `mcp23017_read_port()` - Read entire 8-bit port

**Usage in fw-main**:
- Used by `io_expander.c` wrapper for hardware abstraction
- Buttons read from Board 0 (INPUT)
- LEDs controlled via Board 1 (OUTPUT)
- All hardware modules use via `module_io.c` abstraction

**Documentation**: See `COMPONENT_PROMPT.md` in component directory

---

### 3. ADS1015 Driver (`/ots-fw-main/components/ads1015_driver`)

**Purpose**: 12-bit I2C ADC driver for analog input reading

**Hardware**: ADS1015 4-channel 12-bit ADC with programmable gain amplifier

**Features**:
- 4 single-ended analog inputs (AIN0-AIN3)
- 12-bit resolution (0-4095 range)
- Programmable gain (±256mV to ±6.144V ranges)
- Single-shot or continuous conversion modes
- Configurable sample rate (128-3300 SPS)

**I2C Address**: 0x48 (default, configurable via ADDR pin)

**Public API**:
- `ads1015_init()` - Initialize device
- `ads1015_read_adc()` - Read single-ended ADC channel
- `ads1015_set_gain()` - Set PGA gain
- `ads1015_set_sample_rate()` - Set conversion rate

**Usage in fw-main**:
- Used by `troops_module.c` to read slider position
- Channel AIN0 connected to potentiometer slider
- Maps ADC value (0-4095) to percentage (0-100%)
- Polled every 100ms for troop deployment control

**Documentation**: See `COMPONENT_PROMPT.md` in component directory

---

### 4. HD44780 LCD Driver (`/ots-fw-main/components/hd44780_pcf8574`)

**Purpose**: I2C LCD display driver for 16x2 character LCDs via PCF8574 I2C backpack

**Hardware**: HD44780-compatible LCD + PCF8574 I2C expander backpack

**Features**:
- I2C interface (4-bit mode over I2C expander)
- 16x2 character display (configurable)
- Backlight control
- Cursor positioning
- Custom character support (not yet implemented)
- Write string operations

**I2C Address**: 0x27 (default) or 0x3F (configurable)

**Public API**:
- `lcd_init()` - Initialize LCD display
- `lcd_clear()` - Clear entire display
- `lcd_set_cursor()` - Position cursor (column, row)
- `lcd_write_string()` - Write string at current cursor
- `lcd_backlight()` - Control backlight on/off
- `lcd_write_char()` - Write single character

**Usage in fw-main**:
- Used by `troops_module.c` for troop count display
- Line 1: Current and max troop counts (e.g., "120K / 1.1M")
- Line 2: Deployment percentage and calculated troops (e.g., "50% (60K)")
- Uses intelligent unit scaling (K/M/B)
- Updated when game state changes or slider moves

**Documentation**: See `COMPONENT_PROMPT.md` in component directory

---

### 5. WS2812 RMT Driver (`/ots-fw-main/components/ws2812_rmt`)

**Purpose**: RGB LED strip driver for WS2812B/NeoPixel LEDs using ESP32 RMT peripheral

**Hardware**: WS2812B (NeoPixel) addressable RGB LEDs

**Features**:
- ESP32 RMT peripheral for precise timing (no bitbanging)
- Support for multiple LED strips
- 24-bit color (8-bit per channel RGB)
- Brightness control
- Color correction
- Effects library (optional)

**Public API**:
- `ws2812_init()` - Initialize RMT channel and configure strip
- `ws2812_set_pixel()` - Set individual pixel color
- `ws2812_set_all()` - Set all pixels to same color
- `ws2812_show()` - Update LED strip with buffered colors
- `ws2812_clear()` - Turn off all LEDs

**Usage in fw-main**:
- Potentially used for status indicators or effects
- Not yet integrated into main modules
- Reserved for future lighting module

**Documentation**: See `COMPONENT_PROMPT.md` in component directory

---

### 6. ESP HTTP Server Core (`/ots-fw-main/components/esp_http_server_core`)

**Purpose**: HTTP server utilities and OTA update handling

**Features**:
- HTTP server configuration helpers
- OTA firmware update handlers
- CORS support
- Static file serving
- WebSocket upgrade support

**Usage in fw-main**:
- Used by `ota_manager.c` for firmware updates
- Provides HTTP endpoint at port 3232
- Handles `/update` POST for OTA uploads

**Documentation**: See component implementation

## Creating New Components

### Step 1: Choose Component Location

**Decision criteria**:
- **Shared `/ots-fw-shared/components/`**: If reusable across multiple firmware projects (fw-main, fw-audiomodule, etc.)
- **Project `/ots-fw-main/components/`**: If specific to fw-main only

### Step 2: Create Component Structure

```bash
cd /path/to/components/
mkdir my_driver
cd my_driver

# Create required files
touch CMakeLists.txt
touch idf_component.yml
touch README.md
touch COMPONENT_PROMPT.md
mkdir include
touch include/my_driver.h
touch my_driver.c
```

### Step 3: Write Component Files

**CMakeLists.txt**:
```cmake
idf_component_register(
    SRCS "my_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer  # Add ESP-IDF dependencies
)
```

**idf_component.yml**:
```yaml
version: "1.0.0"
description: "Driver for XYZ hardware"
url: https://github.com/your-org/ots
dependencies:
  idf:
    version: ">=5.0.0"
```

**include/my_driver.h**:
```c
#ifndef MY_DRIVER_H
#define MY_DRIVER_H

#include "esp_err.h"

esp_err_t my_driver_init(void);
// ... other functions

#endif
```

**my_driver.c**: Implementation

**COMPONENT_PROMPT.md**: See template below

### Step 4: Document with COMPONENT_PROMPT.md

Create AI-friendly documentation (see template in next section)

### Step 5: Integrate in fw-main

1. Add to `ots-fw-main/src/CMakeLists.txt` REQUIRES
2. Include header in module source
3. Call init in module `_init()` function
4. Use API in module `_update()` function

## Component Prompt Template

Each component should have a `COMPONENT_PROMPT.md` file with this structure:

```markdown
# [Component Name] - ESP-IDF Component

## Hardware Overview

**Device**: [Chip name/model]
**Interface**: [I2C/SPI/UART/GPIO/etc.]
**Purpose**: [One-line description]

**Key specifications**:
- Voltage: [Operating voltage]
- Current: [Typical/max current]
- Interface speed: [e.g., I2C 100kHz/400kHz]
- Resolution: [if applicable]
- Channels/Pins: [Number of I/O]

**Datasheet**: [Link to datasheet]

## Component Scope

**This component handles**:
- ✅ Hardware initialization
- ✅ Low-level register access
- ✅ I2C/SPI transactions
- ✅ Error handling and recovery
- ✅ Hardware-specific timing

**This component does NOT handle**:
- ❌ Application logic
- ❌ Game state management
- ❌ WebSocket protocol
- ❌ High-level business rules

## Public API

### Initialization

```c
esp_err_t xyz_init(const xyz_config_t *config);
```
[Description, parameters, return values]

### Data Operations

```c
esp_err_t xyz_read(uint8_t *data, size_t len);
esp_err_t xyz_write(const uint8_t *data, size_t len);
```
[Description, parameters, return values]

### Configuration

```c
esp_err_t xyz_set_mode(xyz_mode_t mode);
```
[Description, parameters, return values]

## Implementation Notes

### I2C Communication

[Details about I2C addressing, timing, quirks]

### Register Map

[Key registers and their purposes]

### Error Handling

[How errors are detected and reported]

### Hardware Quirks

[Any device-specific oddities, workarounds, timing requirements]

## Usage Example

```c
#include "xyz_driver.h"

void my_module_init(void) {
    xyz_config_t config = {
        .i2c_addr = 0x48,
        .freq_hz = 100000,
    };
    
    ESP_ERROR_CHECK(xyz_init(&config));
    
    uint8_t data[4];
    esp_err_t ret = xyz_read(data, sizeof(data));
    if (ret == ESP_OK) {
        // Process data
    }
}
```

## Integration with fw-main

### Module Usage

[List which fw-main modules use this component]

Example:
- `troops_module.c` - Reads slider position every 100ms
- `alert_module.c` - Blinks LEDs on alerts

### Pin Connections

[GPIO pin mappings if applicable]

### Timing Requirements

[Polling rates, update frequencies, delays needed]

## Testing Strategy

### Hardware Tests

1. **Basic connectivity**: Detect device on I2C bus
2. **Read/Write**: Verify data transactions work
3. **Error cases**: Test with device disconnected
4. **Performance**: Measure transaction times

### Integration Tests

1. **Module init**: Component initializes without errors
2. **Data flow**: Data reaches application layer correctly
3. **Error recovery**: System handles device failures gracefully

## Dependencies

**ESP-IDF Components**:
- `driver` - For I2C/SPI/GPIO drivers
- `esp_timer` - For timing (if needed)
- [List others]

**External Components** (in idf_component.yml):
- None (or list if using component registry)

## Future Enhancements

- [ ] Interrupt support
- [ ] DMA transfers
- [ ] Power management
- [ ] Advanced features...

## References

- Datasheet: [URL]
- ESP-IDF I2C driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- [Other references]
```

## Design Principles

### 1. Separation of Concerns

**Hardware components handle**:
- Hardware initialization and configuration
- Low-level register/pin access
- Communication protocols (I2C/SPI transactions)
- Hardware timing and sequencing
- Error detection and reporting

**Application code (fw-main modules) handles**:
- Business logic (when to read/write)
- Game state integration
- Event handling
- UI/display formatting
- Protocol messages (WebSocket, CAN)

### 2. Error Handling

All component functions return `esp_err_t`:
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` for bad parameters
- `ESP_ERR_TIMEOUT` for communication timeouts
- `ESP_FAIL` for general errors
- Device-specific error codes (documented in component)

### 3. Configuration Structures

Use configuration structures for initialization:
```c
typedef struct {
    uint8_t i2c_addr;
    uint32_t freq_hz;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
} xyz_config_t;

// Default configuration helper
#define XYZ_DEFAULT_CONFIG() { \
    .i2c_addr = 0x48, \
    .freq_hz = 100000, \
    .sda_pin = GPIO_NUM_21, \
    .scl_pin = GPIO_NUM_22, \
}
```

### 4. Thread Safety

Components should be **not thread-safe by default** (for performance). If multi-threading needed, application must handle synchronization.

Document thread-safety requirements in component README.

### 5. Resource Management

- Components should **not** initialize I2C/SPI masters (fw-main does this globally)
- Components should **not** call `vTaskDelay()` (use timeout parameters)
- Components should clean up resources on failure

## Component Development Workflow

1. **Design phase**: Write COMPONENT_PROMPT.md first (AI guidance)
2. **API design**: Define public API in header file
3. **Implementation**: Write component code
4. **Testing**: Create hardware test in fw-main (test-xyz environment)
5. **Integration**: Use in actual hardware module
6. **Documentation**: Update README with usage examples

## Hardware Test Environments

Create dedicated test environments in `platformio.ini`:

```ini
[env:test-xyz]
extends = env:esp32-s3-dev
build_flags =
    ${env:esp32-s3-dev.build_flags}
    -DTEST_XYZ=1
```

Then in `main.c`:
```c
#ifdef TEST_XYZ
    // Test XYZ component standalone
    xyz_test_main();
#else
    // Normal firmware
    app_main();
#endif
```

Example test environments:
- `test-i2c` - I2C bus scan
- `test-adc` - ADC reading test
- `test-lcd` - LCD display test
- `test-outputs` - LED output test

## Best Practices

### ✅ DO

- Keep components hardware-focused
- Use ESP-IDF standard types (`esp_err_t`, `gpio_num_t`, etc.)
- Document all public functions with Doxygen comments
- Provide usage examples in README
- Handle all error cases
- Use consistent naming (`component_verb_noun()`)
- Log with ESP-IDF logging (`ESP_LOGI/LOGW/LOGE`)

### ❌ DON'T

- Mix hardware and application logic
- Use Arduino-style APIs (`digitalWrite()`, `delay()`)
- Block indefinitely in component functions
- Allocate large buffers on stack
- Assume hardware is always present
- Use global state (use opaque handles instead)

## Component Versioning

Follow semantic versioning in `idf_component.yml`:
- **Major**: Breaking API changes
- **Minor**: New features, backward compatible
- **Patch**: Bug fixes

Example: `1.2.3`
- Breaking change: 1.x.x → 2.0.0
- New feature: 1.2.x → 1.3.0
- Bug fix: 1.2.3 → 1.2.4

## Summary

**Component Philosophy**:
> Hardware components are pure hardware abstraction layers. They know nothing about games, WebSockets, or application logic. They only know how to talk to hardware.

**Integration Philosophy**:
> Application modules in fw-main use components to access hardware, then apply business logic on top.

This separation makes the codebase:
- **Modular**: Components can be developed/tested independently
- **Reusable**: Same component works in multiple projects
- **Testable**: Hardware can be mocked for unit tests
- **Maintainable**: Hardware changes don't affect application logic

## References

- ESP-IDF Component System: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-cmakelists-files
- ESP-IDF Component Registry: https://components.espressif.com/
- CMake in ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html
