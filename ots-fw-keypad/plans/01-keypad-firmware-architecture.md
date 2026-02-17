# OTS Keypad Firmware - Architecture Plan

This document defines the detailed architecture for implementing the keypad firmware before writing code.

## Design Goals

1. **Simplicity**: Keypad is a "dumb" input/output device with no game logic
2. **Modularity**: Clean separation between hardware layers (matrix, LEDs, CAN)
3. **USB-Ready**: Architecture allows swapping CAN output with USB HID later
4. **Testability**: Each module can be tested independently
5. **Performance**: <5ms matrix scan rate, <10ms LED update latency

---

## Module Architecture

### 1. Matrix Scanner Module

**File**: `src/matrix_scanner.c` + `include/matrix_scanner.h`

**Responsibility**: 
- Scan 3×7 key matrix at high frequency (200Hz = 5ms intervals)
- Debounce key state changes
- Generate press/release events
- Provide key state query API

**Public API**:
```c
typedef enum {
    KEY_STATE_RELEASED = 0,
    KEY_STATE_PRESSED = 1
} key_state_t;

typedef void (*key_event_callback_t)(uint8_t key_id, key_state_t state);

esp_err_t matrix_scanner_init(key_event_callback_t callback);
esp_err_t matrix_scanner_start(void);
esp_err_t matrix_scanner_stop(void);
key_state_t matrix_scanner_get_key_state(uint8_t key_id);
```

**Internal Implementation**:

```c
// Private data structures
typedef struct {
    key_state_t current_state;      // Current stable state
    key_state_t raw_state;          // Last raw scan result
    uint32_t debounce_timer;        // Timestamp for debounce
    bool debounce_active;           // Debouncing in progress
} key_state_internal_t;

typedef struct {
    key_state_internal_t keys[15];  // State for all 15 keys
    key_event_callback_t callback;  // Event callback
    TaskHandle_t scan_task;         // FreeRTOS task handle
    bool running;                   // Scanner active flag
} matrix_scanner_ctx_t;

// Private functions
static void matrix_scanner_task(void *arg);
static void scan_matrix_once(uint8_t row_states[MATRIX_ROWS][MATRIX_COLS]);
static uint8_t matrix_to_key_id(uint8_t row, uint8_t col);
static void process_key_state(uint8_t key_id, key_state_t raw_state, uint32_t now_ms);
static void configure_gpio(void);
```

**Scanning Algorithm**:
1. Set all row GPIOs to input (high-Z)
2. Set Row 0 to output LOW
3. Read all 7 column GPIOs (LOW = key pressed in Row 0)
4. Repeat for Row 1, Row 2
5. Update internal state matrix
6. Process debouncing for each key that changed
7. Trigger callback for stable state transitions

**Debouncing Logic**:
- When raw state changes: Start debounce timer (20ms default)
- Continue scanning but don't trigger events
- After 20ms: If raw state still matches, update stable state → trigger event
- If raw state changes during debounce: Restart timer

**Key ID Mapping**:
```c
// Convert (row, col) to key_id (1-15)
// Row2/Col3 = K15, all others sequential left-to-right, top-to-bottom
uint8_t matrix_to_key_id(uint8_t row, uint8_t col) {
    if (row == 2 && col == 3) return 15;  // Spacebar
    if (row == 2) return 0;  // Unused position
    return (row * MATRIX_COLS) + col + 1;
}
```

**Task Structure**:
- Priority: `tskIDLE_PRIORITY + 2` (moderate priority)
- Stack: 2048 bytes
- Loop: `vTaskDelay(pdMS_TO_TICKS(5))` for 200Hz scan rate
- Runs continuously after `matrix_scanner_start()`

---

### 2. LED Controller Module

**File**: `src/led_controller.c` + `include/led_controller.h`

**Responsibility**:
- Control 15× SK6812-MINI-E RGB LEDs
- Maintain LED state array (color + brightness per LED)
- Update physical LEDs via RMT peripheral
- Provide simple on/off/color API (no complex animations for now)

**Public API**:
```c
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_rgb_t;

esp_err_t led_controller_init(void);
esp_err_t led_controller_set_key(uint8_t key_id, bool on, led_rgb_t color);
esp_err_t led_controller_set_bulk(uint16_t key_bitmask, bool on, led_rgb_t color);
esp_err_t led_controller_set_all(bool on, led_rgb_t color);
esp_err_t led_controller_update(void);  // Push state to LEDs
esp_err_t led_controller_get_key(uint8_t key_id, bool *on, led_rgb_t *color);
```

**Internal Implementation**:

```c
// Private data structures
typedef struct {
    bool on;              // LED on/off
    led_rgb_t color;      // RGB color (0-255 each)
} led_state_t;

typedef struct {
    led_state_t leds[NUM_LEDS];   // State for all 15 LEDs
    uint8_t brightness_global;     // Global brightness divisor (0-255)
    rmt_channel_handle_t rmt_chan; // RMT channel handle
    bool needs_update;             // Dirty flag
} led_controller_ctx_t;

// Private functions
static esp_err_t configure_rmt(void);
static esp_err_t write_leds_to_hardware(void);
static void rgb_to_rmt_items(led_rgb_t color, bool on, rmt_symbol_word_t *items);
```

**RMT Configuration**:
- Use ESP-IDF's `led_strip` component (handles SK6812 timing)
- GPIO: `GPIO_RGB_DATA` (GPIO 3)
- Encoding: GRB order (SK6812 standard)
- Timing: 0=0.3µs+0.9µs, 1=0.6µs+0.6µs (auto-handled by led_strip)

**Update Strategy**:
- Setting LED state marks `needs_update = true`
- Call `led_controller_update()` to push to hardware (explicit update)
- Allows batching multiple LED changes before hardware write

**Initial State**:
- All LEDs OFF on init
- Default color: White (255, 255, 255)
- Global brightness: 128 (50%)

---

### 3. CAN Handler Module

**File**: `src/can_handler.c` + `include/can_handler.h`

**Responsibility**:
- Initialize CAN bus using shared `can_driver` component
- Send key events to main controller
- Receive LED commands from main controller
- Handle module discovery protocol

**Public API**:
```c
esp_err_t can_handler_init(void);
esp_err_t can_handler_start(void);
esp_err_t can_handler_stop(void);
esp_err_t can_handler_send_key_event(uint8_t key_id, key_state_t state);
```

**Internal Implementation**:

```c
// Private data structures
typedef struct {
    TaskHandle_t rx_task;           // CAN RX task
    QueueHandle_t tx_queue;         // Outgoing message queue
    can_driver_handle_t can_handle; // CAN driver handle
    bool running;                   // Handler active
} can_handler_ctx_t;

// Private functions
static void can_rx_task(void *arg);
static void can_tx_task(void *arg);
static void handle_led_set_message(const twai_message_t *msg);
static void handle_led_bulk_message(const twai_message_t *msg);
static void handle_module_query(const twai_message_t *msg);
static void send_module_announce(void);
```

**CAN Message Formats** (from shared component `can_protocol_keypad`):

**Outgoing - Key Event**:
```c
// CAN ID: 0x430 + key_id (0x431-0x43F)
typedef struct {
    uint8_t key_id;      // 1-15
    uint8_t state;       // 0=released, 1=pressed
    uint16_t timestamp;  // ms since boot (for debounce verification)
} __attribute__((packed)) can_msg_key_event_t;
```

**Incoming - LED Set (Single)**:
```c
// CAN ID: 0x430
typedef struct {
    uint8_t key_id;      // 1-15 (0xFF = all)
    uint8_t state;       // 0=off, 1=on
    uint8_t r, g, b;     // RGB color
} __attribute__((packed)) can_msg_led_set_t;
```

**Incoming - LED Set (Bulk)**:
```c
// CAN ID: 0x440
typedef struct {
    uint16_t key_bitmask;  // Bit 0=K1, Bit 14=K15
    uint8_t state;         // 0=off, 1=on
    uint8_t r, g, b;       // RGB color
} __attribute__((packed)) can_msg_led_bulk_t;
```

**Module Discovery**:
- Uses `can_protocol_discovery` component
- Responds to `MODULE_QUERY` (CAN ID 0x7FE)
- Sends `MODULE_ANNOUNCE` (CAN ID 0x7FD) with:
  - Module type: `0x05` (keypad)
  - Capabilities: 15 keys, 15 RGB LEDs
  - Firmware version from `config.h`

**Task Structure**:
- **RX Task**: Priority `tskIDLE_PRIORITY + 3`, Stack 3072 bytes
  - Blocks on CAN driver receive queue
  - Processes LED commands → calls `led_controller_set_*()`
  - Handles discovery messages
- **TX Task**: Optional (for queued sending, or send directly from callback)

**Key Event Flow**:
```
matrix_scanner callback → can_handler_send_key_event() 
  → Build CAN message → Send via can_driver
```

---

### 4. Main Control Module

**File**: `src/main.c`

**Responsibility**:
- System initialization (NVS, logging, GPIO)
- Mode detection (CAN vs USB, future)
- Start all modules in correct order
- Coordinate shutdown
- Watchdog feeding (if needed)

**Initialization Sequence**:
```c
void app_main(void) {
    // 1. ESP-IDF basics
    nvs_flash_init();
    esp_log_level_set("*", ESP_LOG_INFO);
    
    // 2. Log startup info
    ESP_LOGI(TAG, "OTS Keypad v%s", OTS_KEYPAD_VERSION);
    ESP_LOGI(TAG, "Matrix: %dx%d, %d keys", MATRIX_ROWS, MATRIX_COLS, NUM_KEYS);
    
    // 3. Initialize modules (don't start yet)
    led_controller_init();          // Init RMT, set all LEDs off
    can_handler_init();             // Init CAN driver
    matrix_scanner_init(on_key_event); // Init GPIO, register callback
    
    // 4. Start modules (bottom-up: LED → CAN → Matrix)
    led_controller_update();        // Push initial LED state
    can_handler_start();            // Start CAN RX/TX tasks
    matrix_scanner_start();         // Start matrix scanning task
    
    // 5. Send module announce
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for CAN stabilization
    can_handler_send_module_announce();
    
    ESP_LOGI(TAG, "Keypad module ready");
    
    // 6. Main loop (idle, WDT feeding if needed)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

**Key Event Callback**:
```c
static void on_key_event(uint8_t key_id, key_state_t state) {
    ESP_LOGI(TAG, "Key K%d %s", key_id, 
             state == KEY_STATE_PRESSED ? "PRESSED" : "RELEASED");
    
    // Send to CAN bus
    can_handler_send_key_event(key_id, state);
}
```

**Mode Detection** (future, Phase 3):
```c
// Check if CAN bus is active
// If no CAN traffic after 5s → switch to USB HID mode
// For Phase 1: CAN-only mode
```

---

## Data Flow Diagrams

### Key Press Event Flow

```
[User presses K5]
    ↓
matrix_scanner.c: GPIO scan detects Row0/Col4 = LOW
    ↓
matrix_scanner.c: Start debounce timer (20ms)
    ↓
[20ms passes, state still pressed]
    ↓
matrix_scanner.c: Update stable state → trigger callback
    ↓
main.c: on_key_event(key_id=5, state=PRESSED)
    ↓
can_handler.c: can_handler_send_key_event(5, PRESSED)
    ↓
can_handler.c: Build CAN message (ID=0x435, data=[5, 1, timestamp_lo, timestamp_hi])
    ↓
can_driver: Send to CAN bus
    ↓
[Main controller receives message]
```

### LED Command Flow

```
[Main controller decides K5 LED = Green]
    ↓
Main controller: Send CAN message (ID=0x430, data=[5, 1, 0, 255, 0])
    ↓
can_driver: Receive message in RX task
    ↓
can_handler.c: Decode message → LED Set Single (key=5, on=true, rgb={0,255,0})
    ↓
can_handler.c: Call led_controller_set_key(5, true, {0,255,0})
    ↓
led_controller.c: Update internal state, mark needs_update=true
    ↓
led_controller.c: Call led_controller_update() immediately
    ↓
led_controller.c: Write all 15 LED states to RMT → SK6812 LEDs
    ↓
[K5 LED lights up green]
```

---

## Task and Memory Model

### FreeRTOS Tasks

| Task | Priority | Stack | Purpose |
|------|----------|-------|---------|
| `matrix_scan_task` | 2 | 2048 | Matrix scanning at 200Hz |
| `can_rx_task` | 3 | 3072 | CAN message reception |
| `can_tx_task` | 2 | 2048 | CAN message transmission (optional) |
| IDLE task | 0 | - | ESP-IDF default |

**Priority Rationale**:
- CAN RX highest: Must respond to LED commands quickly
- Matrix scanner moderate: 5ms latency acceptable
- CAN TX moderate: Key events not ultra-critical (human reaction time ~100ms)

### Memory Budget

**Flash**:
- Firmware code: ~250KB (estimated)
- No filesystem, no OTA partition
- Target: <512KB total

**RAM**:
- Static data: ~5KB (LED buffers, CAN queues)
- Tasks: 3 tasks × 2.5KB avg = ~7.5KB
- Heap: 32KB reserved for ESP-IDF + drivers
- Target: <50KB total

---

## Configuration and Constants

All configuration in `include/config.h`:

```c
// Timing
#define MATRIX_SCAN_RATE_MS     5       // 200Hz scan rate
#define KEY_DEBOUNCE_MS         20      // Debounce time
#define CAN_INIT_DELAY_MS       100     // Wait before announce

// Hardware
#define NUM_KEYS                15
#define NUM_LEDS                15
#define MATRIX_ROWS             3
#define MATRIX_COLS             7

// GPIO pins (defined in config.h)
#define GPIO_ROW0               6
#define GPIO_ROW1               7
#define GPIO_ROW2               8
#define GPIO_COL0               9
// ... etc

// CAN IDs (from can_protocol_keypad)
#define CAN_ID_KEY_EVENT_BASE   0x430   // 0x430 + key_id
#define CAN_ID_LED_SET          0x430
#define CAN_ID_LED_BULK         0x440

// LED defaults
#define LED_BRIGHTNESS_DEFAULT  128     // 50%
#define LED_COLOR_DEFAULT       {255, 255, 255}  // White
```

---

## Error Handling Strategy

### Module Initialization
- Each `*_init()` function returns `esp_err_t`
- On error: Log error, return error code, halt system
- No partial initialization allowed

### Runtime Errors
- **CAN bus error**: Log warning, attempt reconnection
- **RMT write failure**: Log error, retry once
- **Matrix scan error**: Log error, continue (non-fatal)

### Logging Levels
- `ESP_LOGI`: Normal operation (key events, LED updates)
- `ESP_LOGW`: Recoverable errors (CAN timeout)
- `ESP_LOGE`: Critical errors (init failure)
- `ESP_LOGD`: Debug only (matrix scan details) - disabled in release

---

## Testing Strategy

### Unit Testing (ESP-IDF `unity` framework)

**Test: matrix_scanner**
- Mock GPIO reads
- Verify key_id mapping (row/col → key_id)
- Test debounce logic (rapid press/release)
- Verify callback triggering

**Test: led_controller**
- Set individual LEDs, verify state
- Set bulk LEDs, verify bitmask handling
- Verify RMT data encoding (manual inspection)

**Test: can_handler**
- Mock CAN driver
- Verify message formatting (key events)
- Verify message parsing (LED commands)
- Test discovery protocol

### Integration Testing

**Test: Key Press → CAN Message**
1. Ground Col4 GPIO (simulate K5 press)
2. Monitor CAN bus with logic analyzer
3. Verify CAN message (ID=0x435, data=[5, 1, ...])

**Test: CAN Message → LED ON**
1. Send CAN message (ID=0x430, data=[5, 1, 0, 255, 0])
2. Verify K5 LED lights green
3. Measure latency (<10ms)

**Test: Full Loop**
1. Physical keypad connected to main controller via CAN
2. Press K1 → Main controller receives event
3. Main controller sends LED command
4. Verify K1 LED updates

---

## Build and Deployment

### PlatformIO Commands
```bash
cd ots-fw-keypad

# Build
pio run -e m5stack-stamps3-espidf

# Flash
pio run -e m5stack-stamps3-espidf -t upload

# Monitor
pio device monitor

# Clean build
pio run -t clean
```

### Expected Output
```
I (123) KEYPAD: OTS Keypad v0.1.0-dev
I (124) KEYPAD: Hardware: M5Stack Stamp S3 (ESP32-S3)
I (125) KEYPAD: Configuration:
I (126) KEYPAD:   Matrix: 3x7 (15 keys)
I (127) KEYPAD:   Row GPIOs: 6, 7, 8
I (128) KEYPAD:   RGB Data GPIO: 3 (15 LEDs)
I (129) KEYPAD:   CAN TX/RX: GPIO 5/4
I (234) MATRIX: Scanner started (200Hz)
I (235) CAN: Handler started (500kbit/s)
I (335) CAN: Module announce sent
I (336) KEYPAD: Keypad module ready
```

---

## Implementation Phases

### Phase 1: Matrix Scanner (Week 1)
- [ ] Implement `matrix_scanner.c` skeleton
- [ ] Configure GPIO pins (rows=output, cols=input+pullup)
- [ ] Implement single matrix scan function
- [ ] Create FreeRTOS scan task (200Hz loop)
- [ ] Test: Log raw matrix state every scan
- [ ] Implement debounce logic
- [ ] Implement key_id mapping (row/col → 1-15)
- [ ] Test: Press keys, verify callback triggers

### Phase 2: LED Controller (Week 1)
- [ ] Implement `led_controller.c` skeleton
- [ ] Configure RMT peripheral for SK6812
- [ ] Implement state array (15 LEDs)
- [ ] Implement `set_key()`, `set_all()` functions
- [ ] Implement RMT write function
- [ ] Test: Set K1 red, K2 green, K3 blue
- [ ] Test: Set all white, then all off
- [ ] Test: Measure LED update latency

### Phase 3: CAN Handler (Week 2)
- [ ] Create `can_protocol_keypad` shared component first
- [ ] Implement `can_handler.c` skeleton
- [ ] Initialize `can_driver` component
- [ ] Implement key event sending
- [ ] Test: Press key, verify CAN message on bus
- [ ] Implement LED command receiving
- [ ] Test: Send CAN message, verify LED updates
- [ ] Implement module discovery
- [ ] Test: Main controller detects keypad module

### Phase 4: Integration (Week 2)
- [ ] Connect all modules in `main.c`
- [ ] Test full loop with main controller
- [ ] Verify LED latency (<10ms)
- [ ] Stress test: Rapid key presses (all 15 keys)
- [ ] Power consumption measurement
- [ ] Update documentation

---

## Dependencies

### Shared Components (ots-fw-shared)
- `can_driver` - Generic CAN bus driver (existing)
- `can_protocol_discovery` - Module discovery (existing)
- `can_protocol_keypad` - Keypad CAN messages (TO CREATE)

### ESP-IDF Components
- `driver/gpio` - GPIO control
- `driver/rmt` - RMT for SK6812 LEDs
- `led_strip` - SK6812 encoding helper
- `freertos` - Tasks, queues
- `nvs_flash` - NVS init (not used yet, but initialized)
- `esp_log` - Logging

---

## Next Steps

1. **Create shared component**: `can_protocol_keypad` in `ots-fw-shared/`
2. **Implement Phase 1**: Matrix scanner with GPIO and debouncing
3. **Test matrix scanner**: Verify key detection before moving to LEDs
4. **Implement Phase 2**: LED controller with RMT
5. **Implement Phase 3**: CAN handler integration
6. **Test end-to-end**: Full keypad + main controller integration

---

## Related Documentation

- **Main Controller Plan**: `MAIN_CONTROLLER_UPDATE_PLAN.md`
- **Project Prompt**: `PROJECT_PROMPT.md`
- **Hardware Config**: `include/config.h`
- **PCB Documentation**: `../ots-hardware/pcbs/keypad.md`
- **Module Spec**: `../ots-hardware/modules/keypad-module.md`
- **CAN Bus Protocol**: `/prompts/CANBUS_MESSAGE_SPEC.md`
