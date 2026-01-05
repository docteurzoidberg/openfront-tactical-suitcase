# Module: Nuke Control Panel

## Overview
- **Module Name**: Nuke Control Panel
- **Size**: 16U (Standard height)
- **Game State Domain**: Outbound nuke launch commands
- **Hardware Interface**: Direct I2C via MCP23017 I/O Expander (Board 0)

## Description
The **Nuke Module** is an input/output hardware module for the OpenFront Tactical Suitcase (OTS) that allows players to launch nuclear weapons. It provides three launch buttons (ATOM, HYDRO, MIRV) with corresponding LED feedback to indicate launch status.

## Purpose

The Nuke Module serves as:
- **Primary weapon control interface**: Launch nuclear strikes via physical buttons
- **Visual feedback system**: LEDs confirm successful launches
- **Game state indicator**: Shows when nukes are in flight

## Hardware Components

### Button Inputs (3 total)
- [x] 3x Illuminated momentary push buttons (with integrated LEDs)

All buttons are connected via MCP23017 I/O expander (Board 0) using the `ModuleIO` abstraction layer.

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

### Current Pin Mapping (in `module_io.h`)

```cpp
namespace NukeModule {
  // Button inputs
  constexpr PinMap BTN_ATOM  = {0, 1};   // Board 0, Pin 1
  constexpr PinMap BTN_HYDRO = {0, 2};   // Board 0, Pin 2
  constexpr PinMap BTN_MIRV  = {0, 3};   // Board 0, Pin 3
  
  // LED outputs
  constexpr PinMap LED_ATOM  = {0, 8};   // Board 0, Pin 8
  constexpr PinMap LED_HYDRO = {0, 9};   // Board 0, Pin 9
  constexpr PinMap LED_MIRV  = {0, 10};  // Board 0, Pin 10
}
```

### Communication Protocol
Main controller uses I2C via MCP23017:
- **Read buttons**: Poll GPIO pins every 50ms (debouncing)
- **Write LEDs**: Direct GPIO output control with blink patterns

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
3. **Duration**: 10 seconds (configurable)
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

## Game State Mapping

This module doesn't consume game state directly, but sends commands to the game and receives confirmation events.

### Protocol Integration

#### Outgoing Messages (to Server)

When a button is pressed, send an `event` message:

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

#### Incoming Messages (from Server)

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

## Module Behavior

### On Button Press
1. User presses one of the three nuke buttons (Atom/Hydro/MIRV)
2. Hardware detects button state change (via MCP23017 GPIO)
3. Debounce input (50ms software debounce)
4. Send `event` message via WebSocket with corresponding event type
5. Message flows: Hardware → Server → Userscript
6. Userscript calls game's nuke method for the specified type

### On Nuke Launch Confirmation
1. Server successfully processes nuke launch
2. Server sends `event` message back with launch confirmation
3. Message flows: Server → Hardware
4. Hardware receives event and triggers LED blink for that specific button
5. Corresponding button LED blinks for 10 seconds (configurable)
6. LED returns to idle state (off)
7. Multiple buttons can be blinking simultaneously if multiple nukes were sent

### LED Blink Pattern
- **Duration**: 10 seconds per button (configurable)
- **Pattern**: 500ms on, 500ms off (1Hz blink)
- **Trigger**: Receipt of launch event matching button type
- **Independence**: Each button has its own blink timer and state

## UI Component Mockup

Vue component path: `ots-simulator/app/components/hardware/NukeModule.vue`

### Props
```typescript
interface NukeModuleProps {
  connected: boolean
  blinkingButtons: {
    atom: boolean
    hydro: boolean
    mirv: boolean
  }
}
```

### Emits
```typescript
interface NukeModuleEmits {
  sendNuke: (nukeType: 'atom' | 'hydro' | 'mirv') => void
}
```

### Visual Design
Three large illuminated buttons in a row or triangle formation:
- [🔴 ATOM] - Red button with glow effect when blinking
- [🔴 HYDRO] - Red button with glow effect when blinking
- [🔴 MIRV] - Red button with glow effect when blinking

Each button can blink independently. When `blinkingButtons.atom` is true, the atom button LED blinks. Same for hydro and mirv. Multiple buttons can blink simultaneously.

## Firmware Implementation

### Header
`ots-fw-main/include/module_io.h` (pin definitions)

### Source
`ots-fw-main/src/main.cpp` (button polling and LED updates)

Key functions:
- `pollButtons()`: Poll GPIO every 50ms, debounce, send WebSocket event messages
- `handleLaunchEvent()`: Process nuke launch events from server
- `updateNukeModule()`: Handle blink patterns for all three LEDs independently, update GPIO output

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

## Configuration

### Timing Constants

```cpp
#define NUKE_BLINK_DURATION_MS  10000  // 10 seconds
#define NUKE_BLINK_INTERVAL_MS  500    // 500ms (1Hz blink)
#define BUTTON_POLL_INTERVAL_MS 50     // 50ms debounce
```

## Notes
- Button debouncing implemented in firmware (50ms window)
- Each LED has independent blink timer and state (non-blocking)
- Multiple buttons can blink simultaneously
- Blink duration configurable per button (default: 10 seconds)
- Uses WebSocket protocol defined in `/prompts/protocol-context.md`
- LEDs only respond to server events, not direct button presses
- No cooldown restriction - rapid fire allowed
- Module shares MCP23017 Board 0 with Main Power Module

---

**Module Type:** Input/Output (buttons + LEDs)  
**I/O Boards:** MCP23017 Board 0 (shared with Main Power Module)  
**Button Count:** 3 (ATOM, HYDRO, MIRV)  
**LED Count:** 3 (corresponding to each button)  
**Timing:** Non-blocking, millis()-based  
**Cooldown:** None (rapid fire allowed)  
**LED Trigger:** Server events only (not local button presses)

