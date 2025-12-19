# Alert Module - Project Prompt

## Overview

The **Alert Module** is an output-only hardware module for the OpenFront Tactical Suitcase (OTS) that provides visual feedback for incoming threats and warnings. It displays threat alerts through 6 LEDs controlled via MCP23017 I/O expander boards.

## Purpose

The Alert Module serves as a real-time threat notification system, visually indicating:
- General warning status (when any threat is active)
- Incoming nuclear strike
- Incoming hydrogen bomb
- Incoming MIRV strike
- Land-based invasion
- Naval-based invasion

## Hardware Components

### LED Outputs (6 total)

All LEDs are controlled via MCP23017 I/O expander boards using the `ModuleIO` abstraction layer.

| LED | Purpose | Pin Mapping | Behavior |
|-----|---------|-------------|----------|
| **WARNING** | General alert indicator | Configured in `module_io.h` | Fast blink (300ms) when ANY other alert is active |
| **ATOM** | Incoming nuclear strike | Configured in `module_io.h` | Blinks for X seconds on alert |
| **HYDRO** | Incoming hydrogen bomb | Configured in `module_io.h` | Blinks for X seconds on alert |
| **MIRV** | Incoming MIRV strike | Configured in `module_io.h` | Blinks for X seconds on alert |
| **LAND** | Land invasion alert | Configured in `module_io.h` | Blinks for X seconds on alert |
| **NAVAL** | Naval invasion alert | Configured in `module_io.h` | Blinks for X seconds on alert |

**Note:** LEDs can be distributed across one or multiple MCP23017 boards. Pin assignments are defined in `include/module_io.h` using the `PinMap` structure.

### Current Pin Mapping (in `alert_module.h`)

```c
// Alert Module - LED Outputs (Board 1 - OUTPUT pins)
#define ALERT_LED_WARNING_BOARD 1
#define ALERT_LED_WARNING_PIN   0
#define ALERT_LED_ATOM_BOARD    1
#define ALERT_LED_ATOM_PIN      1
#define ALERT_LED_HYDRO_BOARD   1
#define ALERT_LED_HYDRO_PIN     2
#define ALERT_LED_MIRV_BOARD    1
#define ALERT_LED_MIRV_PIN      3
#define ALERT_LED_LAND_BOARD    1
#define ALERT_LED_LAND_PIN      4
#define ALERT_LED_NAVAL_BOARD   1
#define ALERT_LED_NAVAL_PIN     5
```

## Functional Behavior

### Alert Activation

1. **Trigger Source**: Game events received from the OTS server via WebSocket
   - Events are received as `event` type messages
   - Event types: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
   - Game events update the internal game state (`m_alert` fields in `hwState`)

2. **LED Blink Pattern**:
   - Each alert LED blinks at its configured interval for a specific duration
   - Blink interval: Configurable per alert type (default: same as warning)
   - Alert duration: X seconds (configurable timeout)
   - Multiple alerts can be active simultaneously

3. **WARNING LED Logic**:
   - The WARNING LED uses a **fast blink** pattern: 300ms ON / 300ms OFF
   - WARNING LED is active whenever **any** other alert LED is blinking
   - WARNING LED automatically turns off when all alert LEDs are inactive

### Alert Clearing

- **Automatic Timeout**: Each alert has an individual timeout duration
- After the timeout expires, the corresponding LED stops blinking
- The WARNING LED continues blinking as long as at least one alert LED is active
- No manual player action required to clear alerts

### Multiple Simultaneous Alerts

- The module supports multiple concurrent alerts
- Each alert LED operates independently with its own timeout
- WARNING LED remains active as long as ANY alert is active
- Example: ATOM + LAND alerts can be active at the same time

## Protocol Integration

### Incoming Messages (from OTS Server)

The Alert Module reacts to messages received via the WebSocket server:

#### 1. Game State Updates (`state` messages)
```json
{
  "type": "state",
  "payload": {
    "gameState": { ... },
    "hwState": {
      "m_alert": {
        "m_alert_warning": false,  // Computed by firmware
        "m_alert_atom": true,      // Incoming nuke alert
        "m_alert_hydro": false,    // Incoming hydro alert
        "m_alert_mirv": false,     // Incoming MIRV alert
        "m_alert_land": false,     // Land invasion alert
        "m_alert_naval": false     // Naval invasion alert
      }
    }
  }
}
```

#### 2. Game Events (`event` messages)
```json
{
  "type": "event",
  "payload": {
    "type": "ALERT_ATOM",  // or ALERT_HYDRO, ALERT_MIRV, etc.
    "message": "Incoming nuclear strike detected!"
  }
}
```

### Outgoing Messages (to OTS Server)

The Alert Module is **output-only** and does not send commands to the server. It only reacts to incoming state updates and events.

## Software Architecture

### Core Components

1. **Alert State Manager**
   - Tracks active alerts and their timers
   - Manages individual alert timeouts
   - Calculates WARNING LED state based on active alerts

2. **LED Controller**
   - Handles blink timing for each LED
   - Uses non-blocking timers (millis() based)
   - Updates hardware via `ModuleIO` class

3. **Event Handler**
   - Processes incoming `event` and `state` messages
   - Triggers alert activation with timeout
   - Updates internal alert state

### Data Structures

```cpp
struct AlertState {
  bool active;              // Is this alert currently active?
  unsigned long startTime;  // When alert was triggered (millis)
  unsigned long duration;   // How long alert should last (milliseconds)
  unsigned long blinkInterval; // Blink on/off interval (milliseconds)
  bool ledState;            // Current LED state (on/off)
  unsigned long lastToggle; // Last blink toggle time
};

struct AlertModuleState {
  AlertState atom;
  AlertState hydro;
  AlertState mirv;
  AlertState land;
  AlertState naval;
  
  // WARNING LED state (computed)
  bool warningActive;
  unsigned long warningLastToggle;
  bool warningLedState;
};
```

### Main Loop Logic

```cpp
void updateAlertModule(unsigned long currentTime) {
  bool anyAlertActive = false;
  
  // Update each alert LED
  anyAlertActive |= updateAlert(alertState.atom, AlertModule::LED_ATOM, currentTime);
  anyAlertActive |= updateAlert(alertState.hydro, AlertModule::LED_HYDRO, currentTime);
  anyAlertActive |= updateAlert(alertState.mirv, AlertModule::LED_MIRV, currentTime);
  anyAlertActive |= updateAlert(alertState.land, AlertModule::LED_LAND, currentTime);
  anyAlertActive |= updateAlert(alertState.naval, AlertModule::LED_NAVAL, currentTime);
  
  // Update WARNING LED (fast blink when any alert active)
  updateWarningLED(anyAlertActive, currentTime);
}

bool updateAlert(AlertState &alert, const PinMap &ledPin, unsigned long currentTime) {
  if (!alert.active) return false;
  
  // Check timeout
  if (currentTime - alert.startTime >= alert.duration) {
    alert.active = false;
    moduleIO.setLED(ledPin, false);
    return false;
  }
  
  // Handle blinking
  if (currentTime - alert.lastToggle >= alert.blinkInterval) {
    alert.ledState = !alert.ledState;
    alert.lastToggle = currentTime;
    moduleIO.setLED(ledPin, alert.ledState);
  }
  
  return true; // Alert still active
}

void updateWarningLED(bool shouldBeActive, unsigned long currentTime) {
  if (!shouldBeActive) {
    moduleIO.setLED(AlertModule::LED_WARNING, false);
    alertState.warningActive = false;
    return;
  }
  
  // Fast blink (300ms interval)
  if (currentTime - alertState.warningLastToggle >= 300) {
    alertState.warningLedState = !alertState.warningLedState;
    alertState.warningLastToggle = currentTime;
    moduleIO.setLED(AlertModule::LED_WARNING, alertState.warningLedState);
  }
}
```

## Configuration

### Timing Constants

Define in firmware code or configuration:

```cpp
// Alert durations (in milliseconds)
#define ALERT_DURATION_ATOM   10000  // 10 seconds
#define ALERT_DURATION_HYDRO  10000  // 10 seconds
#define ALERT_DURATION_MIRV   10000  // 10 seconds
#define ALERT_DURATION_LAND   15000  // 15 seconds
#define ALERT_DURATION_NAVAL  15000  // 15 seconds

// Blink intervals (in milliseconds)
#define ALERT_BLINK_INTERVAL     500  // 500ms for alert LEDs
#define WARNING_BLINK_INTERVAL   300  // 300ms for WARNING LED (fast)
```

### Hardware Configuration

Update pin mappings in `include/module_io.h` to match physical wiring:

```cpp
// Alert Module - LED Outputs (Board 1 - OUTPUT pins)
#define ALERT_LED_WARNING_BOARD 1
#define ALERT_LED_WARNING_PIN   0
#define ALERT_LED_ATOM_BOARD    1
#define ALERT_LED_ATOM_PIN      1
#define ALERT_LED_HYDRO_BOARD   1
#define ALERT_LED_HYDRO_PIN     2
#define ALERT_LED_MIRV_BOARD    1
#define ALERT_LED_MIRV_PIN      3
#define ALERT_LED_LAND_BOARD    1
#define ALERT_LED_LAND_PIN      4
#define ALERT_LED_NAVAL_BOARD   1
#define ALERT_LED_NAVAL_PIN     5

// Or spread across multiple boards:
// #define ALERT_LED_WARNING_BOARD 0
// #define ALERT_LED_WARNING_PIN   15
// etc.
```

## Implementation Checklist

### Phase 1: Basic LED Control
- [ ] Define `AlertState` and `AlertModuleState` structures
- [ ] Implement `updateAlert()` function with timeout logic
- [ ] Implement `updateWarningLED()` with fast blink
- [ ] Test individual LED control via `ModuleIO`

### Phase 2: Event Integration
- [ ] Parse `ALERT_*` event types in `protocol.cpp`
- [ ] Trigger alert activation on event receipt
- [ ] Update internal `m_alert` state fields
- [ ] Test with simulated WebSocket messages

### Phase 3: State Synchronization
- [ ] Process `state` messages to set alert states
- [ ] Ensure consistency between events and state updates
- [ ] Handle edge cases (alert re-trigger, timeout reset)

### Phase 4: Multi-Alert Support
- [ ] Test multiple simultaneous alerts
- [ ] Verify WARNING LED logic with various combinations
- [ ] Validate individual timeout behavior

### Phase 5: Optimization
- [ ] Optimize blink timing accuracy
- [ ] Add debouncing if needed
- [ ] Performance testing with all alerts active

## Testing Strategy

### Unit Tests
1. **Alert Timeout**: Verify alerts clear after configured duration
2. **Blink Timing**: Validate blink intervals are accurate
3. **WARNING Logic**: Test WARNING LED with various alert combinations
4. **Multiple Alerts**: Ensure independent operation of concurrent alerts

### Integration Tests
1. **Event Processing**: Send `ALERT_*` events and verify LED activation
2. **State Updates**: Send `state` messages and verify LED synchronization
3. **Timeout Behavior**: Verify LEDs turn off after timeout expires
4. **WARNING LED**: Test that WARNING follows any-alert-active rule

### Hardware Tests
1. **LED Visibility**: Verify all LEDs are visible and bright enough
2. **Blink Rate**: Confirm 300ms blink is noticeable but not annoying
3. **Multiple Boards**: Test pin mappings across different MCP23017 boards
4. **I2C Stability**: Ensure reliable operation under continuous blinking

## Future Enhancements

### Short Term
- [ ] Configurable alert durations per threat type
- [ ] Different blink patterns for different threat levels
- [ ] Audio feedback integration (buzzer/speaker)

### Medium Term
- [ ] Priority-based WARNING LED patterns (fast/slow based on threat level)
- [ ] Alert history/log tracking
- [ ] LED brightness control (PWM support)

### Long Term
- [ ] RGB LED support for color-coded threats
- [ ] Animation patterns (chase, fade, pulse)
- [ ] External display integration (LCD/OLED threat details)

## Troubleshooting

### LED Not Blinking
- Check pin mapping in `module_io.h`
- Verify MCP23017 board address in `config.h`
- Ensure I2C bus is properly initialized
- Test LED with `moduleIO.setLED()` directly

### WARNING LED Always On
- Check for stuck alert states
- Verify timeout logic is working
- Debug `anyAlertActive` flag computation

### Timing Issues
- Use `millis()` for non-blocking delays
- Avoid `delay()` calls in main loop
- Check for unsigned long overflow (rare, ~49 days)

### Multiple Alerts Not Working
- Verify each `AlertState` is independently managed
- Check that timeouts don't interfere with each other
- Ensure sufficient processing time in main loop

## Related Documentation

- **Main Firmware Context**: `copilot-project-context.md`
- **Protocol Specification**: `../protocol-context.md` (repository root)
- **Hardware I/O**: See "Hardware I/O Architecture" in `copilot-project-context.md`
- **MCP23017 Library**: Adafruit MCP23017 Arduino Library documentation

---

**Module Type:** Output-only (no inputs)  
**I/O Boards:** MCP23017 via I2C  
**LED Count:** 6 (WARNING + 5 threat types)  
**Timing:** Non-blocking, millis()-based  
**Last Updated:** December 9, 2025
