# Module: Main Power Module

## Overview
- **Module Name**: Main Power Module
- **Size**: 8U (Standard height - Half module)
- **Game State Domain**: System status indication
- **Hardware Interface**: Direct I2C via MCP23017 I/O Expander (Board 0) + Hardware power switch

## Description
The **Main Power Module** is the central power distribution and status module for the OpenFront Tactical Suitcase (OTS). It provides power switching, visual power indication, and real-time WebSocket connection status feedback.

## Purpose

The Main Power Module serves as:
- **Power Distribution Hub**: Controls 12V power to all modules in the suitcase
- **Connection Status Indicator**: Displays real-time WebSocket connection status via LINK LED
- **Power Indication**: Shows when the suitcase is powered on

## Hardware Components

### Physical Switches
- [x] 1x Global power switch (SPST)

#### Global Power Switch
- **Type**: Physical on/off switch (SPST or similar)
- **Function**: Electrically provides 12V power to the entire suitcase
- **Location**: Main power input circuit
- **Behavior**: When ON, provides 12V to all modules; when OFF, entire system is unpowered
- **Firmware Interaction**: **NONE** - This is a hardware-only switch, no GPIO monitoring required

**Note:** The power switch is NOT monitored by firmware because when it's OFF, the entire system (including the ESP32) is unpowered. The firmware only runs when the switch is ON.

### LED Indicators (2 total)
- [x] 1x POWER LED (hardware-controlled)
- [x] 1x LINK LED (firmware-controlled)

The module has two LED indicators:

| LED | Purpose | Connection Type | Firmware Control |
|-----|---------|-----------------|------------------|
| **POWER LED** | Shows suitcase is powered | Directly connected to 12V rail | **NO** - Hardware only |
| **LINK LED** | WebSocket connection status | MCP23017 Board 0, Pin 0 | **YES** - Firmware controlled |

#### POWER LED
- **Voltage**: 12V (with appropriate current-limiting resistor)
- **Connection**: Directly to 12V power rail (after power switch)
- **Behavior**: 
  - ON when power switch is ON (12V present)
  - OFF when power switch is OFF (no power)
- **Firmware Control**: **NONE** - This is a passive indicator
- **Purpose**: Visual confirmation that the suitcase is receiving power

#### LINK LED
- **Voltage**: Controlled by MCP23017 (typically 3.3V or 5V logic)
- **Connection**: MCP23017 Board 0, Pin 0
- **Behavior**:
  - **ON (solid)**: WiFi AND WebSocket connections are active
  - **OFF**: Either WiFi or WebSocket is disconnected
  - **BLINKING**: OTA firmware update in progress
  - **RAPID BLINK (10x)**: OTA update error
- **Firmware Control**: **YES** - Actively controlled based on connection state and OTA status
- **Purpose**: Real-time status indicator for connectivity and firmware updates

### Current Pin Mapping (in `module_io.h`)

```cpp
namespace MainModule {
  constexpr PinMap LED_LINK = {0, 0};  // Board 0, Pin 0 - Link status indicator
}
```

### Communication Protocol
Main controller uses I2C via MCP23017:
- **Read operations**: None (power switch not monitored by firmware)
- **Write LED**: Direct GPIO output control for LINK LED

## Functional Behavior

### Power Switch Operation

1. **Power-Up Sequence**:
   - User turns ON the global power switch
   - 12V power is supplied to all modules
   - POWER LED illuminates immediately (hardware connection)
   - ESP32-S3 boots and starts firmware
   - Firmware initializes and attempts WebSocket connection
   - LINK LED turns ON when connection established

2. **Power-Down Sequence**:
   - User turns OFF the global power switch
   - 12V power is cut from all modules
   - POWER LED turns OFF immediately
   - Entire system powers down (no graceful shutdown)

### LINK LED Behavior

The LINK LED reflects the **real-time status** of network connectivity and firmware update operations.

#### Connection States

**State 1: Disconnected / Connecting**
- LINK LED: **OFF**
- When: 
  - Firmware is booting
  - WiFi is disconnected or connecting
  - WebSocket connection not yet established
  - Either WiFi or WebSocket connection lost
  - Server is unreachable
  - WiFi auto-reconnection in progress

**State 2: Fully Connected**
- LINK LED: **ON (solid)**
- When:
  - WiFi connected AND WebSocket connection active
  - Firmware can send/receive messages
  - Game is ready to play

**State 3: OTA Update in Progress**
- LINK LED: **BLINKING (1Hz)**
- When:
  - Over-The-Air firmware update is being uploaded
  - Progress displayed via blink pattern
  - All other LEDs turned off during update

**State 4: OTA Update Error**
- LINK LED: **RAPID BLINK** (10 times, 100ms intervals)
- When:
  - OTA authentication failed
  - OTA upload error occurred
  - After blinking, returns to normal operation

#### Update Trigger Points

The LINK LED state should be updated immediately when:
- WiFi connection is established → Check WebSocket, update LED
- WebSocket connection is successfully established → Turn LED **ON** (if WiFi also connected)
- WebSocket connection is lost/disconnected → Turn LED **OFF**
- WiFi connection is lost → Turn LED **OFF**
- Reconnection attempt succeeds → Turn LED **ON**
- OTA update starts → Begin **BLINKING**
- OTA update completes → Device reboots (LED will reflect new connection state)
- OTA update fails → **RAPID BLINK**, then return to connection-based state

## Game State Mapping

The Main Power Module does **NOT** send or receive protocol messages. It is purely a status indicator module.

- **No incoming messages** to process
- **No outgoing commands** to send
- **No game state** to track (beyond connection status)

The module simply reflects the WebSocket connection state maintained by the `OtsWsClient` class.

## Module Behavior

### On Power-Up
1. User flips power switch to ON position
2. 12V power supplied to entire system
3. POWER LED illuminates immediately (hardware)
4. ESP32-S3 boots and initializes firmware
5. Firmware attempts WiFi connection
6. Firmware attempts WebSocket connection
7. LINK LED turns ON when both WiFi and WebSocket are connected

### On Connection State Change
1. Firmware monitors WiFi and WebSocket connection status
2. When either connection is lost, LINK LED turns OFF
3. When both connections are restored, LINK LED turns ON
4. No user interaction required

### During OTA Update
1. OTA update initiated (via network)
2. LINK LED begins blinking (1Hz pattern)
3. Update progress shown via serial output
4. On success: Device reboots, LINK LED shows connection state
5. On error: LINK LED rapid blinks 10x, then returns to normal

## UI Component Mockup

Vue component path: `ots-simulator/app/components/hardware/MainPowerModule.vue`

### Props
```typescript
interface MainPowerModuleProps {
  connected: boolean  // WebSocket connection status
  otaInProgress?: boolean
  otaError?: boolean
}
```

### Emits
```typescript
// No emits - this is a status indicator module
```

### Visual Design
Two LED indicators:
- [🔴 POWER] - Red LED (always on when system powered)
- [🟢 LINK] - Green LED (on when connected, blinking during OTA)

Simple visual representation showing system power and connection status.

## Firmware Implementation

### Header
`ots-fw-main/include/module_io.h` (pin definitions)

### Source
`ots-fw-main/src/main.cpp` (connection monitoring and LED updates)

Key functions:
- `updateMainModule()`: Monitor connection status and update LINK LED
- Connection state is derived from `OtsWsClient::isConnected()`

### Data Structures

```cpp
// Simple state tracking for edge detection
struct MainModuleState {
  bool lastConnectionState = false;  // Track previous state for edge detection
};
```

### Implementation Example

```cpp
MainModuleState mainState;

void updateMainModule() {
  // Get current WebSocket connection status
  bool connected = otsWs.isConnected();
  
  // Update LINK LED if state changed
  if (connected != mainState.lastConnectionState) {
    moduleIO.setLED(MainModule::LED_LINK, connected);
    mainState.lastConnectionState = connected;
    
    Serial.printf("[Main Module] Connection %s\n", 
                  connected ? "ESTABLISHED" : "LOST");
  }
}
```

## Hardware Wiring

### POWER LED Circuit
```
12V Rail ----[Resistor]----[POWER LED]----[GND]
                ^
                |
         (Calculate resistor value based on LED specs)
         Typical: 1kΩ for 12V with 20mA LED
```

### LINK LED Circuit
```
MCP23017 (Board 0, Pin 0) ----[Resistor]----[LINK LED]----[GND]
                                   ^
                                   |
                           (Calculate for 3.3V/5V logic)
                           Typical: 220Ω for 5V with 20mA LED
```

### Power Distribution
```
[12V Power Supply]
       |
[Power Switch (SPST)]
       |
       +----[POWER LED]----[GND]
       |
       +----[DC-DC Converter to 5V]----[ESP32-S3 + MCP23017 boards]
       |
       +----[12V Rail to other modules]
```

## Over-The-Air (OTA) Firmware Updates

The Main Power Module's LINK LED provides visual feedback during OTA firmware updates.

### OTA Configuration

Defined in `include/config.h`:
```cpp
#define OTA_HOSTNAME "ots-fw-main"  // mDNS hostname
#define OTA_PASSWORD "ots2025"      // OTA password (CHANGE THIS!)
#define OTA_PORT 3232               // OTA port
```

### LINK LED Behavior During OTA

**Update Initiation:**
1. OTA upload starts
2. All module LEDs turn OFF
3. LINK LED begins BLINKING (toggled every ~1 second with progress updates)

**Update Progress:**
- LINK LED blinks to show activity
- Serial output shows progress percentage
- Update typically takes 30-60 seconds depending on firmware size

**Update Success:**
- Device automatically reboots
- LINK LED will show normal connection state after reboot
- Firmware version updated

**Update Failure:**
- LINK LED rapid blinks 10 times (100ms on/off)
- Error message shown in serial output
- Device continues normal operation with old firmware
- Possible errors: authentication failed, connection lost, insufficient space

### Performing OTA Updates

**PlatformIO:**
```bash
# Upload via network (mDNS hostname)
pio run -t upload --upload-port ots-fw-main.local

# Upload via IP address
pio run -t upload --upload-port 192.168.x.x
```

**Arduino IDE:**
1. Tools → Port → Select "ots-fw-main at 192.168.x.x"
2. Sketch → Upload
3. Enter password when prompted

## Configuration

No special configuration needed beyond pin mapping.

## Notes

### What the Firmware Does NOT Control

1. **Global Power Switch**: This is a physical switch that controls power to the entire system. When OFF, the firmware is not running and cannot monitor it.

2. **POWER LED**: This LED is directly connected to the 12V rail and automatically illuminates when power is present. The firmware does not and cannot control it.

### What the Firmware DOES Control

1. **LINK LED**: This is the ONLY component in the Main Power Module that the firmware actively controls. It reflects the real-time WebSocket connection status.

### Additional Notes

- Module shares MCP23017 Board 0 with Nuke Module
- LINK LED is the only firmware-controlled output
- Connection status derived from `OtsWsClient` class
- No protocol messages sent or received
- Simple edge-detection for state changes
- OTA updates automatically manage LINK LED during upload

---

**Module Type:** Status indicator (output-only)  
**I/O Boards:** MCP23017 Board 0 (shared with Nuke Module)  
**LED Count:** 1 controllable LED (LINK) + 1 passive LED (POWER)  
**Connection Source:** WebSocket client (`OtsWsClient`)  
**Update Frequency:** On connection state change (event-driven)
