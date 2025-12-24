# RGB LED Status Indicator

Simple status-only RGB LED implementation for ESP32-S3 onboard LED (GPIO48).

## Status Colors

| Status | Color | Pattern | Meaning |
|--------|-------|---------|---------|
| **DISCONNECTED** | 🔴 Red | Solid | No WiFi connection |
| **WIFI_ONLY** | 🟠 Orange | Solid | WiFi connected, WebSocket not connected |
| **CONNECTED** | 🟢 Green | Solid | WiFi + WebSocket both connected (fully operational) |
| **ERROR** | 🔴 Red | Fast blink (5Hz) | Hardware error (I2C failure, etc.) |

## Hardware

- **GPIO Pin**: GPIO48 (typical ESP32-S3 devboard onboard RGB LED)
- **LED Type**: WS2812B addressable RGB LED
- **Driver**: ESP-IDF `led_strip` component with RMT backend
- **Update Rate**: 100ms (10Hz task)

## Configuration

Edit `/include/config.h` to change GPIO pin:

```c
// RGB LED GPIO pin (onboard WS2812 on most ESP32-S3 devboards)
#define RGB_LED_GPIO 48
```

## API Usage

```c
#include "rgb_status.h"

// Initialize (call once in main)
esp_err_t ret = rgb_status_init();

// Set status
rgb_status_set(RGB_STATUS_DISCONNECTED);  // Red solid
rgb_status_set(RGB_STATUS_WIFI_ONLY);     // Orange solid
rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED); // Purple solid
rgb_status_set(RGB_STATUS_GAME_STARTED);         // Green solid
rgb_status_set(RGB_STATUS_ERROR);         // Red fast blink

// Get current status
rgb_status_t current = rgb_status_get();
```

## Automatic Behavior

The RGB LED automatically updates based on system events:

### Network Events
- **Network disconnected** → Red solid
- **Network connected** → Orange solid (WiFi only, waiting for WebSocket)

### WebSocket Events
- **WebSocket connected** → Green solid (fully operational)
- **WebSocket disconnected** → Orange solid (falls back to WiFi only)

### Hardware Events
- **I2C recovery** → Red fast blink for 2 seconds, then restore previous status
- **OTA update** → Red solid during update, green briefly before reboot

## Integration Points

Automatic status updates are integrated in:

1. **main.c** - Network and WebSocket connection handlers
2. **ota_manager.c** - OTA update progress (shows error state)
3. **main.c** - I/O expander recovery callback (shows error temporarily)

## Troubleshooting

### LED Not Working

Check GPIO pin:
```bash
# ESP32-S3 devboard typically uses GPIO48
# Some boards may use different pins
```

Check component dependencies in CMakeLists.txt:
```cmake
REQUIRES 
    driver  # Must include this for led_strip
```

### Wrong Colors

The WS2812B color format is **GRB** (not RGB). The driver is configured for this:
```c
.color_component_format = {
    .format = LED_STRIP_COLOR_COMPONENT_FMT_GRB
}
```

### LED Flickers

Ensure task priority allows regular updates. The RGB task runs at priority 5:
```c
xTaskCreate(rgb_status_task, "rgb_status", 2048, NULL, 5, &rgb_task_handle);
```

## Future Enhancements

For more advanced features, see `/prompts/RGB_STATUS_PLAN.md` for:
- Breathing/pulse effects
- OTA progress bar using LED brightness
- Multiple priority levels
- Configurable patterns
- Game state ambient effects

## Resource Usage

- **RAM**: ~2KB (task stack)
- **CPU**: Minimal (~1% at 10Hz update rate)
- **Dependencies**: `led_strip` component (ESP-IDF)
- **GPIO**: 1 pin (GPIO48)

## Notes

- The RGB LED is independent of the module LED controller (`led_controller.c`)
- Module LEDs (nuke buttons, alert indicators) continue to function normally
- RGB updates happen in a separate FreeRTOS task (non-blocking)
- Status changes are logged for debugging

## See Also

- [Main Power Module](prompts/MAIN_POWER_MODULE_PROMPT.md) - LINK LED behavior
- [LED Controller](include/led_controller.h) - Module LED control
- [Event Dispatcher](include/event_dispatcher.h) - System event handling
