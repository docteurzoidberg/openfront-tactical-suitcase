# Main Power Module - Project Prompt

## Overview

The **Main Power Module** is the central power distribution and status module for the OpenFront Tactical Suitcase (OTS). It provides power switching, visual power indication, and real-time WebSocket connection status feedback.

## Purpose

The Main Power Module serves as:
- **Power Distribution Hub**: Controls 12V power to all modules in the suitcase
- **Connection Status Indicator**: Displays real-time WebSocket connection status via LINK LED
- **Power Indication**: Shows when the suitcase is powered on

## Hardware Components

### Physical Switches

#### Global Power Switch
- **Type**: Physical on/off switch (SPST or similar)
- **Function**: Electrically provides 12V power to the entire suitcase
- **Location**: Main power input circuit
- **Behavior**: When ON, provides 12V to all modules; when OFF, entire system is unpowered
- **Firmware Interaction**: **NONE** - This is a hardware-only switch, no GPIO monitoring required

**Note:** The power switch is NOT monitored by firmware because when it's OFF, the entire system (including the ESP32) is unpowered. The firmware only runs when the switch is ON.

### LED Indicators

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
// Main Power Module - LED Output (Board 1 - OUTPUT pin)
#define MAIN_LED_LINK_BOARD 1
#define MAIN_LED_LINK_PIN   7
```

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

## Protocol Integration

### Connection Status Source

The LINK LED state is derived from the WebSocket client connection status, which is managed by the `OtsWsClient` class.

The firmware should:
1. Monitor the WebSocket connection state in the main loop
2. Update the LINK LED whenever the connection state changes
3. Ensure the LED accurately reflects the current connection status

### No Protocol Messages Required

The Main Power Module does **NOT** send or receive protocol messages. It is purely a status indicator module.

- **No incoming messages** to process
- **No outgoing commands** to send
- **No game state** to track (beyond connection status)

The module simply reflects the WebSocket connection state maintained by the `OtsWsClient` class.

## Software Architecture

### Core Components

1. **Connection State Monitor**
   - Polls WebSocket client connection status
   - Detects state transitions (connected ↔ disconnected)
   - Updates LINK LED accordingly

2. **LED Controller**
   - Simple on/off control via `ModuleIO::setLED()`
   - No blinking or animations required
   - Immediate updates on state changes

### Data Structures

```cpp
// No complex state structures needed
// Just track the last known connection state to detect changes

struct MainModuleState {
  bool lastConnectionState = false;  // Track previous state for edge detection
};
```

### Main Loop Integration

```cpp
MainModuleState mainState;

void updateMainModule() {
  // Get current WebSocket connection status
  bool connected = otsWs.isConnected();
  
  // Update LINK LED if state changed
  if (connected != mainState.lastConnectionState) {
    moduleIO.setLED(MainModule::LED_LINK, connected);
    mainState.lastConnectionState = connected;
    
    // Optional: Log state changes
    Serial.printf("[Main Module] Connection %s\n", 
                  connected ? "ESTABLISHED" : "LOST");
  }
}

void loop() {
  otsWs.loop();           // WebSocket client processing
  updateMainModule();     // Update LINK LED based on connection
  pollButtons();          // Other modules...
}
```

### Alternative: Event-Driven Updates

If the WebSocket client provides connection event callbacks:

```cpp
void onWebSocketConnected() {
  moduleIO.setLED(MainModule::LED_LINK, true);
  Serial.println("[Main Module] LINK LED ON - Connected to server");
}

void onWebSocketDisconnected() {
  moduleIO.setLED(MainModule::LED_LINK, false);
  Serial.println("[Main Module] LINK LED OFF - Disconnected from server");
}

// Register callbacks in setup()
void setup() {
  // ... other initialization ...
  otsWs.onConnect(onWebSocketConnected);
  otsWs.onDisconnect(onWebSocketDisconnected);
}
```

## Implementation Checklist

### Phase 1: Hardware Setup
- [x] Define `MainModule::LED_LINK` pin mapping
- [x] Add `MainLEDs` structure to `ModuleIO`
- [x] Implement `writeMainLEDs()` function
- [x] Configure LINK LED pin as output in `ModuleIO::begin()`
- [x] Initialize LINK LED to OFF on startup

### Phase 2: Connection Monitoring
- [ ] Add connection state tracking in main loop
- [ ] Detect WebSocket connection state changes
- [ ] Update LINK LED on state transitions
- [ ] Test with connect/disconnect scenarios

### Phase 3: Integration
- [ ] Verify LINK LED responds to WiFi connection
- [ ] Verify LINK LED responds to WebSocket connection
- [ ] Test behavior during server disconnects/reconnects
- [ ] Ensure LED state is accurate on startup

### Phase 4: Reliability
- [ ] Handle edge cases (rapid connect/disconnect)
- [ ] Ensure LED state persists across reconnection attempts
- [ ] Add logging for connection state changes
- [ ] Document expected behavior in code comments

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

## Configuration

### Pin Mapping

Defined in `include/module_io.h`:

```cpp
// Main Power Module - LED Output (Board 1 - OUTPUT pin)
#define MAIN_LED_LINK_BOARD 1
#define MAIN_LED_LINK_PIN   7
```

### ModuleIO Structure

Defined in `include/module_io.h`:

```cpp
struct MainLEDs {
  bool link = false;  // LINK LED state
};
```

### Usage Example

```cpp
// Turn LINK LED on
ModuleIO::MainLEDs leds;
leds.link = true;
moduleIO.writeMainLEDs(leds);

// Or use direct LED control
moduleIO.setLED(MainModule::LED_LINK, true);
```

## Testing Strategy

### Manual Tests

1. **Power-Up Test**:
   - Turn power switch OFF → Entire system should be unpowered
   - Turn power switch ON → POWER LED should illuminate immediately
   - Wait for ESP32 boot → LINK LED should turn ON when connected

2. **Connection Test**:
   - With system powered and connected → LINK LED should be ON
   - Disconnect WiFi/server → LINK LED should turn OFF
   - Reconnect → LINK LED should turn ON again

3. **Server Disconnect Test**:
   - With WiFi connected, stop the OTS server
   - LINK LED should turn OFF within a few seconds
   - Restart server → LINK LED should turn ON when reconnected

### Automated Tests

1. **Connection State Tracking**: Verify LED follows WiFi + WebSocket state
2. **State Transition**: Ensure LED updates on connection changes
3. **Edge Cases**: Test rapid connect/disconnect cycles
4. **OTA Updates**: Verify LED blinks during OTA upload

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

### OTA Security

- **Password protected**: Prevents unauthorized updates
- **WiFi network isolation**: Only accessible on local network
- **Change default password** in production environments
- Authentication failures logged to serial

### OTA Troubleshooting

**Device not found:**
- Ensure LINK LED is ON (WiFi connected)
- Check device and computer on same network
- Try IP address instead of hostname
- Verify mDNS working: `ping ots-fw-main.local`

**Upload fails:**
- Check WiFi signal strength (LED should be solid ON)
- Verify correct OTA password
- Ensure sufficient free heap memory
- Check firewall not blocking port 3232

## Troubleshooting

### POWER LED Not Illuminating
- Check 12V power supply is connected
- Verify power switch is ON
- Check resistor value and LED polarity
- Measure voltage at LED terminals (should be ~12V)

### LINK LED Always OFF
- Verify WiFi connection is working (check serial logs)
- Confirm WebSocket connection to server
- Check both WiFi AND WebSocket must be connected
- Check MCP23017 Board 0, Pin 0 is configured as output
- Test LED directly with `moduleIO.setLED(MainModule::LED_LINK, true)`
- Verify I2C bus and MCP23017 initialization

### LINK LED Always ON
- Check for stuck HIGH state on MCP23017 pin
- Verify connection state logic is correct
- Review `OtsWsClient::isConnected()` implementation
- Check for initialization issues (LED set to ON at startup)

### LINK LED Not Updating
- Verify `updateMainModule()` is called in main loop
- Check for state change detection logic
- Add debug logging to track connection state changes
- Ensure `checkWiFi()` is running (checks every 5 seconds)

### LINK LED Blinking Unexpectedly
- Check if OTA update is in progress
- Review serial logs for OTA activity
- Verify no OTA clients attempting connection
- Check for code bugs in LED control logic
- Ensure `otsWs.loop()` is being called regularly

## Future Enhancements

### Short Term
- [ ] Add slow blink pattern when connecting (vs solid OFF)
- [ ] Different patterns for WiFi vs WebSocket connection states
- [ ] Heartbeat indicator (brief flash every N seconds when connected)

### Medium Term
- [ ] Multi-color LED (green=connected, yellow=connecting, red=error)
- [ ] Error state indication (rapid blink on connection errors)
- [ ] Battery level indicator (if using battery power)

### Long Term
- [ ] OLED/LCD display for detailed status (IP address, server, etc.)
- [ ] Soft power switch with graceful shutdown support
- [ ] Power consumption monitoring and display

## Related Documentation

- **Main Firmware Context**: `copilot-project-context.md`
- **WebSocket Server**: `ws_server.h` - Connection status source
- **Hardware I/O**: See "Hardware I/O Architecture" in `copilot-project-context.md`
- **Protocol Specification**: `../protocol-context.md` (repository root)

## Important Notes

### What the Firmware Does NOT Control

1. **Global Power Switch**: This is a physical switch that controls power to the entire system. When OFF, the firmware is not running and cannot monitor it.

2. **POWER LED**: This LED is directly connected to the 12V rail and automatically illuminates when power is present. The firmware does not and cannot control it.

### What the Firmware DOES Control

1. **LINK LED**: This is the ONLY component in the Main Power Module that the firmware actively controls. It reflects the real-time WebSocket connection status.

---

**Module Type:** Status indicator (output-only)  
**I/O Boards:** MCP23017 Board 0 (shared with Nuke Module)  
**LED Count:** 1 controllable LED (LINK) + 1 passive LED (POWER)  
**Connection Source:** WebSocket client (`OtsWsClient`)  
**Update Frequency:** On connection state change (event-driven)  
**Last Updated:** December 9, 2025
