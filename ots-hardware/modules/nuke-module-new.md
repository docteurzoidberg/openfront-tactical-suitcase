# Module: Nuke Control Panel

## Overview
- **Module Name**: Nuke Control Panel
- **Size**: 16U 
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

All buttons are connected via MCP23017 I/O expander (Board 0) using the ModuleIO abstraction layer.

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

### Current Pin Mapping (in module_io.h)

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

