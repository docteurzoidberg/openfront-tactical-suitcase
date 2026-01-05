# System Status Module - Project Prompt

## Overview

The **System Status Module** is responsible for displaying game state and system information on a 16x2 character LCD. It acts as the primary visual interface for players, showing everything from boot messages to victory/defeat screens.

## Purpose

The System Status Module serves as:
- **Game state visualizer**: Shows current game phase (lobby, playing, ended)
- **Connection status indicator**: Displays network and WebSocket status
- **Information display**: Shows troop counts, game results, and system messages
- **User feedback system**: Provides visual confirmation of system state changes

## Hardware Components

### LCD Display
- **Model**: 16x2 I2C LCD (HD44780 via PCF8574 backpack)
- **I2C Address**: 0x27 (default) or 0x3F (alternative)
- **Connection**: Shared I2C bus (GPIO8 SDA, GPIO9 SCL)
- **Driver**: `lcd_driver.h` / `lcd_driver.c` (shared component)

## Display Screens

The module implements **7 distinct screen types** defined in `/ots-hardware/DISPLAY_SCREENS_SPEC.md`:

### 1. Splash Screen (Boot)
```
 OPENFRONT       
 TACTICAL SUITE  
```
- Shown during firmware boot
- Duration: 2 seconds
- Transitions to: Waiting screen

### 2. Waiting for Connection
```
 WAITING FOR     
 CONNECTION...   
```
- Shown when network connected but no userscript
- Updates on WiFi connect/disconnect
- Transitions to: Lobby screen (on userscript connect)

### 3. Lobby Screen
```
 LOBBY           
 Waiting...      
```
- Shown when userscript connected but game not started
- Indicates ready to play
- Transitions to: Troops screen (on GAME_START event)

### 4. Troops Screen (Active Gameplay)
```
120K / 1.1M      
50% (60K)        
```
- Line 1: Current troops / Max troops (right-aligned)
- Line 2: Deployment % and calculated amount (left-aligned)
- Updates on troop state changes from server
- Unit scaling: K (thousands), M (millions), B (billions)
- Transitions to: Victory/Defeat screen (on game end)

### 5. Victory Screen
```
   VICTORY!      
  You won!       
```
- Shown when player or player's team wins
- Center-aligned text
- Triggered by: GAME_END event with victory=true
- Duration: Until next game starts or connection lost

### 6. Defeat Screen
```
   DEFEAT        
  You lost       
```
- Shown when player dies or loses
- Center-aligned text
- Triggered by: GAME_END event with victory=false
- Duration: Until next game starts or connection lost

### 7. Shutdown Screen (optional)
```
 SHUTTING DOWN   
 Goodbye!        
```
- Shown on graceful shutdown (if implemented)
- Duration: Until power off

## Screen Transitions

```
Boot → Splash (2s)
     ↓
Splash → Waiting (WiFi connected, no userscript)
       ↓
Waiting → Lobby (userscript connects)
        ↓
Lobby → Troops (GAME_START event)
      ↓
Troops → Victory/Defeat (GAME_END event)
       ↓
Victory/Defeat → Lobby (new game starts)
               ↓
               Waiting (userscript disconnects)
```

## Event-to-Screen Mapping

### Network Events (Internal)
- `INTERNAL_EVENT_NETWORK_CONNECTED` → Waiting screen
- `INTERNAL_EVENT_NETWORK_DISCONNECTED` → Clear LCD or show error
- `INTERNAL_EVENT_WS_CONNECTED` → Lobby screen (if userscript)
- `INTERNAL_EVENT_WS_DISCONNECTED` → Waiting screen

### Game Events (Protocol)
- `GAME_EVENT_GAME_START` → Troops screen
- `GAME_EVENT_GAME_END` → Victory or Defeat screen (based on data)
- `GAME_EVENT_WIN` → Victory screen
- `GAME_EVENT_LOOSE` → Defeat screen
- Troop updates → Update Troops screen (if active)

## Module State Structure

```c
typedef enum {
    SCREEN_SPLASH,
    SCREEN_WAITING,
    SCREEN_LOBBY,
    SCREEN_TROOPS,
    SCREEN_VICTORY,
    SCREEN_DEFEAT,
    SCREEN_SHUTDOWN
} screen_type_t;

typedef struct {
    screen_type_t current_screen;
    screen_type_t previous_screen;
    
    // Troops screen data
    uint32_t current_troops;
    uint32_t max_troops;
    uint8_t deployment_percent;
    
    // Connection state
    bool network_connected;
    bool userscript_connected;
    
    // Screen update control
    bool screen_dirty;
    uint64_t last_update_ms;
    
    // Initialization
    bool initialized;
} system_status_state_t;
```

## Implementation Tasks

### 1. Module Initialization

Implements `hardware_module_t` interface:

```c
static esp_err_t system_status_init(void) {
    ESP_LOGI(TAG, "Initializing system status module...");
    
    // LCD already initialized by lcd_driver
    // Just verify it's working
    if (!lcd_is_initialized()) {
        ESP_LOGE(TAG, "LCD not initialized!");
        return ESP_FAIL;
    }
    
    // Show splash screen
    show_screen(SCREEN_SPLASH);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Transition to waiting screen
    show_screen(SCREEN_WAITING);
    
    module_state.initialized = true;
    return ESP_OK;
}
```

### 2. Event Handler

```c
static void system_status_handle_event(game_event_type_t event_type, const char* event_data_json) {
    if (!module_state.initialized) return;
    
    switch (event_type) {
        case INTERNAL_EVENT_NETWORK_CONNECTED:
            module_state.network_connected = true;
            if (!module_state.userscript_connected) {
                show_screen(SCREEN_WAITING);
            }
            break;
            
        case INTERNAL_EVENT_NETWORK_DISCONNECTED:
            module_state.network_connected = false;
            module_state.userscript_connected = false;
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_write_string("NO CONNECTION");
            break;
            
        case INTERNAL_EVENT_WS_CONNECTED:
            // Check if userscript connected (parse event_data_json)
            if (is_userscript_client(event_data_json)) {
                module_state.userscript_connected = true;
                show_screen(SCREEN_LOBBY);
            }
            break;
            
        case INTERNAL_EVENT_WS_DISCONNECTED:
            // Check if userscript disconnected
            if (was_userscript_client(event_data_json)) {
                module_state.userscript_connected = false;
                show_screen(SCREEN_WAITING);
            }
            break;
            
        case GAME_EVENT_GAME_START:
            show_screen(SCREEN_TROOPS);
            break;
            
        case GAME_EVENT_GAME_END:
            // Parse victory/defeat from event_data_json
            if (is_victory(event_data_json)) {
                show_screen(SCREEN_VICTORY);
            } else {
                show_screen(SCREEN_DEFEAT);
            }
            break;
            
        case GAME_EVENT_WIN:
            show_screen(SCREEN_VICTORY);
            break;
            
        case GAME_EVENT_LOOSE:
            show_screen(SCREEN_DEFEAT);
            break;
            
        default:
            // Check for troop updates in event_data_json
            if (event_data_json && module_state.current_screen == SCREEN_TROOPS) {
                parse_troop_data(event_data_json);
                module_state.screen_dirty = true;
            }
            break;
    }
}
```

### 3. Screen Rendering

```c
static void show_screen(screen_type_t screen) {
    if (screen == module_state.current_screen) return;
    
    module_state.previous_screen = module_state.current_screen;
    module_state.current_screen = screen;
    
    lcd_clear();
    
    switch (screen) {
        case SCREEN_SPLASH:
            lcd_set_cursor(0, 0);
            lcd_write_string(" OPENFRONT      ");
            lcd_set_cursor(0, 1);
            lcd_write_string(" TACTICAL SUITE ");
            break;
            
        case SCREEN_WAITING:
            lcd_set_cursor(0, 0);
            lcd_write_string(" WAITING FOR    ");
            lcd_set_cursor(0, 1);
            lcd_write_string(" CONNECTION...  ");
            break;
            
        case SCREEN_LOBBY:
            lcd_set_cursor(0, 0);
            lcd_write_string(" LOBBY          ");
            lcd_set_cursor(0, 1);
            lcd_write_string(" Waiting...     ");
            break;
            
        case SCREEN_TROOPS:
            update_troops_display();
            break;
            
        case SCREEN_VICTORY:
            lcd_set_cursor(0, 0);
            lcd_write_string("   VICTORY!     ");
            lcd_set_cursor(0, 1);
            lcd_write_string("  You won!      ");
            break;
            
        case SCREEN_DEFEAT:
            lcd_set_cursor(0, 0);
            lcd_write_string("   DEFEAT       ");
            lcd_set_cursor(0, 1);
            lcd_write_string("  You lost      ");
            break;
            
        case SCREEN_SHUTDOWN:
            lcd_set_cursor(0, 0);
            lcd_write_string(" SHUTTING DOWN  ");
            lcd_set_cursor(0, 1);
            lcd_write_string(" Goodbye!       ");
            break;
    }
    
    ESP_LOGI(TAG, "Screen changed: %s -> %s", 
             screen_name(module_state.previous_screen),
             screen_name(module_state.current_screen));
}
```

### 4. Troops Display

```c
static void update_troops_display(void) {
    char line1[LCD_COLS + 1];
    char line2[LCD_COLS + 1];
    char current_str[8], max_str[8], calc_str[8];
    
    // Format troop counts with K/M/B scaling
    format_troops(module_state.current_troops, current_str, sizeof(current_str));
    format_troops(module_state.max_troops, max_str, sizeof(max_str));
    
    // Line 1: "120K / 1.1M" (right-aligned)
    snprintf(line1, sizeof(line1), "%s / %s", current_str, max_str);
    int padding = LCD_COLS - strlen(line1);
    if (padding > 0) {
        memmove(line1 + padding, line1, strlen(line1) + 1);
        memset(line1, ' ', padding);
    }
    
    // Line 2: "50% (60K)" (left-aligned)
    uint32_t calculated = ((uint64_t)module_state.current_troops * 
                          module_state.deployment_percent) / 100;
    format_troops(calculated, calc_str, sizeof(calc_str));
    snprintf(line2, sizeof(line2), "%d%% (%s)", 
             module_state.deployment_percent, calc_str);
    
    // Write to LCD
    lcd_set_cursor(0, 0);
    lcd_write_string(line1);
    lcd_set_cursor(0, 1);
    lcd_write_string(line2);
    
    module_state.screen_dirty = false;
}

static void format_troops(uint32_t troops, char* buffer, size_t buffer_size) {
    if (troops >= 1000000000) {
        snprintf(buffer, buffer_size, "%.1fB", troops / 1000000000.0);
    } else if (troops >= 1000000) {
        snprintf(buffer, buffer_size, "%.1fM", troops / 1000000.0);
    } else if (troops >= 1000) {
        snprintf(buffer, buffer_size, "%.1fK", troops / 1000.0);
    } else {
        snprintf(buffer, buffer_size, "%lu", (unsigned long)troops);
    }
}
```

### 5. Module Update

```c
static void system_status_update(void) {
    if (!module_state.initialized) return;
    
    // Update troops display if dirty and on troops screen
    if (module_state.screen_dirty && 
        module_state.current_screen == SCREEN_TROOPS) {
        update_troops_display();
    }
}
```

### 6. Parsing Helper Functions

```c
static bool is_userscript_client(const char* json_data) {
    if (!json_data) return false;
    cJSON* root = cJSON_Parse(json_data);
    if (!root) return false;
    
    cJSON* client_type = cJSON_GetObjectItem(root, "clientType");
    bool result = client_type && 
                  cJSON_IsString(client_type) && 
                  strcmp(client_type->valuestring, "userscript") == 0;
    
    cJSON_Delete(root);
    return result;
}

static bool is_victory(const char* json_data) {
    if (!json_data) return false;
    cJSON* root = cJSON_Parse(json_data);
    if (!root) return false;
    
    cJSON* victory = cJSON_GetObjectItem(root, "victory");
    bool result = victory && cJSON_IsTrue(victory);
    
    cJSON_Delete(root);
    return result;
}

static void parse_troop_data(const char* json_data) {
    cJSON* root = cJSON_Parse(json_data);
    if (!root) return;
    
    cJSON* troops = cJSON_GetObjectItem(root, "troops");
    if (troops) {
        cJSON* current = cJSON_GetObjectItem(troops, "current");
        cJSON* max = cJSON_GetObjectItem(troops, "max");
        
        if (current && cJSON_IsNumber(current)) {
            module_state.current_troops = (uint32_t)current->valueint;
        }
        if (max && cJSON_IsNumber(max)) {
            module_state.max_troops = (uint32_t)max->valueint;
        }
        
        module_state.screen_dirty = true;
    }
    
    cJSON_Delete(root);
}
```

## Protocol Integration

### State Messages (from server)
```json
{
  "type": "state",
  "payload": {
    "timestamp": 1234567890,
    "troops": {
      "current": 120000,
      "max": 1100000
    }
  }
}
```

### Game Events
```json
{
  "type": "event",
  "payload": {
    "type": "GAME_START",
    "timestamp": 1234567890,
    "message": "Game started!"
  }
}
```

```json
{
  "type": "event",
  "payload": {
    "type": "GAME_END",
    "timestamp": 1234567890,
    "message": "Game ended",
    "data": {
      "victory": true
    }
  }
}
```

## Testing Checklist

### Screen Transitions
- [ ] Boot shows splash screen for 2 seconds
- [ ] Splash → Waiting transition works
- [ ] Waiting → Lobby on userscript connect
- [ ] Lobby → Troops on GAME_START
- [ ] Troops → Victory/Defeat on game end
- [ ] Victory/Defeat → Lobby on new game

### Display Content
- [ ] All text properly centered/aligned
- [ ] No text overflow (16 chars max)
- [ ] Troops display formats correctly (K/M/B)
- [ ] Line 1 right-aligned, Line 2 left-aligned
- [ ] Victory/Defeat screens show correct text

### Event Handling
- [ ] Network connect/disconnect updates screen
- [ ] Userscript connect/disconnect updates screen
- [ ] Game start transitions to troops display
- [ ] Game end shows correct victory/defeat
- [ ] Troop updates refresh troops screen

### Edge Cases
- [ ] Screen doesn't flicker on rapid updates
- [ ] Handle missing JSON fields gracefully
- [ ] Handle very large troop numbers (>1B)
- [ ] Handle zero troops
- [ ] Handle connection loss during gameplay

## Configuration

### Timing Constants
```c
#define SPLASH_DURATION_MS 2000
#define SCREEN_UPDATE_INTERVAL_MS 100
```

### LCD Settings
```c
#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_I2C_ADDR 0x27  // or 0x3F
```

## Key Considerations

1. **Screen state persistence**: Remember current screen across event handling
2. **Dirty flag**: Only update LCD when data changes (avoid flickering)
3. **Event ordering**: Network events before game events
4. **JSON parsing**: Always check for NULL and validate data types
5. **Text alignment**: Use proper padding and alignment for readability
6. **Unit scaling**: Consistent decimal precision (1 decimal place for K/M/B)
7. **Error handling**: Graceful degradation if LCD fails

## Integration with Other Modules

- **Troops Module**: Shares troop data, but Troops Module controls slider
- **Network Manager**: Receives connection status events
- **WebSocket Server**: Receives userscript connection events
- **Event Dispatcher**: All events routed through central dispatcher

## Debug Output

```
[SysStatus] Module initialized
[SysStatus] Showing splash screen
[SysStatus] Screen changed: SPLASH -> WAITING
[SysStatus] Userscript connected
[SysStatus] Screen changed: WAITING -> LOBBY
[SysStatus] Game started
[SysStatus] Screen changed: LOBBY -> TROOPS
[SysStatus] Troops updated: 120000 / 1100000
[SysStatus] Game ended: VICTORY
[SysStatus] Screen changed: TROOPS -> VICTORY
```

## References

- Display specification: `/ots-hardware/DISPLAY_SCREENS_SPEC.md`
- LCD driver: `include/lcd_driver.h`, `src/lcd_driver.c`
- Protocol: `/prompts/WEBSOCKET_MESSAGE_SPEC.md`
- Game state: `include/game_state.h`
- Similar module: `TROOPS_MODULE_PROMPT.md` (shares troops display logic)
