# Troops Module Firmware Implementation Prompt

## Context

You are implementing the **Troops Module** for the OTS (OpenFront Tactical Station) firmware. This module provides real-time troop count visualization and deployment percentage control for the OpenFront.io browser game.

## Module Overview

- **Size**: 16U (full-height module)
- **Display**: 2×16 character I2C LCD
- **Input**: Potentiometer slider via I2C ADC (ADS1115)
- **Protocol**: WebSocket JSON messages (see `/protocol-context.md`)

## Hardware Components

### LCD Display
- **Model**: 2×16 I2C LCD (HD44780 + PCF8574 I2C backpack)
- **I2C Address**: 0x27 (or 0x3F)
- **Library**: LiquidCrystal_I2C
- **Connection**: Shared I2C bus (SDA/SCL from main controller)

### ADC (for slider)
- **Model**: ADS1115 (16-bit I2C ADC)
- **I2C Address**: 0x48
- **Library**: Adafruit_ADS1X15
- **Connection**: Shared I2C bus
- **Input**: Potentiometer slider on channel A0

## Display Format

### Line 1: Troop Counts
```
120K / 1.1M
```
Format: `{current} / {max}` with intelligent unit scaling

### Line 2: Deployment Percentage
```
50% (60K)
```
Format: `{percent}% ({calculated})` where calculated = current × (percent/100)

### Unit Scaling Logic
```cpp
String formatTroops(uint32_t troops) {
  if (troops >= 1000000000) {
    return String(troops / 1000000000.0, 1) + "B";
  } else if (troops >= 1000000) {
    return String(troops / 1000000.0, 1) + "M";
  } else if (troops >= 1000) {
    return String(troops / 1000.0, 1) + "K";
  } else {
    return String(troops);
  }
}
```

## Protocol Integration

### Incoming: Troop State Updates

Listen for game state messages containing troop information:

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

**Implementation**:
1. Parse JSON in WebSocket message handler
2. Extract `troops.current` and `troops.max`
3. Update module state variables
4. Refresh LCD Line 1 immediately
5. Recalculate and refresh LCD Line 2 (using current slider %)

### Outgoing: Slider Commands

When slider position changes by ≥1%, send command:

```json
{
  "type": "cmd",
  "payload": {
    "action": "set-troops-percent",
    "params": {
      "percent": 50
    }
  }
}
```

**Implementation**:
1. Poll ADC every 100ms
2. Convert ADC value (0-32767) to percentage (0-100)
3. Compare with `lastSentPercent`
4. If `abs(newPercent - lastSentPercent) >= 1`:
   - Send command via WebSocket
   - Update `lastSentPercent`
   - Refresh LCD Line 2

## Module State Structure

```cpp
struct TroopsModuleState {
  // From game state
  uint32_t currentTroops;
  uint32_t maxTroops;
  
  // From slider
  uint8_t sliderPercent;      // 0-100
  uint8_t lastSentPercent;    // For change detection
  
  // Timing
  uint32_t lastSliderRead;    // Debounce timestamp (millis)
  
  // Display cache (optional, for efficiency)
  String lastLine1;
  String lastLine2;
  bool displayDirty;
};
```

## Implementation Tasks

### 1. Module Initialization

```cpp
void troops_module_init() {
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Initialize ADC
  ads.setGain(GAIN_ONE);  // +/- 4.096V range
  ads.begin();
  
  // Initialize state
  troopsState.currentTroops = 0;
  troopsState.maxTroops = 0;
  troopsState.sliderPercent = 0;
  troopsState.lastSentPercent = 0;
  troopsState.lastSliderRead = 0;
  troopsState.displayDirty = true;
  
  // Show startup message
  lcd.setCursor(0, 0);
  lcd.print("  TROOPS MODULE ");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing  ");
  delay(1000);
  lcd.clear();
}
```

### 2. State Update Handler

```cpp
void troops_module_handle_state(const JsonObject& payload) {
  // Check if troops data exists
  if (!payload.containsKey("troops")) return;
  
  JsonObject troops = payload["troops"];
  
  // Update state
  troopsState.currentTroops = troops["current"];
  troopsState.maxTroops = troops["max"];
  troopsState.displayDirty = true;
  
  // Update display
  troops_module_update_display();
}
```

### 3. Slider Polling

```cpp
void troops_module_poll_slider() {
  uint32_t now = millis();
  
  // Debounce (100ms interval)
  if (now - troopsState.lastSliderRead < 100) return;
  troopsState.lastSliderRead = now;
  
  // Read ADC
  int16_t adc = ads.readADC_SingleEnded(0);  // Channel A0
  if (adc < 0) adc = 0;
  
  // Map to percentage (0-100)
  uint8_t newPercent = map(adc, 0, 32767, 0, 100);
  newPercent = constrain(newPercent, 0, 100);
  
  // Update state
  troopsState.sliderPercent = newPercent;
  
  // Check for ≥1% change
  if (abs(newPercent - troopsState.lastSentPercent) >= 1) {
    troops_module_send_command(newPercent);
    troopsState.lastSentPercent = newPercent;
    troopsState.displayDirty = true;
  }
  
  // Update display if needed
  if (troopsState.displayDirty) {
    troops_module_update_display();
  }
}
```

### 4. Display Update

```cpp
void troops_module_update_display() {
  if (!troopsState.displayDirty) return;
  
  // Line 1: Current / Max
  String line1 = formatTroops(troopsState.currentTroops) + " / " + 
                 formatTroops(troopsState.maxTroops);
  
  // Center or right-align (pad to 16 chars)
  while (line1.length() < 16) {
    line1 = " " + line1;  // Right-align
  }
  
  lcd.setCursor(0, 0);
  lcd.print(line1);
  
  // Line 2: Percent% (Calculated)
  uint32_t calculated = (troopsState.currentTroops * troopsState.sliderPercent) / 100;
  String line2 = String(troopsState.sliderPercent) + "% (" + 
                 formatTroops(calculated) + ")";
  
  // Center or left-align (pad to 16 chars)
  while (line2.length() < 16) {
    line2 = line2 + " ";  // Left-align
  }
  
  lcd.setCursor(0, 1);
  lcd.print(line2);
  
  troopsState.displayDirty = false;
}
```

### 5. Command Sender

```cpp
void troops_module_send_command(uint8_t percent) {
  // Build JSON command
  StaticJsonDocument<256> doc;
  doc["type"] = "cmd";
  JsonObject payload = doc.createNestedObject("payload");
  payload["action"] = "set-troops-percent";
  JsonObject params = payload.createNestedObject("params");
  params["percent"] = percent;
  
  // Send via WebSocket
  String output;
  serializeJson(doc, output);
  ws_send(output.c_str());
  
  // Debug log
  Serial.printf("[Troops] Sent: set-troops-percent %d%%\n", percent);
}
```

### 6. Main Loop Integration

```cpp
void loop() {
  // ... existing code ...
  
  // Poll troops module slider
  troops_module_poll_slider();
  
  // ... rest of loop ...
}
```

### 7. WebSocket Message Router (add to existing handler)

```cpp
void ws_handle_message(const char* data) {
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, data);
  
  if (error) {
    Serial.println("[WS] JSON parse error");
    return;
  }
  
  const char* type = doc["type"];
  
  if (strcmp(type, "state") == 0) {
    troops_module_handle_state(doc["payload"]);
    // ... other module handlers ...
  }
  // ... other message types ...
}
```

## Testing Checklist

### I2C Bus Test
- [ ] LCD detected at 0x27 (or 0x3F)
- [ ] ADC detected at 0x48
- [ ] No I2C bus errors in serial output

### Display Test
- [ ] LCD backlight turns on
- [ ] Startup message appears
- [ ] Both lines display correctly
- [ ] Characters are readable

### Slider Test
- [ ] ADC reads ~0 at 0% position
- [ ] ADC reads ~32767 at 100% position
- [ ] Slider values map correctly to 0-100%
- [ ] Commands sent only on ≥1% change

### Protocol Test
- [ ] Receives state messages from server
- [ ] Parses troops.current and troops.max correctly
- [ ] Display updates when state received
- [ ] Sends set-troops-percent commands
- [ ] Commands have correct JSON format

### Scaling Test
- [ ] Low troops (< 1K): shows raw number
- [ ] Medium troops (< 1M): shows with "K"
- [ ] High troops (≥ 1M): shows with "M"
- [ ] Very high troops (≥ 1B): shows with "B"
- [ ] Decimal precision correct (1 decimal place)

### Integration Test
- [ ] Module works alongside other modules (Alert, Nuke, etc.)
- [ ] No interference on shared I2C bus
- [ ] WebSocket messages routed correctly
- [ ] Display updates in real-time during gameplay

## Code Organization

Place implementation in:
- **Source**: `/ots-fw-main/src/module_troops.c` (or .cpp)
- **Header**: `/ots-fw-main/include/module_troops.h`
- **Integration**: Update `/ots-fw-main/src/main.c` to call troops module functions

## Dependencies

Add to `platformio.ini`:
```ini
lib_deps = 
    marcoschwartz/LiquidCrystal_I2C@^1.1.4
    adafruit/Adafruit ADS1X15@^2.4.0
    bblanchon/ArduinoJson@^6.21.0
```

## Key Considerations

1. **Debouncing**: 100ms minimum between slider reads to avoid command spam
2. **Change Threshold**: Only send commands on ≥1% difference
3. **Display Updates**: Immediate (no animations or delays)
4. **Scaling**: Use 1 decimal place for K/M/B units (e.g., "1.2M" not "1234K")
5. **Padding**: Right-align Line 1, left-align Line 2 (or adjust for readability)
6. **Error Handling**: Handle missing troops data gracefully (show "---")
7. **I2C Conflicts**: Ensure no address collisions with other modules

## Example Serial Debug Output

```
[Troops] Module initialized
[Troops] LCD detected at 0x27
[Troops] ADC detected at 0x48
[Troops] State received: 120000 / 1100000 troops
[Troops] Display updated: 120K / 1.1M
[Troops] Slider: 50% -> sending command
[Troops] Sent: set-troops-percent 50%
[Troops] Display updated: 50% (60K)
```

## References

- Main protocol: `/protocol-context.md`
- Hardware spec: `/ots-hardware/modules/troops-module.md`
- Other module implementations: `/ots-fw-main/prompts/ALERT_MODULE_PROMPT.md`
