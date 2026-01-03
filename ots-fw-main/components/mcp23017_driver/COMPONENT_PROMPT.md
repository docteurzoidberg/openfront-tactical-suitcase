# MCP23017 Driver Component - ESP-IDF Component

## Hardware Overview

**Device**: MCP23017 I2C 16-bit I/O Expander
**Manufacturer**: Microchip Technology
**Interface**: I2C (TWI/I2C-compatible)
**Purpose**: Add 16 GPIO pins to ESP32 via I2C interface

**Key specifications**:
- Voltage: 1.8V to 5.5V operation
- Current: 25mA max per pin, 125mA max per port
- Pins: 16 GPIO (2 × 8-bit ports: GPIOA, GPIOB)
- I2C Speed: 100 kHz, 400 kHz, 1.7 MHz
- Address: 0x20-0x27 (configurable via A0/A1/A2 pins)
- Features: Pull-up resistors, interrupt support, sequential operation

**Pin functions**:
- Each pin configurable as input or output
- Internal 100kΩ pull-up resistors available
- Interrupt-on-change capable (INT pins)
- Polarity inversion support

**Datasheet**: https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf

## Component Scope

**This component handles**:
- ✅ MCP23017 register read/write via I2C
- ✅ Pin direction configuration (input/output)
- ✅ Digital pin read/write (single pin)
- ✅ Port read/write (8-bit operations)
- ✅ Pull-up resistor control
- ✅ Error detection and reporting

**This component does NOT handle**:
- ❌ I2C bus initialization (fw-main does this globally)
- ❌ High-level button/LED abstractions
- ❌ Debouncing logic
- ❌ Application-specific pin mappings
- ❌ Interrupt handling (not yet implemented)

**Architecture separation**:
- **mcp23017_driver component**: Low-level register access (this component)
- **io_expander.c wrapper**: Board-level abstraction (in fw-main)
- **module_io.c**: High-level module I/O (buttons, LEDs) (in fw-main)
- **Hardware modules**: Application logic using I/O (in fw-main)

## Public API

### Data Types

```c
/**
 * @brief MCP23017 GPIO pin number (0-15)
 */
typedef uint8_t mcp23017_pin_t;

/**
 * @brief MCP23017 port selection
 */
typedef enum {
    MCP23017_PORT_A = 0,  ///< Port A (pins 0-7, GPIOA)
    MCP23017_PORT_B = 1   ///< Port B (pins 8-15, GPIOB)
} mcp23017_port_t;

/**
 * @brief Pin mode (direction)
 */
typedef enum {
    MCP23017_MODE_OUTPUT = 0,  ///< Output mode
    MCP23017_MODE_INPUT = 1    ///< Input mode
} mcp23017_mode_t;

/**
 * @brief Pin state (logic level)
 */
typedef enum {
    MCP23017_LOW = 0,   ///< Logic low (0V)
    MCP23017_HIGH = 1   ///< Logic high (VCC)
} mcp23017_state_t;
```

### Initialization

```c
/**
 * @brief Initialize MCP23017 device
 * 
 * Configures the device with default settings:
 * - All pins as inputs
 * - No pull-ups enabled
 * - Interrupts disabled
 * - Sequential operation enabled
 * 
 * @param i2c_addr I2C address (0x20-0x27)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_init(uint8_t i2c_addr);
```

**I2C address selection** (via A0/A1/A2 hardware pins):
```
Base: 0x20
A2 A1 A0 | Address
---------+---------
0  0  0  | 0x20  ← Board 0 (inputs) in OTS
0  0  1  | 0x21  ← Board 1 (outputs) in OTS
0  1  0  | 0x22
0  1  1  | 0x23
1  0  0  | 0x24
1  0  1  | 0x25
1  1  0  | 0x26
1  1  1  | 0x27
```

### Pin Configuration

```c
/**
 * @brief Set pin direction (input or output)
 * 
 * @param i2c_addr I2C device address
 * @param pin Pin number (0-15)
 * @param mode MCP23017_MODE_INPUT or MCP23017_MODE_OUTPUT
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_pin_mode(uint8_t i2c_addr, mcp23017_pin_t pin, mcp23017_mode_t mode);

/**
 * @brief Enable/disable pull-up resistor on input pin
 * 
 * @param i2c_addr I2C device address
 * @param pin Pin number (0-15)
 * @param enable true to enable pull-up, false to disable
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_pullup(uint8_t i2c_addr, mcp23017_pin_t pin, bool enable);
```

**Pin numbering**:
- Pins 0-7: Port A (GPIOA0-GPIOA7)
- Pins 8-15: Port B (GPIOB0-GPIOB7)

### Single Pin Operations

```c
/**
 * @brief Write to output pin
 * 
 * @param i2c_addr I2C device address
 * @param pin Pin number (0-15)
 * @param state MCP23017_LOW or MCP23017_HIGH
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_digital_write(uint8_t i2c_addr, mcp23017_pin_t pin, mcp23017_state_t state);

/**
 * @brief Read from input pin
 * 
 * @param i2c_addr I2C device address
 * @param pin Pin number (0-15)
 * @param[out] state Pointer to store pin state
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_digital_read(uint8_t i2c_addr, mcp23017_pin_t pin, mcp23017_state_t *state);
```

### Port Operations (8-bit)

```c
/**
 * @brief Write entire 8-bit port
 * 
 * More efficient than individual pin writes when updating multiple pins.
 * 
 * @param i2c_addr I2C device address
 * @param port MCP23017_PORT_A or MCP23017_PORT_B
 * @param value 8-bit value to write (bit 0 = pin 0, bit 7 = pin 7)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_write_port(uint8_t i2c_addr, mcp23017_port_t port, uint8_t value);

/**
 * @brief Read entire 8-bit port
 * 
 * More efficient than individual pin reads when reading multiple pins.
 * 
 * @param i2c_addr I2C device address
 * @param port MCP23017_PORT_A or MCP23017_PORT_B
 * @param[out] value Pointer to store 8-bit port value
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_read_port(uint8_t i2c_addr, mcp23017_port_t port, uint8_t *value);
```

### Port Direction Configuration

```c
/**
 * @brief Configure entire 8-bit port direction at once
 * 
 * More efficient than individual pin_mode calls.
 * 
 * @param i2c_addr I2C device address
 * @param port MCP23017_PORT_A or MCP23017_PORT_B
 * @param direction 8-bit direction mask (1=input, 0=output)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mcp23017_set_port_direction(uint8_t i2c_addr, mcp23017_port_t port, uint8_t direction);
```

**Example - Configure all pins as inputs**:
```c
mcp23017_set_port_direction(0x20, MCP23017_PORT_A, 0xFF);  // All inputs
mcp23017_set_port_direction(0x20, MCP23017_PORT_B, 0xFF);  // All inputs
```

**Example - Configure all pins as outputs**:
```c
mcp23017_set_port_direction(0x21, MCP23017_PORT_A, 0x00);  // All outputs
mcp23017_set_port_direction(0x21, MCP23017_PORT_B, 0x00);  // All outputs
```

## Implementation Notes

### Register Map (Key Registers)

**IOCON = 0x0A** (Configuration register):
- Bit 5 (SEQOP): Sequential operation mode
  - 0 = Sequential addressing enabled (default)
  - 1 = Sequential addressing disabled
- Bit 1 (INTPOL): Interrupt polarity
- Bit 0 (INTCC): Interrupt clearing method

**Direction registers**:
- IODIRA (0x00): Port A direction (1=input, 0=output)
- IODIRB (0x01): Port B direction (1=input, 0=output)

**Pull-up registers**:
- GPPUA (0x0C): Port A pull-up enable (1=enabled)
- GPPUB (0x0D): Port B pull-up enable (1=enabled)

**GPIO registers**:
- GPIOA (0x12): Port A data (read/write)
- GPIOB (0x13): Port B data (read/write)

**Output latch registers** (preferred for writes):
- OLATA (0x14): Port A output latch
- OLATB (0x15): Port B output latch

### I2C Communication

**Read operation**:
```c
// Read single register
uint8_t reg_addr = MCP23017_REG_GPIOA;
uint8_t data;
i2c_master_write_read_device(I2C_NUM_0, i2c_addr, &reg_addr, 1, &data, 1, timeout);
```

**Write operation**:
```c
// Write single register
uint8_t write_buf[2] = {MCP23017_REG_OLATA, 0xFF};
i2c_master_write_to_device(I2C_NUM_0, i2c_addr, write_buf, 2, timeout);
```

**Sequential mode** (default):
- After reading/writing a register, internal address pointer auto-increments
- Allows efficient multi-byte transfers
- Example: Write GPIOA then GPIOB in single transaction

### Pin-to-Port Mapping

```c
// Convert pin number (0-15) to port and bit
mcp23017_port_t port = (pin < 8) ? MCP23017_PORT_A : MCP23017_PORT_B;
uint8_t bit = pin & 0x07;  // Bit position within port (0-7)
```

### Read-Modify-Write Pattern

For single-bit operations, use read-modify-write:

```c
esp_err_t mcp23017_digital_write(uint8_t i2c_addr, mcp23017_pin_t pin, mcp23017_state_t state) {
    // 1. Determine port and bit
    mcp23017_port_t port = (pin < 8) ? MCP23017_PORT_A : MCP23017_PORT_B;
    uint8_t bit = pin & 0x07;
    
    // 2. Read current port value
    uint8_t port_value;
    esp_err_t ret = mcp23017_read_port(i2c_addr, port, &port_value);
    if (ret != ESP_OK) return ret;
    
    // 3. Modify bit
    if (state == MCP23017_HIGH) {
        port_value |= (1 << bit);   // Set bit
    } else {
        port_value &= ~(1 << bit);  // Clear bit
    }
    
    // 4. Write back
    return mcp23017_write_port(i2c_addr, port, port_value);
}
```

### Error Handling

**Common errors**:
- `ESP_ERR_INVALID_ARG`: Invalid pin number (>15) or NULL pointer
- `ESP_ERR_TIMEOUT`: I2C communication timeout
- `ESP_FAIL`: I2C NACK (device not responding)

**Device detection**:
```c
// Test if device is present on bus
esp_err_t ret = mcp23017_init(0x20);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "MCP23017 found at 0x20");
} else {
    ESP_LOGE(TAG, "MCP23017 not responding");
}
```

### Performance Optimization

**Port operations are faster** than individual pin operations:

```c
// Slow - 8 I2C transactions
for (int i = 0; i < 8; i++) {
    mcp23017_digital_write(0x21, i, states[i]);
}

// Fast - 1 I2C transaction
uint8_t port_value = 0;
for (int i = 0; i < 8; i++) {
    if (states[i]) port_value |= (1 << i);
}
mcp23017_write_port(0x21, MCP23017_PORT_A, port_value);
```

**Typical operation times** (100 kHz I2C):
- Single register write: ~300μs
- Single register read: ~400μs
- Port write (8 pins): ~300μs
- 8 individual writes: ~2.4ms

## Usage Example

### Basic Initialization

```c
#include "mcp23017_driver.h"

void app_main(void) {
    // Assume I2C bus already initialized
    
    // Initialize Board 0 at 0x20 (inputs)
    esp_err_t ret = mcp23017_init(0x20);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init MCP23017 at 0x20");
        return;
    }
    
    // Configure all pins as inputs with pull-ups
    mcp23017_set_port_direction(0x20, MCP23017_PORT_A, 0xFF);  // All inputs
    mcp23017_set_port_direction(0x20, MCP23017_PORT_B, 0xFF);
    
    // Enable pull-ups on button pins
    for (int pin = 0; pin < 16; pin++) {
        mcp23017_pullup(0x20, pin, true);
    }
    
    // Initialize Board 1 at 0x21 (outputs)
    ret = mcp23017_init(0x21);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init MCP23017 at 0x21");
        return;
    }
    
    // Configure all pins as outputs
    mcp23017_set_port_direction(0x21, MCP23017_PORT_A, 0x00);  // All outputs
    mcp23017_set_port_direction(0x21, MCP23017_PORT_B, 0x00);
    
    // Turn off all LEDs initially
    mcp23017_write_port(0x21, MCP23017_PORT_A, 0x00);
    mcp23017_write_port(0x21, MCP23017_PORT_B, 0x00);
}
```

### Reading Button State

```c
// Read single button (active-low with pull-up)
bool is_button_pressed(uint8_t pin) {
    mcp23017_state_t state;
    esp_err_t ret = mcp23017_digital_read(0x20, pin, &state);
    return (ret == ESP_OK && state == MCP23017_LOW);  // Active-low
}

// Read all buttons at once (efficient)
void read_all_buttons(bool buttons[16]) {
    uint8_t port_a, port_b;
    
    mcp23017_read_port(0x20, MCP23017_PORT_A, &port_a);
    mcp23017_read_port(0x20, MCP23017_PORT_B, &port_b);
    
    // Convert to button states (active-low, so invert)
    for (int i = 0; i < 8; i++) {
        buttons[i] = !(port_a & (1 << i));      // Port A buttons
        buttons[i+8] = !(port_b & (1 << i));    // Port B buttons
    }
}
```

### Controlling LEDs

```c
// Set single LED
void set_led(uint8_t pin, bool on) {
    mcp23017_digital_write(0x21, pin, on ? MCP23017_HIGH : MCP23017_LOW);
}

// Set multiple LEDs efficiently
void set_led_pattern(uint8_t pattern) {
    // pattern = 8-bit LED pattern for Port A
    mcp23017_write_port(0x21, MCP23017_PORT_A, pattern);
}

// LED chase animation
void led_chase(void) {
    for (int i = 0; i < 16; i++) {
        uint8_t pattern = (1 << (i & 0x07));
        if (i < 8) {
            mcp23017_write_port(0x21, MCP23017_PORT_A, pattern);
            mcp23017_write_port(0x21, MCP23017_PORT_B, 0x00);
        } else {
            mcp23017_write_port(0x21, MCP23017_PORT_A, 0x00);
            mcp23017_write_port(0x21, MCP23017_PORT_B, pattern);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Integration with fw-main

### Current Architecture

**Layer 1: mcp23017_driver component** (this component)
- Low-level register access
- Generic pin/port operations
- No application logic

**Layer 2: io_expander.c** (in fw-main)
- Board-level abstraction (Board 0 @ 0x20, Board 1 @ 0x21)
- Wrappers: `io_expander_init()`, `io_expander_read_inputs()`, `io_expander_write_outputs()`

**Layer 3: module_io.c** (in fw-main)
- High-level module I/O (buttons, LEDs)
- Pin mapping for hardware modules
- Functions: `moduleIO.readNukeButtons()`, `moduleIO.setLED()`

**Layer 4: Hardware modules** (in fw-main)
- Application logic
- Uses `module_io.c` API
- Examples: `nuke_module.c`, `alert_module.c`, `main_power_module.c`

### Board Configuration

**OTS Hardware uses 2 MCP23017 boards**:

**Board 0 (0x20) - INPUT board**:
- All 16 pins configured as INPUT with pull-up
- Used for buttons and switches
- Active-low (button press = LOW)

**Board 1 (0x21) - OUTPUT board**:
- All 16 pins configured as OUTPUT
- Used for LEDs and indicators
- Active-high (LED on = HIGH)

### Pin Mapping

See `/ots-fw-main/include/module_io.h` for full pin mappings.

**Example pins**:
```
Board 0 (Inputs):
- Pin 1: Nuke ATOM button
- Pin 2: Nuke HYDRO button
- Pin 3: Nuke MIRV button

Board 1 (Outputs):
- Pin 0: Alert WARNING LED
- Pin 1: Alert ATOM LED
- Pin 7: Main power LINK LED
- Pin 8: Nuke ATOM LED
```

### Initialization Sequence

```c
// In main.c
void app_main(void) {
    // 1. Initialize I2C bus
    i2c_master_init();
    
    // 2. Initialize MCP23017 boards via io_expander.c
    io_expander_init();  // Calls mcp23017_init() for both boards
    
    // 3. Initialize hardware modules
    module_manager_init();  // Registers and inits all modules
    
    // 4. Main loop
    while (1) {
        module_manager_update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### Usage in Modules

```c
// In nuke_module.c
static void nuke_module_update(void) {
    // Read button states (via module_io.c → io_expander.c → mcp23017_driver)
    bool atom_pressed = moduleIO.readButton(NUKE_BTN_ATOM_PIN);
    
    if (atom_pressed) {
        // Turn on LED (via module_io.c → io_expander.c → mcp23017_driver)
        moduleIO.setLED(NUKE_LED_ATOM_PIN, true);
    }
}
```

### Build Integration

In `/ots-fw-main/src/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c"
         "io_expander.c"
         "module_io.c"
         # ...
    INCLUDE_DIRS "../include"
    REQUIRES 
        driver
        mcp23017_driver  # This component
        # ...
)
```

## Testing Strategy

### Hardware Tests

**Test 1: Device detection**
```bash
pio run -e test-i2c -t upload
# Expected: MCP23017 detected at 0x20 and 0x21
```

**Test 2: Pin direction**
```c
// Configure as output and read back
mcp23017_pin_mode(0x21, 0, MCP23017_MODE_OUTPUT);
// Verify by reading IODIR register (should be 0 for output)
```

**Test 3: Output test**
```c
// Blink LED on pin 0
while (1) {
    mcp23017_digital_write(0x21, 0, MCP23017_HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));
    mcp23017_digital_write(0x21, 0, MCP23017_LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
}
```

**Test 4: Input test**
```c
// Read button state (with pull-up, active-low)
mcp23017_pin_mode(0x20, 1, MCP23017_MODE_INPUT);
mcp23017_pullup(0x20, 1, true);

mcp23017_state_t state;
mcp23017_digital_read(0x20, 1, &state);
ESP_LOGI(TAG, "Button: %s", state == MCP23017_LOW ? "PRESSED" : "RELEASED");
```

**Test 5: Port operations**
```c
// Test 8-bit port write/read
mcp23017_write_port(0x21, MCP23017_PORT_A, 0xAA);  // 10101010
uint8_t read_val;
mcp23017_read_port(0x21, MCP23017_PORT_A, &read_val);
assert(read_val == 0xAA);
```

### Integration Tests

**Test 1: Button to LED**
```c
// Read button, control LED
bool button = !mcp23017_digital_read(0x20, 1, ...);  // Active-low
mcp23017_digital_write(0x21, 8, button);  // LED follows button
```

**Test 2: Module integration**
```c
// Verify nuke module can read buttons and control LEDs
nuke_module_init();
nuke_module_update();
// Manually press button, verify LED lights up
```

## Dependencies

**ESP-IDF Components**:
- `driver` - I2C master driver (`i2c.h`)
- `esp_log` - Logging macros

**External Components**: None

**I2C bus initialization** (done by fw-main, not by component):
```c
// In main.c or io_expander.c
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_8,
    .scl_io_num = GPIO_NUM_9,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000,
};
i2c_param_config(I2C_NUM_0, &conf);
i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
```

## Future Enhancements

### Short Term
- [ ] Interrupt-on-change support (INTA/INTB pins)
- [ ] Polarity inversion configuration
- [ ] Open-drain output mode

### Medium Term
- [ ] Batch operations (read/write multiple pins atomically)
- [ ] Pin state caching (reduce I2C traffic)
- [ ] Interrupt handler registration API

### Long Term
- [ ] Support for MCP23008 (8-bit version)
- [ ] Support for MCP23S17 (SPI version)
- [ ] DMA-based I2C transfers

## Performance Characteristics

**I2C timing** (100 kHz):
- Single pin write: ~300μs
- Single pin read: ~400μs
- Port write: ~300μs
- Port read: ~400μs

**I2C timing** (400 kHz):
- Single pin write: ~100μs
- Single pin read: ~150μs
- Port write: ~100μs
- Port read: ~150μs

**Recommendations**:
- Use port operations when updating multiple pins
- Use 400 kHz I2C for faster response
- Read all inputs once per cycle, cache values

## References

- **MCP23017 Datasheet**: https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf
- **ESP-IDF I2C Driver**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- **Application Note**: AN1043 (Interfacing MCP23017 to Microcontroller)

## Troubleshooting

### Issue: Device not responding (NACK)

**Symptom**: `mcp23017_init()` returns `ESP_FAIL`

**Causes**:
- Wrong I2C address (check A0/A1/A2 pins)
- I2C bus not initialized
- SDA/SCL wiring issues
- Power supply not connected

**Solution**:
1. Run I2C bus scan: `pio run -e test-i2c -t upload`
2. Verify address via scope or logic analyzer
3. Check VCC (should be 3.3V or 5V)
4. Verify SDA/SCL pull-up resistors (4.7kΩ)

### Issue: Read values always 0xFF

**Symptom**: `mcp23017_digital_read()` always returns HIGH

**Causes**:
- Pin configured as output instead of input
- No pull-up resistor on input
- Button not connected properly

**Solution**:
1. Verify pin mode: `mcp23017_pin_mode(addr, pin, MCP23017_MODE_INPUT)`
2. Enable pull-up: `mcp23017_pullup(addr, pin, true)`
3. Check button wiring (one side to pin, other to GND)

### Issue: Outputs not changing

**Symptom**: LEDs don't light up when writing HIGH

**Causes**:
- Pin configured as input instead of output
- LED polarity reversed
- Insufficient current drive

**Solution**:
1. Verify pin mode: `mcp23017_pin_mode(addr, pin, MCP23017_MODE_OUTPUT)`
2. Check LED polarity (anode to pin, cathode to GND)
3. Add current-limiting resistor (220Ω-1kΩ)
4. Max current per pin: 25mA

### Issue: I2C bus hangs

**Symptom**: Firmware freezes during I2C operations

**Causes**:
- I2C bus stuck (SDA or SCL held low)
- Multiple masters on bus without arbitration
- Incorrect pull-up resistor values

**Solution**:
1. Power cycle the board
2. Add I2C bus recovery routine
3. Use proper pull-up resistors (2.2kΩ-4.7kΩ for 100kHz, 1kΩ-2.2kΩ for 400kHz)
4. Check for short circuits on SDA/SCL lines
