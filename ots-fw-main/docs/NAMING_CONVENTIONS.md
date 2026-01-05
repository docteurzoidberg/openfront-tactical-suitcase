# OTS Firmware - Naming Conventions & Code Standards

## Project Overview

**Project Name**: OpenFront Tactical Suitcase (OTS)  
**Firmware Name**: `ots-fw-main`  
**Version**: 0.1.0  
**Framework**: ESP-IDF for ESP32-S3

## Naming Conventions

### 1. File Names
- **Source files**: `snake_case.c` (e.g., `nuke_module.c`, `ws_server.c`)
- **Header files**: `snake_case.h` (e.g., `nuke_module.h`, `ws_server.h`)
- **Module pattern**: `<module_name>_module.{c,h}` for hardware modules
- **Driver pattern**: `<device>_driver.{c,h}` for I2C device drivers

### 2. Logging Tags (TAG)
**Convention**: `OTS_<COMPONENT>` in UPPERCASE

All logging tags use the `OTS_` prefix for easy filtering in serial output.

**Examples**:
```c
static const char *TAG = "OTS_MAIN";         // Main application
static const char *TAG = "OTS_NETWORK";      // Network manager
static const char *TAG = "OTS_WS_SERVER";    // WebSocket server
static const char *TAG = "OTS_NUKE";         // Nuke module
static const char *TAG = "OTS_ALERT";        // Alert module
static const char *TAG = "OTS_TROOPS";       // Troops module
static const char *TAG = "OTS_POWER";        // Main power module
static const char *TAG = "OTS_LCD";          // LCD driver
static const char *TAG = "OTS_ADC";          // ADC driver
```

**Complete TAG List**:
- Core: `OTS_MAIN`, `OTS_NETWORK`, `OTS_WS_SERVER`, `OTS_WS_PROTO`, `OTS_OTA`
- System: `OTS_GAME_STATE`, `OTS_EVENTS`, `OTS_MOD_MGR`, `OTS_IO_EXP`, `OTS_MODULE_IO`
- Control: `OTS_LED_CTRL`, `OTS_BUTTONS`, `OTS_IO_TASK`, `OTS_NUKE_TRK`
- Modules: `OTS_NUKE`, `OTS_ALERT`, `OTS_POWER`, `OTS_TROOPS`
- Drivers: `OTS_LCD`, `OTS_ADC`

### 3. Constants & Macros
- **All caps with underscores**: `UPPER_SNAKE_CASE`
- **Project constants**: Prefix with `OTS_` (e.g., `OTS_FIRMWARE_VERSION`)
- **Module-specific**: Prefix with module name (e.g., `NUKE_BTN_ATOM_PIN`, `TROOPS_LCD_ADDR`)
- **I/O board aliases**: `IO_BOARD_INPUT`, `IO_BOARD_OUTPUT`

**Examples**:
```c
#define OTS_PROJECT_NAME        "OpenFront Tactical Suitcase"
#define OTS_FIRMWARE_NAME       "ots-fw-main"
#define OTS_FIRMWARE_VERSION    "0.1.0"

#define WIFI_SSID               "IOT"
#define WS_SERVER_URL           "ws://192.168.77.121:3000/ws"

#define NUKE_BTN_ATOM_PIN       1
#define ALERT_LED_WARNING_BOARD IO_BOARD_OUTPUT
#define TROOPS_SLIDER_POLL_MS   100
```

### 4. Functions
- **Public API**: `<module>_<action>()` in snake_case (e.g., `nuke_module_init()`)
- **Static/private**: `static` keyword with descriptive names (e.g., `update_nuke_button_led_state()`)
- **Hardware module interface**: `<module>_module_get()` returns `hardware_module_t*`

**Examples**:
```c
// Public API
esp_err_t nuke_state_manager_init(void);
void led_handler_alert_on(uint8_t led_index, uint32_t duration_ms);
const hardware_module_t* troops_module_get(void);

// Static helpers
static void update_nuke_button_led_state(uint8_t led_index, nuke_type_t nuke_type);
static bool handle_event(const internal_event_t *event);
```

### 5. Types
- **Structs**: `snake_case_t` suffix (e.g., `module_status_t`, `internal_event_t`)
- **Enums**: `UPPER_SNAKE_CASE` for enum values, `snake_case_t` for type name
- **Module state**: `<module>_module_state_t` pattern

**Examples**:
```c
typedef struct {
    bool initialized;
    bool operational;
    uint32_t error_count;
    char last_error[64];
} module_status_t;

typedef enum {
    GAME_EVENT_NUKE_LAUNCHED,
    GAME_EVENT_HYDRO_LAUNCHED,
    GAME_EVENT_MIRV_LAUNCHED
} game_event_type_t;

typedef struct {
    uint32_t current_troops;
    uint32_t max_troops;
    uint8_t slider_percent;
    bool initialized;
} troops_module_state_t;
```

### 6. Variables
- **Local variables**: `snake_case` (e.g., `unit_id`, `led_index`)
- **Static module state**: `static <module>_module_state_t module_state`
- **Global constants**: `const` with descriptive names

**Examples**:
```c
static module_status_t status = {0};
static troops_module_state_t module_state = {0};

uint32_t unit_id = ots_parse_unit_id(event->data);
bool led_state = module_io_get_nuke_led(0);
```

## Code Organization

### Directory Structure
```
ots-fw-main/
  include/               # Public headers
    config.h             # Project configuration
    ots_common.h         # Common utilities & helpers
    hardware_module.h    # Module interface
    *_module.h           # Module-specific headers
    *_driver.h           # Driver headers
  src/                   # Implementation
    main.c               # Entry point
    *_module.c           # Hardware modules
    *_driver.c           # I2C device drivers
    *_manager.c          # System managers
```

### Module Pattern
Every hardware module follows the `hardware_module_t` interface:
```c
typedef struct hardware_module {
    const char *name;
    bool enabled;
    esp_err_t (*init)(void);
    esp_err_t (*update)(void);
    bool (*handle_event)(const internal_event_t *event);
    void (*get_status)(module_status_t *status);
    esp_err_t (*shutdown)(void);
} hardware_module_t;
```

**Implementation pattern**:
```c
// Module interface (exposed via getter)
static const hardware_module_t <module>_module = {
    .name = "<Module Name>",
    .init = <module>_module_init,
    .update = <module>_module_update,
    .handle_event = <module>_module_handle_event,
    .get_status = <module>_module_get_status,
    .shutdown = <module>_module_shutdown
};

const hardware_module_t* <module>_module_get(void) {
    return &<module>_module;
}
```

## Common Utilities (`ots_common.h`)

### JSON Parsing Helpers
To reduce code duplication, use shared JSON parsing functions:

```c
// Parse unit ID from JSON (common in nuke/alert modules)
uint32_t ots_parse_unit_id(const char *data_json);

// Parse JSON values with defaults
int32_t ots_json_get_int(cJSON *root, const char *key, int32_t default_value);
const char* ots_json_get_string(cJSON *root, const char *key);
bool ots_json_get_bool(cJSON *root, const char *key, bool default_value);
```

**Usage example**:
```c
#include "ots_common.h"

// Old way (duplicated in each module):
cJSON *root = cJSON_Parse(event->data);
cJSON *id = cJSON_GetObjectItem(root, "nukeUnitID");
uint32_t unit_id = id ? (uint32_t)id->valueint : 0;
cJSON_Delete(root);

// New way (shared helper):
uint32_t unit_id = ots_parse_unit_id(event->data);
```

## Refactoring Improvements

### 1. Eliminated Duplicate Code
- **Before**: 2 identical `parse_unit_id_from_data()` functions (nuke_module.c, alert_module.c)
- **After**: Single `ots_parse_unit_id()` in `ots_common.h`
- **Lines saved**: ~40 lines of duplicate code

### 2. Standardized Logging Tags
- **Before**: Inconsistent tags (`lcd_driver`, `adc_driver`, `NUKE_MOD`, `WS_TRANSPORT`)
- **After**: Consistent `OTS_<COMPONENT>` pattern for all 20 modules/components
- **Benefit**: Easier log filtering with `monitor_filters = OTS_*` in PlatformIO

### 3. Centralized Project Constants
- **Location**: `config.h` with `OTS_PROJECT_NAME`, `OTS_FIRMWARE_VERSION`, etc.
- **Usage**: `OTA_HOSTNAME` now uses `OTS_FIRMWARE_NAME` constant
- **Benefit**: Single source of truth for project metadata

### 4. Modular Driver Architecture
- **LCD Driver**: Shared `lcd_driver.{c,h}` for HD44780 LCDs
- **ADC Driver**: Shared `adc_driver.{c,h}` for ADS1015 ADC
- **Benefit**: Other modules can reuse these drivers without duplication

## Best Practices

### 1. Consistent TAG Usage
```c
// ✅ Good
static const char *TAG = "OTS_MY_MODULE";
ESP_LOGI(TAG, "Module initialized");
ESP_LOGE(TAG, "Failed to initialize: %s", esp_err_to_name(ret));

// ❌ Bad
printf("Module initialized\n");  // No TAG, no log level
```

### 2. Use Common Helpers
```c
// ✅ Good
#include "ots_common.h"
uint32_t unit_id = ots_parse_unit_id(event->data);

// ❌ Bad (duplicate parsing code)
cJSON *root = cJSON_Parse(event->data);
// ... 10 lines of JSON parsing ...
```

### 3. Module State Encapsulation
```c
// ✅ Good
static troops_module_state_t module_state = {0};  // Private to module

// ❌ Bad
troops_module_state_t troops_state;  // Global, exposed
```

### 4. Const Correctness
```c
// ✅ Good
static const char *TAG = "OTS_MODULE";
static const button_mapping_t button_map[] = { ... };

// ❌ Bad
static char *TAG = "OTS_MODULE";  // Modifiable string pointer
```

## Configuration Management

### Project Configuration (`config.h`)
- WiFi credentials
- WebSocket server URL
- I2C pin assignments
- Task priorities
- OTA settings

### Module Configuration (module headers)
- Pin mappings (`NUKE_BTN_ATOM_PIN`, etc.)
- I2C addresses (`TROOPS_LCD_ADDR`, `TROOPS_ADS1015_ADDR`)
- Timing constants (`TROOPS_SLIDER_POLL_MS`, `NUKE_BLINK_DURATION_MS`)
- Module-specific thresholds

## Version History

### Version 0.1.0 (December 19, 2025)
- **Naming Conventions**: Standardized all logging TAGs with `OTS_` prefix
- **Code Refactoring**: Extracted common JSON parsing to `ots_common.h`
- **Driver Abstraction**: Created shared LCD and ADC drivers
- **Project Constants**: Added centralized project identification constants

## References

- **Main Context**: [ots-fw-main/copilot-project-context.md](copilot-project-context.md)
- **Protocol Spec**: [/WEBSOCKET_MESSAGE_SPEC.md](../WEBSOCKET_MESSAGE_SPEC.md)
- **Hardware Modules**: [ots-hardware/](../ots-hardware/)
