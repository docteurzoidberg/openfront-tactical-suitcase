# OTS Firmware Main - Improvement Recommendations

## Critical Issues (High Priority)

### 1. ✅ Missing State Parsing Function (NOT NEEDED)
**Status**: Game server doesn't store or send state updates - only events are used. Event-driven architecture is sufficient for current implementation.

---

### 2. No Error Recovery Mechanisms
**Current Problem**: System continues running even if critical components fail.

**Impact**:
- I/O expander failure = no button/LED control but firmware runs
- WiFi failure = no connectivity but no retry
- Silent failures, hard to debug

**Recommended Fixes**:
1. **Retry I/O expander initialization** with exponential backoff
2. ✅ **WiFi auto-reconnect** in main loop (IMPLEMENTED)
3. **Watchdog timer** to recover from crashes
4. **Health check system** with status reporting

---

## Important Issues (Medium Priority)

### 3. ✅ Deprecated ArduinoJson API (FIXED)
**Status**: Replaced deprecated `containsKey()` calls with ArduinoJson v7 compatible syntax:
- Changed `doc.containsKey("type")` to `doc["type"].is<const char*>()`
- Changed `doc.containsKey("payload")` to `doc["payload"].is<JsonObject>()`
- Build now compiles without deprecation warnings

---

### 4. WebSocket Event Handler Inefficiency (legacy client)
**Status**: Obsolete. The firmware now hosts a WebSocket server (`ws_server.c` + `ws_protocol.c`); the old client-side handler no longer exists.

---

### 5. ✅ Nuke Module Event Handling (IMPLEMENTED)
**Status**: Button commands send events to server, WebSocket callbacks trigger LED blinking for all three nuke types (ATOM, HYDRO, MIRV). Generic LED blinking logic implemented with 10-second duration at 1Hz.

---

### 6. ✅ Game State Event Handling (IMPLEMENTED)
**Status**: Game start/end event tracking fully implemented.
- Added GAME_START and GAME_END event types to protocol
- All LEDs (except LINK) reset when game starts or ends
- Tracks game in progress state
- Handles WIN/LOOSE events same as GAME_END
- Clears all nuke and alert LED states on game end

---

### 7. ✅ LINK LED Shows WiFi + WebSocket Status (IMPLEMENTED)
**Status**: The LINK LED now correctly reflects both WiFi and WebSocket connection status in `updateMainModule()`.

---

## Nice-to-Have Improvements (Low Priority)

### 8. Add Watchdog Timer
**Benefit**: Auto-recovery from crashes and hangs.

```cpp
#include <esp_task_wdt.h>

void setup() {
  // Enable watchdog with 30 second timeout
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  
  // ... rest of setup
}

void loop() {
  esp_task_wdt_reset(); // Feed watchdog
  
  // ... rest of loop
}
```

---

### 9. ✅ Configuration Issues in platformio.ini (FIXED)
**Status**: Build flags moved to correct section in platformio.ini.

---

### 10. Add Telemetry and Monitoring
**Benefit**: Debug production issues, monitor health.

```cpp
void printStats() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 60000) return; // Every 60s
  lastPrint = millis();
  
  Serial.printf("[Stats] Heap: %d / %d bytes free\n", 
                ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("[Stats] Uptime: %lu seconds\n", millis() / 1000);
  Serial.printf("[Stats] WiFi RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("[Stats] WS Clients: %d\n", otsWs.clientCount());
}
```

---

### 11. ✅ OTA Update Support (IMPLEMENTED)
**Status**: Full ArduinoOTA implementation with authentication, LED feedback, and error handling. See `docs/OTA_GUIDE.md` for details.

---

### 12. Web-Based Configuration
**Benefit**: Configure WiFi/settings without reflashing.

**Options**:
- WiFiManager library for captive portal
- ESPAsyncWebServer config page
- Store settings in SPIFFS/LittleFS

---

### 13. ✅ Alert Module Implementation (IMPLEMENTED)
**Status**: Alert module blinking logic fully implemented with event handling.
- All 5 alert LEDs (ATOM, HYDRO, MIRV, LAND, NAVAL) blink on corresponding events
- WARNING LED auto-activates with fast blink (300ms) when any alert is active
- Individual timeout durations: 10s for nuke alerts, 15s for invasion alerts
- Multiple simultaneous alerts supported
- Generic LED blink logic reused from nuke module

---

### 14. Interrupt-Based Button Detection
**Current**: Polling every 50ms.

**Alternative**: Use MCP23017 INT pin for interrupt-driven detection.

**Benefits**:
- Lower power consumption
- Faster response time
- Less CPU usage

**Trade-off**: More complex debouncing, needs hardware INT pin connection.

---

## Priority Recommendations

### Immediate (Do First)
1. 🟡 **Add watchdog timer** - System stability and auto-recovery (quick win)
2. 🟡 **Better error recovery for I/O expanders** - Hardware fault tolerance

### Short Term (Next Sprint)
3. 🟡 **Add telemetry/monitoring** - Debug production issues

### Medium Term (When Time Permits)
5. 🟢 **Improve WebSocket message handling** - Preallocate buffers, avoid fragmentation
6. 🟢 **Web-based configuration** - WiFi setup without reflashing
7. 🟢 **Non-blocking WiFi connection** - Replace delay() in connectWifi()

### Long Term (Future Enhancements)
8. 🔵 **Interrupt-based button detection** - Lower power, faster response

### ✅ Completed
- WiFi blocking with timeout (non-blocking with retry)
- WiFi auto-reconnection (5-second monitoring)
- LINK LED shows WiFi + WebSocket status
- Fix platformio.ini build flags
- OTA update support (ArduinoOTA with authentication)
- Command sending for buttons (all three nuke types)
- Nuke module LED blinking with generic reusable logic
- WebSocket event callbacks for nuke launches
- Fix deprecated ArduinoJson API (containsKey → is<T>)
- Alert module LED blinking with WARNING LED auto-control
- Game state event handling (GAME_START, GAME_END, WIN, LOOSE)
- Reset all LEDs (except LINK) on game end

---

## Code Quality Checklist

- [ ] No blocking delays in main loop
- [ ] All errors logged and handled
- [ ] All TODOs implemented or removed
- [ ] No memory leaks (String management)
- [ ] Watchdog timer enabled
- [ ] WiFi auto-reconnect working
- [ ] All protocol message types handled
- [ ] Unit tests for critical functions
- [ ] Documentation up-to-date
- [ ] Build with no warnings

---

**Last Updated**: December 10, 2025
**Priority**: Address Critical issues first, then work down the list
