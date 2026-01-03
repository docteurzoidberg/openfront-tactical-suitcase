# CAN Driver Component - ESP-IDF Component

## Hardware Overview

**Device**: ESP32 TWAI (Two-Wire Automotive Interface) controller + External CAN transceiver
**Interface**: CAN 2.0B (internal TWAI peripheral + external transceiver chip)
**Purpose**: Generic CAN bus communication with automatic hardware detection and mock fallback

**Key specifications**:
- Protocol: CAN 2.0B (11-bit and 29-bit IDs supported)
- Bitrate: 500 kbps (configurable)
- Frame format: Standard (11-bit ID) or Extended (29-bit ID)
- Data length: 0-8 bytes per frame
- External transceiver: SN65HVD230, MCP2551, or compatible

**Transceiver chips**:
- **SN65HVD230**: 3.3V, low-power, 1 Mbps max
- **MCP2551**: 5V, classic, 1 Mbps max
- **TJA1050**: 5V, high-speed, 1 Mbps max

**ESP32 TWAI Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html

## Component Scope

**This component handles**:
- ✅ TWAI peripheral initialization and configuration
- ✅ CAN frame transmission (blocking with timeout)
- ✅ CAN frame reception (non-blocking queue)
- ✅ Automatic hardware detection (TWAI + transceiver)
- ✅ Mock mode fallback (logging only, no hardware)
- ✅ Statistics tracking (TX/RX counts, errors)
- ✅ Error handling and recovery

**This component does NOT handle**:
- ❌ CAN protocol layer (message IDs, payload encoding)
- ❌ Application-specific message types
- ❌ Message routing or filtering
- ❌ Module discovery or addressing
- ❌ Higher-level protocols (CANopen, J1939, etc.)

**Architecture separation**:
- **can_driver component**: Generic hardware layer (this component)
- **Application protocol layer**: Message IDs, encoding/decoding (in fw-main)
- **Module-specific code**: Business logic using CAN (in fw-main modules)

## Public API

### Data Types

```c
/**
 * @brief CAN frame structure (generic)
 */
typedef struct {
    uint32_t id;              ///< CAN message ID (11-bit or 29-bit)
    uint8_t data[8];          ///< Frame data payload (0-8 bytes)
    uint8_t dlc;              ///< Data length code (0-8)
    bool extended;            ///< Extended frame format (29-bit ID)
    bool rtr;                 ///< Remote transmission request
} can_frame_t;

/**
 * @brief CAN driver configuration
 */
typedef struct {
    gpio_num_t tx_pin;        ///< TWAI TX GPIO pin
    gpio_num_t rx_pin;        ///< TWAI RX GPIO pin
    uint32_t bitrate;         ///< CAN bitrate in bps (e.g., 500000)
    bool use_mock;            ///< Force mock mode (ignore hardware)
} can_config_t;

/**
 * @brief CAN driver statistics
 */
typedef struct {
    uint32_t tx_count;        ///< Total frames transmitted
    uint32_t rx_count;        ///< Total frames received
    uint32_t tx_errors;       ///< Transmission errors
    uint32_t rx_errors;       ///< Reception errors
    bool is_mock;             ///< Currently using mock mode
} can_stats_t;
```

### Initialization

```c
/**
 * @brief Initialize CAN driver with automatic hardware detection
 * 
 * Attempts to initialize physical TWAI hardware. If hardware detection
 * fails (no transceiver present), automatically falls back to mock mode.
 * 
 * Mock mode logs all TX/RX operations to console but doesn't access hardware.
 * 
 * @param config Configuration structure (NULL = use defaults)
 * @return ESP_OK on success (physical or mock), error code on failure
 */
esp_err_t can_driver_init(const can_config_t *config);
```

**Default configuration**:
```c
can_config_t default_config = {
    .tx_pin = GPIO_NUM_5,      // Adjust for your board
    .rx_pin = GPIO_NUM_4,      // Adjust for your board
    .bitrate = 500000,         // 500 kbps
    .use_mock = false,         // Auto-detect hardware
};
```

### Data Transmission

```c
/**
 * @brief Send a CAN frame (blocking with timeout)
 * 
 * In physical mode: Transmits frame via TWAI peripheral
 * In mock mode: Logs frame to console and returns immediately
 * 
 * @param frame Pointer to CAN frame to send
 * @param timeout_ms Timeout in milliseconds (0 = no wait)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout, other errors on failure
 */
esp_err_t can_driver_send(const can_frame_t *frame, uint32_t timeout_ms);
```

**Example**:
```c
can_frame_t frame = {
    .id = 0x420,
    .dlc = 4,
    .extended = false,
    .rtr = false,
};
frame.data[0] = 0x01;
frame.data[1] = 0x02;
frame.data[2] = 0x03;
frame.data[3] = 0x04;

esp_err_t ret = can_driver_send(&frame, 100);  // 100ms timeout
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Frame sent successfully");
}
```

### Data Reception

```c
/**
 * @brief Receive a CAN frame (non-blocking)
 * 
 * Checks if a frame is available in the receive queue and retrieves it.
 * Does not block if no frame is available.
 * 
 * In physical mode: Reads from TWAI RX queue
 * In mock mode: Always returns ESP_ERR_NOT_FOUND (no frames in mock)
 * 
 * @param[out] frame Pointer to buffer for received frame
 * @return ESP_OK if frame received, ESP_ERR_NOT_FOUND if no frame available
 */
esp_err_t can_driver_receive(can_frame_t *frame);
```

**Typical polling pattern**:
```c
void can_poll_task(void *arg) {
    can_frame_t frame;
    while (1) {
        esp_err_t ret = can_driver_receive(&frame);
        if (ret == ESP_OK) {
            // Process received frame
            process_frame(&frame);
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Poll every 10ms
    }
}
```

### Statistics

```c
/**
 * @brief Get driver statistics
 * 
 * @param[out] stats Pointer to statistics structure to fill
 * @return ESP_OK on success
 */
esp_err_t can_driver_get_stats(can_stats_t *stats);
```

**Example**:
```c
can_stats_t stats;
can_driver_get_stats(&stats);
ESP_LOGI(TAG, "CAN Stats: TX=%lu RX=%lu Errors=%lu/%lu Mock=%d",
         stats.tx_count, stats.rx_count, 
         stats.tx_errors, stats.rx_errors,
         stats.is_mock);
```

## Implementation Notes

### Auto-Detection Logic

```c
esp_err_t can_driver_init(const can_config_t *config) {
    if (config && config->use_mock) {
        // User forced mock mode
        return init_mock_mode();
    }
    
    // Try to initialize physical TWAI
    esp_err_t ret = init_twai_physical(config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CAN: Physical TWAI mode active");
        return ESP_OK;
    }
    
    // Hardware init failed - fallback to mock
    ESP_LOGW(TAG, "CAN: Hardware not detected, using mock mode");
    return init_mock_mode();
}
```

**When mock mode is used**:
- TWAI peripheral initialization fails (no transceiver, wrong pins, etc.)
- User explicitly requests mock via `config->use_mock = true`
- Development environment without CAN hardware

### Physical TWAI Mode

**Initialization sequence**:
1. Configure GPIO pins (TX/RX)
2. Initialize TWAI driver with timing config
3. Set acceptance filter (accept all by default)
4. Start TWAI driver
5. Create RX queue for incoming frames

**TWAI timing for 500 kbps** (80 MHz APB clock):
```c
twai_timing_config_t timing_config = TWAI_TIMING_CONFIG_500KBITS();
// Or custom:
// .brp = 8, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false
```

**Acceptance filter** (accept all messages):
```c
twai_filter_config_t filter_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
// Or custom:
// .acceptance_code = 0x420, .acceptance_mask = 0x7F0
```

**General config**:
```c
twai_general_config_t general_config = TWAI_GENERAL_CONFIG_DEFAULT(
    GPIO_NUM_5,   // TX pin
    GPIO_NUM_4,   // RX pin
    TWAI_MODE_NORMAL
);
```

### Mock Mode

**Behavior**:
- `can_driver_send()`: Logs frame details to console with decoded data
- `can_driver_receive()`: Always returns `ESP_ERR_NOT_FOUND` (no RX)
- All statistics tracked (TX count increments, RX stays 0)
- No GPIO configuration or peripheral access

**Mock logging format**:
```
I (12345) CAN: [MOCK TX] ID=0x420 DLC=4 Data=[01 02 03 04]
I (12350) CAN: [MOCK TX] ID=0x421 DLC=2 Data=[AA BB]
```

### Error Handling

**Common errors**:
- `ESP_ERR_INVALID_ARG`: NULL pointer or invalid configuration
- `ESP_ERR_TIMEOUT`: Frame transmission timeout (bus busy/off)
- `ESP_ERR_NOT_FOUND`: No frame available in RX queue
- `ESP_FAIL`: General TWAI driver error

**Error recovery**:
- TWAI driver has automatic error recovery (bus-off → error-active)
- Component tracks error counts in statistics
- Application should monitor error rates and reset if needed

### Thread Safety

**Not thread-safe by default**:
- `can_driver_send()`: Single writer assumed
- `can_driver_receive()`: Single reader assumed
- If multi-threaded access needed, use FreeRTOS mutexes in application

**Typical usage pattern**:
- One task sends frames (e.g., sound module)
- One task receives frames (e.g., message router)
- No mutex needed if single writer/reader

## Usage Example

### Basic Initialization and Send

```c
#include "can_driver.h"

void app_main(void) {
    // Initialize with defaults (auto-detect hardware)
    can_config_t config = {
        .tx_pin = GPIO_NUM_5,
        .rx_pin = GPIO_NUM_4,
        .bitrate = 500000,
        .use_mock = false,
    };
    
    esp_err_t ret = can_driver_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CAN driver init failed");
        return;
    }
    
    // Send a frame
    can_frame_t frame = {
        .id = 0x420,
        .dlc = 2,
        .extended = false,
        .rtr = false,
    };
    frame.data[0] = 0x01;  // Command byte
    frame.data[1] = 0x05;  // Sound index
    
    ret = can_driver_send(&frame, 100);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Frame sent");
    }
    
    // Check stats
    can_stats_t stats;
    can_driver_get_stats(&stats);
    ESP_LOGI(TAG, "Mode: %s", stats.is_mock ? "Mock" : "Physical");
}
```

### Receive Loop

```c
void can_rx_task(void *arg) {
    can_frame_t frame;
    
    while (1) {
        esp_err_t ret = can_driver_receive(&frame);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "RX: ID=0x%03X DLC=%d", frame.id, frame.dlc);
            
            // Route to handler based on ID
            if (frame.id == 0x422) {
                handle_status_message(&frame);
            } else if (frame.id == 0x423) {
                handle_ack_message(&frame);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Poll every 10ms
    }
}

void app_main(void) {
    can_driver_init(NULL);
    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
}
```

## Integration with fw-main

### Current Usage

**sound_module.c**:
- Sends PLAY_SOUND and STOP_SOUND commands via CAN
- Uses protocol layer (`can_protocol.h`) to build frames
- Calls `can_driver_send()` with 100ms timeout
- Statistics tracked for debugging

**can_protocol.c** (application layer):
- Builds CAN frames with specific IDs (0x420-0x423)
- Encodes multi-byte integers (little-endian)
- Decodes received frames (STATUS, ACK messages)
- Logs decoded parameters

### Pin Connections

**ESP32-S3 DevKit**:
- TWAI TX: GPIO 5 (configurable)
- TWAI RX: GPIO 4 (configurable)

**CAN Transceiver (SN65HVD230)**:
```
ESP32-S3          SN65HVD230        CAN Bus
---------         ----------        --------
GPIO 5 (TX) ----> TXD
GPIO 4 (RX) <---- RXD
3.3V -----------> VCC
GND ------------> GND
                  CANH -----------> CANH
                  CANL -----------> CANL
```

**Termination resistor**: 120Ω between CANH and CANL at each end of bus

### Build Integration

In `/ots-fw-main/src/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c"
         "sound_module.c"
         "can_protocol.c"
         # ...
    INCLUDE_DIRS "../include"
    REQUIRES 
        driver
        can_driver  # This component
        # ...
)
```

### Initialization Timing

```c
void app_main(void) {
    // 1. Initialize I2C bus (for other modules)
    i2c_init();
    
    // 2. Initialize CAN driver
    can_driver_init(NULL);  // Auto-detect
    
    // 3. Initialize hardware modules
    module_manager_init();
    
    // 4. Start main loop
    while (1) {
        module_manager_update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Testing Strategy

### Hardware Tests

**Test 1: I2C bus scan** (prerequisite)
```bash
pio run -e test-i2c -t upload
# Verify no conflicts with CAN GPIO pins
```

**Test 2: CAN loopback** (physical mode)
- Connect TX and RX pins together
- Send frame and verify reception
- Check statistics show TX and RX counts match

**Test 3: Two-node communication**
- Two ESP32 boards with transceivers
- Send frames between boards
- Verify bidirectional communication

**Test 4: Mock mode**
- Run firmware without CAN transceiver
- Verify mock mode activates automatically
- Check console logs show [MOCK TX] messages

### Integration Tests

**Test 1: Sound module integration**
```c
// In sound_module.c
sound_module_play(SOUND_GAME_START, true, SOUND_PRIORITY_HIGH);
// Expected: CAN frame sent with ID 0x420
```

**Test 2: Statistics tracking**
```c
// Send 10 frames
for (int i = 0; i < 10; i++) {
    can_driver_send(&frame, 100);
}

// Check stats
can_stats_t stats;
can_driver_get_stats(&stats);
assert(stats.tx_count == 10);
```

**Test 3: Error handling**
```c
// Invalid frame (DLC > 8)
can_frame_t bad_frame = { .dlc = 10 };
esp_err_t ret = can_driver_send(&bad_frame, 100);
assert(ret == ESP_ERR_INVALID_ARG);
```

## Dependencies

**ESP-IDF Components**:
- `driver` - TWAI peripheral driver
- `esp_log` - Logging macros
- `freertos` - FreeRTOS tasks and queues

**External Components**: None

**idf_component.yml**:
```yaml
version: "1.0.0"
description: "Generic CAN bus driver with auto-detection"
url: https://github.com/your-org/ots
dependencies:
  idf:
    version: ">=5.0.0"
```

## Future Enhancements

### Short Term
- [ ] Configurable acceptance filters (reduce CPU load)
- [ ] Error recovery callback hooks
- [ ] Bus-off state detection and notification

### Medium Term
- [ ] Multi-frame message support (>8 bytes via segmentation)
- [ ] Priority-based TX queue (FreeRTOS priority queue)
- [ ] RX buffer overflow protection

### Long Term
- [ ] CANopen protocol layer (optional)
- [ ] J1939 protocol layer (optional)
- [ ] CAN-FD support (if ESP32 hardware supports it)
- [ ] Diagnostic interface (bus load, error frames)

## Performance Characteristics

**Physical mode**:
- TX latency: ~1-2ms (depends on bus load)
- RX latency: <1ms (interrupt-driven)
- Max throughput: ~7000 frames/sec at 500 kbps (theoretical)

**Mock mode**:
- TX latency: <100μs (just logging)
- RX: Not supported
- Zero CPU overhead (no interrupts)

**Memory usage**:
- Static: ~1KB (driver state, buffers)
- Stack: ~2KB per task using component
- Heap: None (no dynamic allocation)

## Protocol Architecture

This component is **hardware-only**. For multi-module CAN protocol architecture, see:

- `/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md` - Multi-module protocol design
- `/ots-fw-main/docs/CAN_MULTI_MODULE_ROADMAP.md` - Implementation roadmap

**Current audio protocol** (application-specific):
- Message IDs: 0x420-0x423 (play/stop/status/ack)
- Defined in `/ots-fw-main/include/can_protocol.h`
- Not part of this component

**Future generic protocol**:
- Priority-based ID allocation: [PPP][TTTT][AAAA]
- Module discovery (WHO_IS_THERE → I_AM_HERE)
- Message routing and filtering
- Will be added as additional component layer

## References

- **ESP-IDF TWAI Driver**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html
- **CAN 2.0B Specification**: Bosch CAN Specification 2.0
- **SN65HVD230 Datasheet**: https://www.ti.com/product/SN65HVD230
- **MCP2551 Datasheet**: https://www.microchip.com/en-us/product/MCP2551
- **CAN Bus Basics**: https://www.ti.com/lit/an/sloa101b/sloa101b.pdf

## Troubleshooting

### Issue: Mock mode always active

**Symptom**: Logs show "[MOCK TX]" even with transceiver connected

**Causes**:
- Wrong GPIO pins configured
- Transceiver not powered
- Transceiver RX/TX swapped
- Missing termination resistors

**Solution**:
1. Check transceiver VCC has 3.3V or 5V
2. Verify TX/RX pin numbers match hardware
3. Check CANH/CANL connections
4. Add 120Ω termination at bus ends

### Issue: TX timeout errors

**Symptom**: `can_driver_send()` returns `ESP_ERR_TIMEOUT`

**Causes**:
- Bus-off state (too many errors)
- No other node on bus (ACK missing)
- Transceiver in standby/sleep mode

**Solution**:
1. Connect second CAN node (or use loopback)
2. Check bus termination (120Ω resistors)
3. Reset TWAI driver to clear bus-off state

### Issue: No frames received

**Symptom**: `can_driver_receive()` always returns `ESP_ERR_NOT_FOUND`

**Causes**:
- Acceptance filter too restrictive
- No frames being sent by other nodes
- RX pin not connected

**Solution**:
1. Use `TWAI_FILTER_CONFIG_ACCEPT_ALL()`
2. Verify another node is transmitting
3. Check RX pin wiring to transceiver RXD

### Issue: High error counts

**Symptom**: `stats.tx_errors` or `stats.rx_errors` increasing rapidly

**Causes**:
- Bus termination missing/incorrect
- Wiring too long or noisy
- Bitrate mismatch between nodes

**Solution**:
1. Add/check 120Ω termination resistors
2. Reduce bus cable length (<40m for 500 kbps)
3. Verify all nodes use same bitrate (500 kbps)
4. Use twisted-pair cable for CANH/CANL
