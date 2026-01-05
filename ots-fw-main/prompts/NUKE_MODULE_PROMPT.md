# Nuke Module - Project Prompt

## Overview

The **Nuke Module** is an input/output hardware module for the OpenFront Tactical Suitcase (OTS) that allows players to launch nuclear weapons. It provides three launch buttons (ATOM, HYDRO, MIRV) with corresponding LED feedback to indicate launch status.

## Purpose

The Nuke Module serves as:
- **Primary weapon control interface**: Launch nuclear strikes via physical buttons
- **Visual feedback system**: LEDs confirm successful launches
- **Game state indicator**: Shows when nukes are in flight

## Hardware Components

### Button Inputs (3 total)

All buttons are connected via MCP23017 I/O expander boards using the `ModuleIO` abstraction layer.

| Button | Purpose | Pin Mapping | Configuration |
|--------|---------|-------------|---------------|
| **ATOM** | Launch atomic bomb | Board 0, Pin 1 | INPUT_PULLUP (active-low) |
| **HYDRO** | Launch hydrogen bomb | Board 0, Pin 2 | INPUT_PULLUP (active-low) |
| **MIRV** | Launch MIRV strike | Board 0, Pin 3 | INPUT_PULLUP (active-low) |

**Button Configuration:**
- Internal pull-up resistors enabled (100kΩ on MCP23017)
- Active-low: Button press connects pin to GND
- Debouncing: 50ms polling interval
- No cooldown restriction: Rapid fire allowed

### LED Outputs (3 total)

| LED | Purpose | Pin Mapping | Behavior |
|-----|---------|-------------|----------|
| **ATOM LED** | Atomic bomb launch indicator | Board 0, Pin 8 | Blinks when ATOM nuke launched |
| **HYDRO LED** | Hydrogen bomb launch indicator | Board 0, Pin 9 | Blinks when HYDRO nuke launched |
| **MIRV LED** | MIRV launch indicator | Board 0, Pin 10 | Blinks when MIRV launched |

### Current Pin Mapping (in `nuke_module.h`)

```c
// Nuke Module - Button Inputs (Board 0 - INPUT pins)
#define NUKE_BTN_ATOM_BOARD  0
#define NUKE_BTN_ATOM_PIN    1
#define NUKE_BTN_HYDRO_BOARD 0
#define NUKE_BTN_HYDRO_PIN   2
#define NUKE_BTN_MIRV_BOARD  0
#define NUKE_BTN_MIRV_PIN    3

// Nuke Module - LED Outputs (Board 1 - OUTPUT pins)
#define NUKE_LED_ATOM_BOARD  1
#define NUKE_LED_ATOM_PIN    8
#define NUKE_LED_HYDRO_BOARD 1
#define NUKE_LED_HYDRO_PIN   9
#define NUKE_LED_MIRV_BOARD  1
#define NUKE_LED_MIRV_PIN    10
```

## Functional Behavior

### Button Press Flow

1. **Player presses button** (e.g., ATOM)
2. **Firmware detects press** via `ModuleIO::readNukeButtons()` (50ms polling)
3. **Command sent to server** as `NUKE_LAUNCHED` event via WebSocket
4. **Server processes launch** (game logic)
5. **Server confirms launch** by sending back `NUKE_LAUNCHED` event
6. **LED starts blinking** for X seconds (launch confirmation)
7. **LED turns OFF** after timeout

### Important: LEDs Are Event-Driven

**LEDs do NOT respond to button presses directly.** They only blink when the game server confirms the launch by sending an event back.

- ❌ Button press → LED immediate response
- ✅ Button press → Server event → LED blink

This ensures LEDs reflect actual game state, not just local button activity.

### Launch Event Types

The module handles three types of launch events from the server:

| Event Type | Triggers | LED Response |
|------------|----------|--------------|
| `NUKE_LAUNCHED` | Atomic bomb launched | ATOM LED blinks for X seconds |
| `HYDRO_LAUNCHED` | Hydrogen bomb launched | HYDRO LED blinks for X seconds |
| `MIRV_LAUNCHED` | MIRV launched | MIRV LED blinks for X seconds |

### LED Blink Behavior

When a launch event is received:

1. **LED turns ON**
2. **Blink pattern**: 500ms ON / 500ms OFF (1Hz)
3. **Duration**: X seconds (configurable, default: 5-10 seconds)
4. **Auto-off**: LED stops blinking after timeout
5. **Multiple launches**: Each LED operates independently

**Timing Constants** (in firmware):
```cpp
#define NUKE_BLINK_DURATION_MS  10000  // 10 seconds
#define NUKE_BLINK_INTERVAL_MS  500    // 500ms (1Hz blink)
```

### No Launch Cooldown

- Players can press buttons as fast as they want
- No artificial delay between launches
- Server/game logic controls actual launch limits
- Firmware just sends commands immediately

## Protocol Integration

### Outgoing Messages (to Server)

When a button is pressed, send a `cmd` or `event` message:

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_LAUNCHED",
    "timestamp": 1234567890
  }
}
```

Or for different nuke types:
- `NUKE_LAUNCHED` for ATOM button
- `HYDRO_LAUNCHED` for HYDRO button
- `MIRV_LAUNCHED` for MIRV button

### Incoming Messages (from Server)

The module processes `event` messages to trigger LED blinking:

```json
{
  "type": "event",
  "payload": {
    "type": "NUKE_LAUNCHED",
    "message": "Atomic bomb launched!",
    "timestamp": 1234567890
  }
}
```

**Event types that trigger LEDs:**
- `NUKE_LAUNCHED` → ATOM LED blinks
- `HYDRO_LAUNCHED` → HYDRO LED blinks
- `MIRV_LAUNCHED` → MIRV LED blinks

## Software Architecture

### Core Components

1. **Button Polling** (`pollButtons()` in `main.cpp`)
   - Reads button states every 50ms
   - Detects press transitions (not pressed → pressed)
   - Sends launch events to server
   - Does NOT control LEDs

2. **Event Handler** (in `ws_server.c`/`ws_protocol.c`)
   - Receives launch events from server
   - Triggers LED blink timers
   - Updates nuke module state

3. **LED Controller** (`updateNukeModule()`)
   - Manages blink timing for each LED
   - Uses non-blocking timers (millis() based)
   - Updates hardware via `ModuleIO`

### Data Structures

```cpp
struct NukeLaunchState {
  bool active;              // Is this nuke currently in flight?
  unsigned long startTime;  // When launch was confirmed (millis)
  unsigned long duration;   // How long LED should blink (milliseconds)
  unsigned long blinkInterval; // Blink on/off interval (milliseconds)
  bool ledState;            // Current LED state (on/off)
  unsigned long lastToggle; // Last blink toggle time
};

struct NukeModuleState {
  NukeLaunchState atom;
  NukeLaunchState hydro;
  NukeLaunchState mirv;
};
```

### Implementation Example

```cpp
void handleLaunchEvent(GameEventType eventType) {
  unsigned long now = millis();
  
  switch (eventType) {
    case GameEventType::NUKE_LAUNCHED:
      nukeState.atom.active = true;
      nukeState.atom.startTime = now;
      nukeState.atom.duration = NUKE_BLINK_DURATION_MS;
      nukeState.atom.blinkInterval = NUKE_BLINK_INTERVAL_MS;
      nukeState.atom.ledState = true;
      nukeState.atom.lastToggle = now;
      Serial.println("[Nuke] ATOM launch confirmed!");
      break;
      
    case GameEventType::HYDRO_LAUNCHED:
      nukeState.hydro.active = true;
      nukeState.hydro.startTime = now;
      nukeState.hydro.duration = NUKE_BLINK_DURATION_MS;
      nukeState.hydro.blinkInterval = NUKE_BLINK_INTERVAL_MS;
      nukeState.hydro.ledState = true;
      nukeState.hydro.lastToggle = now;
      Serial.println("[Nuke] HYDRO launch confirmed!");
      break;
      
    case GameEventType::MIRV_LAUNCHED:
      nukeState.mirv.active = true;
      nukeState.mirv.startTime = now;
      nukeState.mirv.duration = NUKE_BLINK_DURATION_MS;
      nukeState.mirv.blinkInterval = NUKE_BLINK_INTERVAL_MS;
      nukeState.mirv.ledState = true;
      nukeState.mirv.lastToggle = now;
      Serial.println("[Nuke] MIRV launch confirmed!");
      break;
  }
}

void updateNukeModule(unsigned long currentTime) {
  updateNukeLED(nukeState.atom, NukeModule::LED_ATOM, currentTime);
  updateNukeLED(nukeState.hydro, NukeModule::LED_HYDRO, currentTime);
  updateNukeLED(nukeState.mirv, NukeModule::LED_MIRV, currentTime);
}

bool updateNukeLED(NukeLaunchState &state, const PinMap &ledPin, unsigned long currentTime) {
  if (!state.active) return false;
  
  // Check timeout
  if (currentTime - state.startTime >= state.duration) {
    state.active = false;
    moduleIO.setLED(ledPin, false);
    return false;
  }
  
  // Handle blinking
  if (currentTime - state.lastToggle >= state.blinkInterval) {
    state.ledState = !state.ledState;
    state.lastToggle = currentTime;
    moduleIO.setLED(ledPin, state.ledState);
  }
  
  return true; // Still active
}
```

### Button Sending Commands

Update `pollButtons()` in `main.cpp`:

```cpp
void pollButtons() {
  static unsigned long lastPoll = 0;
  static ModuleIO::NukeButtons lastButtons = {false, false, false};
  
  unsigned long now = millis();
  if (now - lastPoll < 50) return;  // Poll every 50ms (debounce)
  lastPoll = now;
  
  ModuleIO::NukeButtons buttons = moduleIO.readNukeButtons();
  
  // Detect button presses (transition from not pressed to pressed)
  if (buttons.atom && !lastButtons.atom) {
    Serial.println("[MAIN] ATOM button pressed - launching!");
    GameEvent event;
    event.type = GameEventType::NUKE_LAUNCHED;
    event.timestamp = millis();
    otsWs.sendGameEvent(event);
  }
  
  if (buttons.hydro && !lastButtons.hydro) {
    Serial.println("[MAIN] HYDRO button pressed - launching!");
    GameEvent event;
    event.type = GameEventType::HYDRO_LAUNCHED;
    event.timestamp = millis();
    otsWs.sendGameEvent(event);
  }
  
  if (buttons.mirv && !lastButtons.mirv) {
    Serial.println("[MAIN] MIRV button pressed - launching!");
    GameEvent event;
    event.type = GameEventType::MIRV_LAUNCHED;
    event.timestamp = millis();
    otsWs.sendGameEvent(event);
  }
  
  lastButtons = buttons;
}
```

## Configuration

### Timing Constants

Define in firmware code or configuration header:

```cpp
// Nuke launch LED blink durations (in milliseconds)
#define NUKE_BLINK_DURATION_MS  10000  // 10 seconds
#define HYDRO_BLINK_DURATION_MS 10000  // 10 seconds
#define MIRV_BLINK_DURATION_MS  10000  // 10 seconds

// Blink intervals (in milliseconds)
#define NUKE_BLINK_INTERVAL_MS  500    // 500ms (1Hz blink)

// Button polling interval
#define BUTTON_POLL_INTERVAL_MS 50     // 50ms debounce
```

### Hardware Configuration

Pin mappings in `include/module_io.h`:

```cpp
// Nuke Module - Button Inputs (Board 0 - INPUT pins)
#define NUKE_BTN_ATOM_BOARD  0
#define NUKE_BTN_ATOM_PIN    1
#define NUKE_BTN_HYDRO_BOARD 0
#define NUKE_BTN_HYDRO_PIN   2
#define NUKE_BTN_MIRV_BOARD  0
#define NUKE_BTN_MIRV_PIN    3

// Nuke Module - LED Outputs (Board 1 - OUTPUT pins)
#define NUKE_LED_ATOM_BOARD  1
#define NUKE_LED_ATOM_PIN    8
#define NUKE_LED_HYDRO_BOARD 1
#define NUKE_LED_HYDRO_PIN   9
#define NUKE_LED_MIRV_BOARD  1
#define NUKE_LED_MIRV_PIN    10
```

## Implementation Checklist

### Phase 1: Button Command Sending
- [x] Button polling implemented (50ms interval)
- [ ] Send `NUKE_LAUNCHED` event on ATOM button press
- [ ] Send `HYDRO_LAUNCHED` event on HYDRO button press
- [ ] Send `MIRV_LAUNCHED` event on MIRV button press
- [ ] Test commands reach server via WebSocket

### Phase 2: LED Blink Logic
- [ ] Define `NukeLaunchState` and `NukeModuleState` structures
- [ ] Implement `handleLaunchEvent()` function
- [ ] Implement `updateNukeLED()` with timeout and blink logic
- [ ] Integrate event handler in WebSocket message processing
- [ ] Add `updateNukeModule()` to main loop

### Phase 3: Event Processing
- [ ] Parse `NUKE_LAUNCHED` events in `ws_server.c`
- [ ] Parse `HYDRO_LAUNCHED` events
- [ ] Parse `MIRV_LAUNCHED` events
- [ ] Trigger LED blinks on event receipt
- [ ] Test with simulated server events

### Phase 4: Integration Testing
- [ ] Test full cycle: button press → server → LED blink
- [ ] Verify multiple simultaneous launches work
- [ ] Test LED timeout behavior
- [ ] Verify LEDs turn off after duration expires
- [ ] Test rapid button presses (no cooldown)

## Testing Strategy

### Unit Tests

1. **Button Detection**: Verify button presses detected correctly
2. **Event Sending**: Confirm events sent to server on button press
3. **LED Timing**: Validate blink intervals are accurate (500ms)
4. **LED Timeout**: Verify LEDs turn off after configured duration
5. **Multiple Launches**: Test all three LEDs can blink simultaneously

### Integration Tests

1. **End-to-End**: Button press → WebSocket → Server → Event → LED blink
2. **Event Processing**: Server events correctly trigger LED patterns
3. **State Synchronization**: LEDs reflect actual game state
4. **No Cooldown**: Rapid button presses all send commands

### Hardware Tests

1. **LED Visibility**: Verify blink pattern is clearly visible
2. **Button Responsiveness**: 50ms debounce feels responsive
3. **Multiple Boards**: Test pin mappings across MCP23017 boards
4. **I2C Stability**: Ensure reliable operation under load

## Workflow Example

**Scenario**: Player launches atomic bomb

1. Player presses **ATOM** button
2. Firmware detects press in `pollButtons()`
3. `NUKE_LAUNCHED` event sent to server via WebSocket
4. Server processes launch (game logic checks resources, cooldowns, etc.)
5. **If launch succeeds:**
   - Server sends `NUKE_LAUNCHED` event back to firmware
   - Firmware receives event in WebSocket handler
   - `handleLaunchEvent()` activates ATOM LED blink timer
   - ATOM LED blinks for 10 seconds
   - LED automatically turns off after timeout
6. **If launch fails:**
   - Server sends error event or no response
   - LED does not activate (no confirmation)

## Future Enhancements

### Short Term
- [ ] Different blink patterns for different nuke types
- [ ] LED brightness control (PWM)
- [ ] Sound effects via buzzer/speaker integration

### Medium Term
- [ ] Show launch progress (faster blink = closer to impact)
- [ ] Multi-stage launch (arm → confirm → launch)
- [ ] Visual countdown timer on LEDs

### Long Term
- [ ] RGB LEDs for color-coded nuke types
- [ ] Animation sequences (launch, flight, impact)
- [ ] Integration with external display (LCD/OLED)

## Troubleshooting

### Button Not Responding
- Check pull-up resistor enabled (INPUT_PULLUP)
- Verify button wiring (one side to pin, other to GND)
- Test button directly with multimeter
- Review serial logs for button press detection
- Check MCP23017 initialization

### LED Not Blinking
- Verify event received from server (check serial logs)
- Test LED directly: `moduleIO.setLED(NukeModule::LED_ATOM, true)`
- Check pin mapping in `module_io.h`
- Ensure `updateNukeModule()` called in main loop
- Verify MCP23017 Board 0 initialized correctly

### LED Stays ON
- Check timeout logic in `updateNukeLED()`
- Verify `duration` field is set correctly
- Review `millis()` overflow handling (rare, ~49 days)
- Check for stuck event processing

### Commands Not Reaching Server
- Verify WebSocket connection (LINK LED should be ON)
- Check event serialization in `serializeGameEvent()`
- Review server logs for received messages
- Test with WebSocket debugging tools

### Multiple Simultaneous Launches Don't Work
- Verify each `NukeLaunchState` is independent
- Check that `updateNukeLED()` is called for all three
- Ensure no shared state between LED controllers
- Test I2C bus performance under load

## Related Documentation

- **Main Firmware Context**: `copilot-project-context.md`
- **Protocol Specification**: `../WEBSOCKET_MESSAGE_SPEC.md` (repository root)
- **Hardware I/O**: See "Hardware I/O Architecture" in `copilot-project-context.md`
- **MCP23017 Library**: Adafruit MCP23017 Arduino Library documentation
- **Alert Module**: `prompts/ALERT_MODULE_PROMPT.md` (similar blink logic)

---

**Module Type:** Input/Output (buttons + LEDs)  
**I/O Boards:** MCP23017 Board 0 (shared with Main Power Module)  
**Button Count:** 3 (ATOM, HYDRO, MIRV)  
**LED Count:** 3 (corresponding to each button)  
**Timing:** Non-blocking, millis()-based  
**Cooldown:** None (rapid fire allowed)  
**LED Trigger:** Server events only (not local button presses)  
**Last Updated:** December 9, 2025
