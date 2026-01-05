# Hardware Diagnostic Implementation Notes for ots-fw-main

## Overview

A hardware diagnostic command/response protocol has been added to allow remote querying of device hardware status. The userscript can request diagnostics, and both the simulator (ots-simulator) and firmware (ots-fw-main) respond with detailed hardware information.

## Protocol

### Command: `hardware-diagnostic`
Sent from userscript to request diagnostic information.

```json
{
  "type": "cmd",
  "payload": {
    "action": "hardware-diagnostic",
    "params": {}
  }
}
```

### Response Event: `HARDWARE_DIAGNOSTIC`
Device responds with diagnostic data.

```json
{
  "type": "event",
  "payload": {
    "type": "HARDWARE_DIAGNOSTIC",
    "timestamp": 1735482000000,
    "message": "ESP32-S3 Hardware Diagnostic",
    "data": {
      "version": "2025-12-31.1",
      "deviceType": "firmware",
      "serialNumber": "OTS-FW-ESP001",
      "owner": "DrZoid",
      "hardware": {
        "lcd": { "present": true, "working": true },
        "inputBoard": { "present": true, "working": true },
        "outputBoard": { "present": true, "working": true },
        "adc": { "present": true, "working": true },
        "soundModule": { "present": false, "working": false }
      }
    }
  }
}
```

## Firmware Implementation Tasks

### 1. Add to protocol.h

Add `GAME_EVENT_HARDWARE_DIAGNOSTIC` to the `game_event_type_t` enum:

```c
typedef enum {
    GAME_EVENT_UNKNOWN = 0,
    GAME_EVENT_INFO,
    GAME_EVENT_GAME_START,
    GAME_EVENT_GAME_END,
    GAME_EVENT_SOUND_PLAY,
    GAME_EVENT_HARDWARE_DIAGNOSTIC,  // ADD THIS
    GAME_EVENT_NUKE_LAUNCHED,
    // ... rest of events
} game_event_type_t;
```

Add to string conversion function in `protocol.c`:
```c
const char* game_event_type_to_string(game_event_type_t type) {
    switch (type) {
        // ... existing cases ...
        case GAME_EVENT_HARDWARE_DIAGNOSTIC: return "HARDWARE_DIAGNOSTIC";
        // ... rest of cases ...
    }
}
```

### 2. Add Device Identity to config.h

Add hardcoded device identity (immutable values):

```c
// Device Identity (hardcoded at compile time)
#define OTS_DEVICE_SERIAL_NUMBER "OTS-FW-ESP001"  // Unique per device
#define OTS_DEVICE_OWNER "DrZoid"                 // Device owner name
```

**Note**: These should be unique per device. Consider using:
- Serial format: `OTS-FW-XXXXXX` where XXXXXX is unique (MAC-based, sequential, etc.)
- Owner: The person or team who owns the device

### 3. Handle Command in ws_server.c

When receiving `hardware-diagnostic` command, call diagnostic handler:

```c
// In handle_incoming_cmd() or equivalent
if (strcmp(action, "hardware-diagnostic") == 0) {
    send_hardware_diagnostic();
    return;
}
```

### 4. Implement send_hardware_diagnostic() Function

Create function to gather hardware status and send response:

```c
void send_hardware_diagnostic(void) {
    ESP_LOGI(TAG, "Hardware diagnostic requested");
    
    // Check hardware component status
    bool lcd_present = lcd_is_present();
    bool lcd_working = lcd_is_working();
    bool input_board_present = io_expander_is_board_present(0);
    bool input_board_working = io_expander_is_board_working(0);
    bool output_board_present = io_expander_is_board_present(1);
    bool output_board_working = io_expander_is_board_working(1);
    bool adc_present = adc_is_present();
    bool adc_working = adc_is_working();
    bool sound_present = false;  // Not yet implemented
    bool sound_working = false;  // Not yet implemented
    
    // Build JSON response
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "event");
    
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "type", "HARDWARE_DIAGNOSTIC");
    cJSON_AddNumberToObject(payload, "timestamp", esp_timer_get_time() / 1000);
    cJSON_AddStringToObject(payload, "message", "ESP32-S3 Hardware Diagnostic");
    
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "version", OTS_FIRMWARE_VERSION);
    cJSON_AddStringToObject(data, "deviceType", "firmware");
    cJSON_AddStringToObject(data, "serialNumber", OTS_DEVICE_SERIAL_NUMBER);
    cJSON_AddStringToObject(data, "owner", OTS_DEVICE_OWNER);
    
    cJSON* hardware = cJSON_CreateObject();
    
    cJSON* lcd = cJSON_CreateObject();
    cJSON_AddBoolToObject(lcd, "present", lcd_present);
    cJSON_AddBoolToObject(lcd, "working", lcd_working);
    cJSON_AddItemToObject(hardware, "lcd", lcd);
    
    cJSON* inputBoard = cJSON_CreateObject();
    cJSON_AddBoolToObject(inputBoard, "present", input_board_present);
    cJSON_AddBoolToObject(inputBoard, "working", input_board_working);
    cJSON_AddItemToObject(hardware, "inputBoard", inputBoard);
    
    cJSON* outputBoard = cJSON_CreateObject();
    cJSON_AddBoolToObject(outputBoard, "present", output_board_present);
    cJSON_AddBoolToObject(outputBoard, "working", output_board_working);
    cJSON_AddItemToObject(hardware, "outputBoard", outputBoard);
    
    cJSON* adc = cJSON_CreateObject();
    cJSON_AddBoolToObject(adc, "present", adc_present);
    cJSON_AddBoolToObject(adc, "working", adc_working);
    cJSON_AddItemToObject(hardware, "adc", adc);
    
    cJSON* soundModule = cJSON_CreateObject();
    cJSON_AddBoolToObject(soundModule, "present", sound_present);
    cJSON_AddBoolToObject(soundModule, "working", sound_working);
    cJSON_AddItemToObject(hardware, "soundModule", soundModule);
    
    cJSON_AddItemToObject(data, "hardware", hardware);
    cJSON_AddItemToObject(payload, "data", data);
    cJSON_AddItemToObject(root, "payload", payload);
    
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ws_server_send_text(json_str, strlen(json_str));
        free(json_str);
    }
    
    cJSON_Delete(root);
}
```

### 5. Implement Hardware Detection Functions

You'll need to implement these helper functions:

```c
// In lcd_driver.c
bool lcd_is_present(void) {
    // Check if LCD responds on I2C bus (address 0x27)
    esp_err_t ret = i2c_master_write_byte(...);
    return (ret == ESP_OK);
}

bool lcd_is_working(void) {
    // Check if LCD responds to commands correctly
    // Can try reading status or verify initialization state
    return lcd_initialized && lcd_is_present();
}

// In io_expander.c
bool io_expander_is_board_present(uint8_t board) {
    uint8_t addr = (board == 0) ? 0x20 : 0x21;
    // Try to read from board
    uint8_t dummy;
    esp_err_t ret = i2c_master_read_from_device(..., &dummy, 1, ...);
    return (ret == ESP_OK);
}

bool io_expander_is_board_working(uint8_t board) {
    // More thorough test - try write/read cycle
    return io_expander_is_board_present(board) && 
           board_responds_to_commands(board);
}

// In adc_driver.c
bool adc_is_present(void) {
    // Check if ADS1015 responds on I2C bus (address 0x48)
    esp_err_t ret = i2c_master_write_byte(...);
    return (ret == ESP_OK);
}

bool adc_is_working(void) {
    // Try reading a value to verify ADC is functional
    int16_t test_value = adc_read_channel(0);
    return (test_value >= 0); // Negative indicates error
}
```

### 6. Update Prompts Documentation

Add device identity information to:
- `ots-fw-main/prompts/PROMPTS_INDEX.md`
- Module-specific prompts where relevant

Example addition:
```markdown
## Device Identity

Each OTS device has unique, immutable identification:
- **Serial Number**: `OTS-FW-XXXXXX` format (defined in config.h)
- **Owner**: Device owner name (defined in config.h)

These values are compiled into the firmware and cannot be changed by users.
```

## Testing

### Test Sequence

1. **Build and flash firmware** with diagnostic support
2. **Connect userscript** to firmware WebSocket
3. **Click "🔧 Diag" button** in userscript HUD
4. **Verify response** appears in log with all hardware status
5. **Test with components disconnected** to verify detection

### Expected Output Example

```
[Hardware Diagnostic]
Version: 2025-12-31.1
Device: firmware
Serial: OTS-FW-ESP001
Owner: DrZoid
LCD: ✓ Present, ✓ Working
Input Board: ✓ Present, ✓ Working
Output Board: ✓ Present, ✓ Working
ADC: ✓ Present, ✓ Working
Sound Module: ✗ Not Present
```

## Files Changed

### Completed (userscript + server)
- ✅ `/prompts/WEBSOCKET_MESSAGE_SPEC.md` - Protocol documentation
- ✅ `/ots-shared/src/game.ts` - TypeScript types
- ✅ `/ots-simulator/server/routes/ws.ts` - Simulator response
- ✅ `/ots-userscript/src/hud/sidebar-hud.ts` - Diagnostic button & handler
- ✅ `/ots-userscript/src/websocket/client.ts` - Command sending
- ✅ `/ots-userscript/src/main.user.ts` - HUD initialization

### TODO (firmware)
- ⏳ `/ots-fw-main/include/protocol.h` - Add event type
- ⏳ `/ots-fw-main/src/protocol.c` - Add string conversion
- ⏳ `/ots-fw-main/include/config.h` - Add device identity
- ⏳ `/ots-fw-main/src/ws_server.c` - Command handler
- ⏳ `/ots-fw-main/src/lcd_driver.c` - Detection functions
- ⏳ `/ots-fw-main/src/io_expander.c` - Detection functions
- ⏳ `/ots-fw-main/src/adc_driver.c` - Detection functions

## Next Steps

1. Add event type to protocol.h/protocol.c
2. Add device identity to config.h
3. Implement send_hardware_diagnostic() function
4. Implement hardware detection helper functions
5. Test on real hardware
6. Update firmware prompt documentation

