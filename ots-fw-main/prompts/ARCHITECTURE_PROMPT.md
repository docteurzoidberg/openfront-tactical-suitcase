# OTS Firmware Architecture Prompt

## Purpose

This prompt provides an overview of the firmware's modular architecture for AI agents working on system-level changes.

## Component Architecture

### Core System Components

```
┌─────────────────────────────────────────────────────────────┐
│                     ESP32-S3 Firmware                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Network    │  │  Event       │  │  Module      │      │
│  │   Manager    │──│  Dispatcher  │──│  Manager     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│         │                  │                  │              │
│         │                  │                  │              │
│  ┌──────▼──────┐  ┌───────▼──────┐  ┌────────▼────────┐   │
│  │  HTTP/WSS   │  │  LED/Button  │  │  Hardware       │   │
│  │  Server     │  │  Controllers │  │  Modules        │   │
│  └─────────────┘  └──────────────┘  └─────────────────┘   │
│                                              │               │
│                                              │               │
│                                      ┌───────▼────────┐     │
│                                      │  I2C Bus       │     │
│                                      │  (GPIO8/9)     │     │
│                                      └────────────────┘     │
│                                              │               │
└──────────────────────────────────────────────┼──────────────┘
                                               │
                    ┌──────────────────────────┼──────────────────────┐
                    │                          │                      │
             ┌──────▼──────┐          ┌───────▼──────┐       ┌──────▼──────┐
             │ MCP23017    │          │  HD44780     │       │  ADS1015    │
             │ I/O Expander│          │  LCD (0x27)  │       │  ADC (0x48) │
             │ (0x20/0x21) │          └──────────────┘       └─────────────┘
             └─────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
   ┌────▼────┐            ┌────▼────┐
   │ Buttons │            │  LEDs   │
   │ Sensors │            │ Relays  │
   └─────────┘            └─────────┘
```

### Initialization Order

**Critical: Components must be initialized in this order to avoid boot failures:**

1. **NVS Flash** - Non-volatile storage
2. **RGB Status LED** (WS2812) - Early boot status indicator
3. **I2C Bus** - Must be initialized before any I2C devices
4. **WiFi Credentials** - Load from NVS
5. **Serial Commands** - Enable provisioning commands
6. **I/O Expanders** (MCP23017) - Only if hardware present
7. **Event Dispatcher** - Event routing system
8. **Module Manager** - Register and initialize all modules
9. **Hardware Modules**:
   - SystemStatus (LCD display) - Always initialized
   - Troops (LCD + ADC) - Always initialized
   - Nuke, Alert, MainPower - Only if I/O expanders present
10. **Game State Manager** - Track game phases
11. **LED/Button/ADC Controllers** - Only if I/O expanders present
12. **Network Manager** - WiFi connection
13. **HTTP/WebSocket Server** - Communication with clients
14. **OTA Manager** - Remote firmware updates

### Component Responsibilities

#### Network Layer
- **network_manager.c** - WiFi lifecycle, mDNS, reconnection logic
- **http_server.c** (component) - HTTPS server with WSS support
- **ws_handlers.c** - WebSocket connection management
- **ws_protocol.c** - Message parsing/serialization
- **ota_manager.c** - HTTP OTA update server (port 3232)

#### Event System
- **event_dispatcher.c** - Central event routing (FreeRTOS queue)
- **game_state.c** - Game phase tracking (LOBBY, IN_GAME, etc.)
- **protocol.c** - Event type definitions and string conversions

#### Hardware Abstraction
- **i2c_handler.c** - Shared I2C master bus (GPIO8 SDA, GPIO9 SCL)
- **io_expander.c** (component) - MCP23017 driver with retry logic
- **lcd_driver.c** (component) - HD44780 LCD via PCF8574 I2C backpack
- **adc_driver.c** (component) - ADS1015 12-bit ADC driver
- **ws2812_rmt.c** (component) - WS2812 RGB LED via RMT peripheral

#### Hardware Controllers
- **led_handler.c** - LED effect management (blink patterns, timers)
- **button_handler.c** - Button debouncing and event posting
- **adc_handler.c** - Periodic ADC scanning and value change detection
- **rgb_handler.c** - RGB LED status state machine

#### Hardware Modules (Event-Driven)
Each module implements `hardware_module_t` interface:

```c
typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    void (*update)(void);
    void (*handle_event)(game_event_type_t type, const char *data);
    esp_err_t (*get_status)(char *buffer, size_t len);
    void (*shutdown)(void);
} hardware_module_t;
```

**Modules:**
- **system_status_module.c** - LCD display manager (7 screen types)
- **troops_module.c** - Troop count display + slider control
- **nuke_module.c** - Nuke launch buttons + LEDs
- **alert_module.c** - Incoming threat indicators (6 LEDs)
- **main_power_module.c** - LINK LED connection status

## ESP-IDF Components

Reusable drivers extracted to `components/`:

```
components/
  ├── hd44780_pcf8574/        # LCD driver (HD44780 via PCF8574 I2C)
  ├── ads1015_driver/         # 12-bit ADC driver
  ├── mcp23017_driver/        # I/O expander driver
  ├── ws2812_rmt/             # WS2812 RGB LED driver
  └── esp_http_server_core/   # HTTP/HTTPS server abstraction
```

**Component Benefits:**
- Reusable across projects via `idf_component.yml`
- Clean API boundaries (hardware abstraction)
- Independent versioning and testing
- Application logic stays in `src/`

## Critical Design Patterns

### 1. Event-Driven Architecture

All communication between components uses the event dispatcher:

```c
// Post event
internal_event_t event = {
    .type = GAME_EVENT_NUKE_LAUNCHED,
    .timestamp = esp_timer_get_time() / 1000,
    .data = data_json
};
event_dispatcher_post(&event);

// Handle event (in module)
void nuke_handle_event(game_event_type_t type, const char *data) {
    if (type == GAME_EVENT_NUKE_LAUNCHED) {
        // Start LED blink
    }
}
```

### 2. State-Based LED Control

LEDs are controlled by tracking state, NOT timers:

```c
// WRONG: Timer-based
led_blink_for_duration(LED_ATOM, 10000);  // ❌

// CORRECT: State-based with unitID tracking
nuke_tracker_add(unitID, NUKE_TYPE_ATOM, NUKE_DIR_OUTGOING);
// LED ON while nuke_tracker_get_active_count(NUKE_TYPE_ATOM) > 0
// LED OFF when all nukes of that type resolve
```

### 3. Module Isolation

Modules do NOT call each other directly:

```c
// WRONG: Direct call
alert_module_activate(ALERT_TYPE_ATOM);  // ❌

// CORRECT: Event-based
event_dispatcher_post(&alert_event);  // ✅
// Alert module receives event and activates
```

### 4. Component Independence

Components accept bus handles, NOT globals:

```c
// Component API
esp_err_t lcd_init(i2c_master_bus_handle_t bus, uint8_t addr);

// Application passes shared bus
lcd_init(ots_i2c_bus_get(), LCD_I2C_ADDR);
```

## Common Pitfalls

### ❌ Initializing I2C devices before bus is ready
```c
// WRONG: Missing I2C bus init
io_expander_begin(ots_i2c_bus_get(), ...);  // ❌ Bus not initialized!
```

### ✅ Proper initialization
```c
// CORRECT: Init bus first
ots_i2c_bus_init();  // Must be called early
io_expander_begin(ots_i2c_bus_get(), ...);
```

### ❌ Blocking operations in event handlers
```c
void handle_event(...) {
    vTaskDelay(5000);  // ❌ Blocks event queue!
}
```

### ✅ Non-blocking with state tracking
```c
void handle_event(...) {
    state.start_time = millis();  // ✅ Track state
}

void update() {
    if (millis() - state.start_time > 5000) {
        // Handle timeout
    }
}
```

### ❌ HTTP server configuration exceeds limits
```c
.max_open_sockets = 8  // ❌ ESP-IDF limit is 7!
```

### ✅ Respect ESP-IDF limits
```c
.max_open_sockets = 4  // ✅ 7 total - 3 internal = 4 available
```

## Adding New Modules

See `prompts/MODULE_DEVELOPMENT_PROMPT.md` for detailed guide.

**Quick checklist:**
1. Create `module_name.{c,h}` in `src/` and `include/`
2. Implement `hardware_module_t` interface
3. Add to `src/CMakeLists.txt` SRCS list
4. Register in `main.c` via `module_manager_register()`
5. Create `prompts/MODULE_NAME_PROMPT.md`
6. Update this architecture document

## Testing Strategy

- **Unit tests**: Components tested independently
- **Integration tests**: `test-*` PlatformIO environments
- **Hardware tests**: Standalone test firmwares (no menus)
- **Automation**: Python scripts in `tools/tests/`

See `docs/TESTING.md` for complete testing guide.

## Key Files Reference

**Core System:**
- `src/main.c` - Application entry point and initialization
- `src/event_dispatcher.c` - Central event routing
- `src/module_manager.c` - Module lifecycle management
- `include/protocol.h` - Event type definitions

**Network:**
- `src/network_manager.c` - WiFi and mDNS
- `components/esp_http_server_core/` - HTTP/HTTPS server
- `src/ws_handlers.c` - WebSocket connection management

**Hardware:**
- `src/i2c_handler.c` - Shared I2C bus
- `components/*/` - Reusable hardware drivers
- `src/*_module.c` - Hardware module implementations

## Protocol Synchronization

**CRITICAL**: All protocol changes start at `/prompts/protocol-context.md` (repo root).

Changes must be synchronized across:
1. TypeScript: `ots-shared/src/game.ts`
2. C Firmware: `ots-fw-main/include/protocol.h`
3. Server: `ots-simulator/` handlers
4. Userscript: `ots-userscript/` trackers

See `prompts/PROTOCOL_CHANGE_PROMPT.md` for workflow.

## Related Prompts

- **Module Development**: `MODULE_DEVELOPMENT_PROMPT.md`
- **Build & Test**: `BUILD_AND_TEST_PROMPT.md`
- **Protocol Changes**: `PROTOCOL_CHANGE_PROMPT.md`
- **Debugging**: `DEBUGGING_AND_RECOVERY_PROMPT.md`
- **Individual Modules**: `*_MODULE_PROMPT.md` files
