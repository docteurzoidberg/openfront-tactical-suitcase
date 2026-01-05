# Module: Alert Module

## Overview
- **Module Name**: Alert Module
- **Size**: 16U (Standard height)
- **Game State Domain**: Inbound threat alerts
- **Hardware Interface**: Direct I2C via MCP23017 I/O Expander (Board 1)

## Description
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
- [x] 6x LEDs for threat indicators

All LEDs are controlled via MCP23017 I/O expander (Board 1) using the `ModuleIO` abstraction layer.

| LED | Purpose | Pin Mapping | Behavior |
|-----|---------|-------------|----------|
| **WARNING** | General alert indicator | Board 1, Pin 0 | Fast blink (300ms) when ANY other alert is active |
| **ATOM** | Incoming nuclear strike | Board 1, Pin 1 | Blinks for X seconds on alert |
| **HYDRO** | Incoming hydrogen bomb | Board 1, Pin 2 | Blinks for X seconds on alert |
| **MIRV** | Incoming MIRV strike | Board 1, Pin 3 | Blinks for X seconds on alert |
| **LAND** | Land invasion alert | Board 1, Pin 4 | Blinks for X seconds on alert |
| **NAVAL** | Naval invasion alert | Board 1, Pin 5 | Blinks for X seconds on alert |

**Note:** LEDs can be distributed across one or multiple MCP23017 boards. Pin assignments are defined in `include/module_io.h` using the `PinMap` structure.

### Current Pin Mapping (in `module_io.h`)

```cpp
namespace AlertModule {
  constexpr PinMap LED_WARNING = {1, 0};  // Board 1, Pin 0
  constexpr PinMap LED_ATOM    = {1, 1};  // Board 1, Pin 1
  constexpr PinMap LED_HYDRO   = {1, 2};  // Board 1, Pin 2
  constexpr PinMap LED_MIRV    = {1, 3};  // Board 1, Pin 3
  constexpr PinMap LED_LAND    = {1, 4};  // Board 1, Pin 4
  constexpr PinMap LED_NAVAL   = {1, 5};  // Board 1, Pin 5
}
```

### Communication Protocol
Main controller uses I2C via MCP23017:
- **Read operations**: None (output-only module)
- **Write LEDs**: Direct GPIO output control with blink patterns

## Functional Behavior

### Alert Activation

1. **Trigger Source**: Game events received from the OTS server via WebSocket
   - Events are received as `event` type messages
   - Event types: `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
   - Game events update the internal game state (`m_alert` fields in `hwState`)

2. **LED Blink Pattern**:
   - Each alert LED blinks at its configured interval for a specific duration
   - Blink interval: Configurable per alert type (default: 500ms)
   - Alert duration: X seconds (configurable timeout, default: 10-15 seconds)
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

## Game State Mapping

### Incoming Messages (from OTS Server)

The Alert Module reacts to messages received via the WebSocket server:

#### 1. Game State Updates (`state` messages)
```json
{
  "type": "state",
  "payload": {
    "gameState": { },
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
    "type": "ALERT_ATOM",
    "message": "Incoming nuclear strike detected!"
  }
}
```

Event types:
- `ALERT_ATOM` - Incoming atomic bomb
- `ALERT_HYDRO` - Incoming hydrogen bomb
- `ALERT_MIRV` - Incoming MIRV strike
- `ALERT_LAND` - Land invasion
- `ALERT_NAVAL` - Naval invasion

### Outgoing Messages (to OTS Server)

The Alert Module is **output-only** and does not send commands to the server. It only reacts to incoming state updates and events.

## Module Behavior

### On Alert Event
1. Server sends `event` message with alert type (e.g., `ALERT_ATOM`)
2. Firmware receives event via WebSocket
3. Corresponding alert LED starts blinking (e.g., ATOM LED)
4. WARNING LED activates with fast blink pattern
5. LED blinks for configured duration (default: 10-15 seconds)
6. LED automatically turns off after timeout
7. WARNING LED continues blinking if other alerts are still active
8. WARNING LED turns off when all alerts have expired

### LED Blink Pattern
- **Alert LEDs**: 500ms on, 500ms off (1Hz blink)
- **WARNING LED**: 300ms on, 300ms off (faster blink)
- **Duration**: Configurable per alert type (default: 10-15 seconds)
- **Independence**: Each LED has its own blink timer and state
- **Multiple alerts**: All active alerts blink simultaneously

## UI Component Mockup

Vue component path: `ots-simulator/app/components/hardware/AlertModule.vue`

### Props
```typescript
interface AlertModuleProps {
  connected: boolean
  activeAlerts: {
    warning: boolean
    atom: boolean
    hydro: boolean
    mirv: boolean
    land: boolean
    naval: boolean
  }
}
```

### Emits
```typescript
// No emits - this is an output-only module
```

### Visual Design
Six LED indicators displayed vertically or in a grid:
- [⚠️ WARNING] - Yellow/orange LED (fast blink when any alert active)
- [☢️ ATOM] - Red LED (incoming nuke)
- [☢️ HYDRO] - Red LED (incoming hydro bomb)
- [☢️ MIRV] - Red LED (incoming MIRV)
- [🪖 LAND] - Red LED (land invasion)
- [⚓ NAVAL] - Red LED (naval invasion)

Each LED can blink independently based on active alerts. WARNING LED shows aggregate alert status.

## Firmware Implementation

### Header
`ots-fw-main/include/module_io.h` (pin definitions)

### Source
`ots-fw-main/src/main.cpp` (alert processing and LED updates)

Key functions:
- `handleAlertEvent()`: Process alert events from server
- `updateAlertModule()`: Handle blink patterns for all LEDs, update GPIO output
- `updateAlert()`: Update individual alert LED state and timing
- `updateWarningLED()`: Update WARNING LED based on active alerts

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

## Configuration

### Timing Constants

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

## Notes
- Output-only module - no input devices
- Each LED has independent blink timer and state (non-blocking)
- Multiple alerts can be active simultaneously
- WARNING LED automatically managed based on active alerts
- Blink durations configurable per alert type
- Uses WebSocket protocol defined in `/prompts/WEBSOCKET_MESSAGE_SPEC.md`
- Module uses MCP23017 Board 1
- All timing is non-blocking using millis()

---

**Module Type:** Output-only (LEDs)  
**I/O Boards:** MCP23017 Board 1  
**LED Count:** 6 (WARNING + 5 threat types)  
**Timing:** Non-blocking, millis()-based  
**Alert Duration:** Configurable (10-15 seconds default)  
**WARNING LED:** Auto-managed based on active alerts
