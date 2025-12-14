#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "protocol.h"
#include "ws_client.h"
#include "io_expander.h"
#include "module_io.h"

OtsWsClient otsWs;
IOExpander io;
ModuleIO moduleIO(io);

// Generic LED blink state structure
struct LEDBlinkState {
  bool active = false;              // Is LED currently blinking?
  unsigned long startTime = 0;      // When blinking started (millis)
  unsigned long duration = 0;       // How long to blink (milliseconds)
  unsigned long blinkInterval = 0;  // Blink on/off interval (milliseconds)
  bool ledState = false;            // Current LED state (on/off)
  unsigned long lastToggle = 0;     // Last blink toggle time
};

// Main module state tracking
struct MainModuleState {
  bool lastConnectionState = false;
};
MainModuleState mainState;

// Nuke module state tracking
struct NukeModuleState {
  LEDBlinkState atom;
  LEDBlinkState hydro;
  LEDBlinkState mirv;
};
NukeModuleState nukeState;

// Configuration for nuke LED blinking
const unsigned long NUKE_BLINK_DURATION_MS = 10000;  // 10 seconds
const unsigned long NUKE_BLINK_INTERVAL_MS = 500;    // 500ms (1Hz)

// Alert module state tracking
struct AlertModuleState {
  LEDBlinkState atom;
  LEDBlinkState hydro;
  LEDBlinkState mirv;
  LEDBlinkState land;
  LEDBlinkState naval;
  
  // WARNING LED state (computed from other alerts)
  bool warningActive = false;
  unsigned long warningLastToggle = 0;
  bool warningLedState = false;
};
AlertModuleState alertState;

// Configuration for alert LED blinking
const unsigned long ALERT_DURATION_ATOM_MS = 10000;  // 10 seconds
const unsigned long ALERT_DURATION_HYDRO_MS = 10000; // 10 seconds
const unsigned long ALERT_DURATION_MIRV_MS = 10000;  // 10 seconds
const unsigned long ALERT_DURATION_LAND_MS = 15000;  // 15 seconds
const unsigned long ALERT_DURATION_NAVAL_MS = 15000; // 15 seconds
const unsigned long ALERT_BLINK_INTERVAL_MS = 500;   // 500ms (1Hz)
const unsigned long WARNING_BLINK_INTERVAL_MS = 300; // 300ms (fast)

// Game state tracking
bool gameInProgress = false;

// Hardware test state
bool hardwareTestInProgress = false;

bool connectWifi(unsigned long timeoutMs = 30000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("[WiFi] Connecting");
  unsigned long startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > timeoutMs) {
      Serial.println();
      Serial.println("[WiFi] Connection timeout!");
      return false;
    }
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void checkWiFi() {
  static unsigned long lastCheck = 0;
  static bool wasConnected = false;
  
  // Check every 5 seconds
  if (millis() - lastCheck < 5000) return;
  lastCheck = millis();
  
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  
  // Detect disconnection
  if (wasConnected && !isConnected) {
    Serial.println("[WiFi] Connection lost! Attempting to reconnect...");
  }
  
  // Try to reconnect if disconnected
  if (!isConnected) {
    connectWifi(10000); // 10 second timeout for reconnection
  }
  
  wasConnected = isConnected;
}

void setupOTA() {
  // Hostname defaults to esp32-[MAC]
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  
  // Password for OTA updates (optional but recommended)
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  // Set OTA port (optional, defaults to 3232)
  ArduinoOTA.setPort(OTA_PORT);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    
    Serial.println("[OTA] Start updating " + type);
    
    // Turn off all LEDs during update
    moduleIO.writeMainLEDs(ModuleIO::MainLEDs{false});
    moduleIO.writeNukeLEDs(ModuleIO::NukeLEDs{false, false, false});
    if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
      moduleIO.writeAlertLEDs(ModuleIO::AlertLEDs{false, false, false, false, false, false});
    }
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    
    // Print progress every 1 second
    if (now - lastPrint > 1000 || progress == total) {
      Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
      lastPrint = now;
      
      // Blink LINK LED during update
      static bool ledState = false;
      ledState = !ledState;
      moduleIO.setLED(MainModule::LED_LINK, ledState);
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
    
    // Rapid blink LINK LED on error
    for (int i = 0; i < 10; i++) {
      moduleIO.setLED(MainModule::LED_LINK, i % 2);
      delay(100);
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");
  Serial.printf("[OTA] Hostname: %s\n", ArduinoOTA.getHostname().c_str());
  Serial.println("[OTA] Password: *** (protected)");
}

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

void updateMainModule(); // Forward declaration

// Generic LED blink update function
// Returns true if still blinking, false if timeout reached
bool updateLEDBlink(LEDBlinkState &state, const PinMap &ledPin, unsigned long currentTime) {
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

// Trigger LED blink for a specific nuke type
void triggerNukeLED(LEDBlinkState &state, const char* nukeName) {
  unsigned long now = millis();
  state.active = true;
  state.startTime = now;
  state.duration = NUKE_BLINK_DURATION_MS;
  state.blinkInterval = NUKE_BLINK_INTERVAL_MS;
  state.ledState = true;
  state.lastToggle = now;
  Serial.printf("[Nuke Module] %s launch confirmed! LED blinking for %lu seconds\n", 
                nukeName, NUKE_BLINK_DURATION_MS / 1000);
}

// Trigger LED blink for a specific alert type
void triggerAlertLED(LEDBlinkState &state, unsigned long duration, const char* alertName) {
  unsigned long now = millis();
  state.active = true;
  state.startTime = now;
  state.duration = duration;
  state.blinkInterval = ALERT_BLINK_INTERVAL_MS;
  state.ledState = true;
  state.lastToggle = now;
  Serial.printf("[Alert Module] %s alert triggered! LED blinking for %lu seconds\n", 
                alertName, duration / 1000);
}

// Reset all module LEDs (except LINK LED)
void resetAllLEDs() {
  Serial.println("[MAIN] Resetting all LEDs (except LINK)");
  
  // Clear all nuke module states
  nukeState.atom.active = false;
  nukeState.hydro.active = false;
  nukeState.mirv.active = false;
  
  // Clear all alert module states
  alertState.atom.active = false;
  alertState.hydro.active = false;
  alertState.mirv.active = false;
  alertState.land.active = false;
  alertState.naval.active = false;
  alertState.warningActive = false;
  
  // Turn off all nuke LEDs
  moduleIO.setLED(NukeModule::LED_ATOM, false);
  moduleIO.setLED(NukeModule::LED_HYDRO, false);
  moduleIO.setLED(NukeModule::LED_MIRV, false);
  
  // Turn off all alert LEDs (only if Board 1 is available)
  if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
    moduleIO.setLED(AlertModule::LED_WARNING, false);
    moduleIO.setLED(AlertModule::LED_ATOM, false);
    moduleIO.setLED(AlertModule::LED_HYDRO, false);
    moduleIO.setLED(AlertModule::LED_MIRV, false);
    moduleIO.setLED(AlertModule::LED_LAND, false);
    moduleIO.setLED(AlertModule::LED_NAVAL, false);
  }
  
  // LINK LED remains unchanged (shows connection status)
}

// Hardware test: Test all LEDs sequentially
void runHardwareTest() {
  if (hardwareTestInProgress) {
    Serial.println("[HARDWARE TEST] Test already in progress, skipping...");
    return;
  }
  
  hardwareTestInProgress = true;
  Serial.println("[HARDWARE TEST] ======= STARTING HARDWARE TEST =======");
  
  // Disable normal module updates during test
  resetAllLEDs();
  
  // Phase 1: All LEDs ON for 3 seconds
  Serial.println("[HARDWARE TEST] Phase 1: All LEDs ON for 3 seconds");
  
  // Main module
  moduleIO.setLED(MainModule::LED_LINK, true);
  
  // Nuke module
  moduleIO.setLED(NukeModule::LED_ATOM, true);
  moduleIO.setLED(NukeModule::LED_HYDRO, true);
  moduleIO.setLED(NukeModule::LED_MIRV, true);
  
  // Alert module (only if Board 1 is available)
  if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
    moduleIO.setLED(AlertModule::LED_WARNING, true);
    moduleIO.setLED(AlertModule::LED_ATOM, true);
    moduleIO.setLED(AlertModule::LED_HYDRO, true);
    moduleIO.setLED(AlertModule::LED_MIRV, true);
    moduleIO.setLED(AlertModule::LED_LAND, true);
    moduleIO.setLED(AlertModule::LED_NAVAL, true);
  }
  
  delay(3000);
  
  // Turn all OFF
  moduleIO.setLED(MainModule::LED_LINK, false);
  moduleIO.setLED(NukeModule::LED_ATOM, false);
  moduleIO.setLED(NukeModule::LED_HYDRO, false);
  moduleIO.setLED(NukeModule::LED_MIRV, false);
  
  if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
    moduleIO.setLED(AlertModule::LED_WARNING, false);
    moduleIO.setLED(AlertModule::LED_ATOM, false);
    moduleIO.setLED(AlertModule::LED_HYDRO, false);
    moduleIO.setLED(AlertModule::LED_MIRV, false);
    moduleIO.setLED(AlertModule::LED_LAND, false);
    moduleIO.setLED(AlertModule::LED_NAVAL, false);
  }
  
  delay(500);
  
  // Phase 2: Test each LED individually (1 second each)
  Serial.println("[HARDWARE TEST] Phase 2: Testing LEDs individually (1 second each)");
  
  // Main Module
  Serial.println("[HARDWARE TEST] - Main Module: LED_LINK");
  moduleIO.setLED(MainModule::LED_LINK, true);
  delay(1000);
  moduleIO.setLED(MainModule::LED_LINK, false);
  delay(200);
  
  // Nuke Module
  Serial.println("[HARDWARE TEST] - Nuke Module: LED_ATOM");
  moduleIO.setLED(NukeModule::LED_ATOM, true);
  delay(1000);
  moduleIO.setLED(NukeModule::LED_ATOM, false);
  delay(200);
  
  Serial.println("[HARDWARE TEST] - Nuke Module: LED_HYDRO");
  moduleIO.setLED(NukeModule::LED_HYDRO, true);
  delay(1000);
  moduleIO.setLED(NukeModule::LED_HYDRO, false);
  delay(200);
  
  Serial.println("[HARDWARE TEST] - Nuke Module: LED_MIRV");
  moduleIO.setLED(NukeModule::LED_MIRV, true);
  delay(1000);
  moduleIO.setLED(NukeModule::LED_MIRV, false);
  delay(200);
  
  // Alert Module (only if Board 1 is available)
  if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
    Serial.println("[HARDWARE TEST] - Alert Module: LED_WARNING");
    moduleIO.setLED(AlertModule::LED_WARNING, true);
    delay(1000);
    moduleIO.setLED(AlertModule::LED_WARNING, false);
    delay(200);
    
    Serial.println("[HARDWARE TEST] - Alert Module: LED_ATOM");
    moduleIO.setLED(AlertModule::LED_ATOM, true);
    delay(1000);
    moduleIO.setLED(AlertModule::LED_ATOM, false);
    delay(200);
    
    Serial.println("[HARDWARE TEST] - Alert Module: LED_HYDRO");
    moduleIO.setLED(AlertModule::LED_HYDRO, true);
    delay(1000);
    moduleIO.setLED(AlertModule::LED_HYDRO, false);
    delay(200);
    
    Serial.println("[HARDWARE TEST] - Alert Module: LED_MIRV");
    moduleIO.setLED(AlertModule::LED_MIRV, true);
    delay(1000);
    moduleIO.setLED(AlertModule::LED_MIRV, false);
    delay(200);
    
    Serial.println("[HARDWARE TEST] - Alert Module: LED_LAND");
    moduleIO.setLED(AlertModule::LED_LAND, true);
    delay(1000);
    moduleIO.setLED(AlertModule::LED_LAND, false);
    delay(200);
    
    Serial.println("[HARDWARE TEST] - Alert Module: LED_NAVAL");
    moduleIO.setLED(AlertModule::LED_NAVAL, true);
    delay(1000);
    moduleIO.setLED(AlertModule::LED_NAVAL, false);
    delay(200);
  }
  
  Serial.println("[HARDWARE TEST] ======= TEST COMPLETE =======");
  
  // Restore normal operation
  hardwareTestInProgress = false;
  updateMainModule(); // Restore LINK LED to correct state
}

void updateNukeModule() {
  unsigned long now = millis();
  updateLEDBlink(nukeState.atom, NukeModule::LED_ATOM, now);
  updateLEDBlink(nukeState.hydro, NukeModule::LED_HYDRO, now);
  updateLEDBlink(nukeState.mirv, NukeModule::LED_MIRV, now);
}

void updateWarningLED(bool shouldBeActive, unsigned long currentTime) {
  if (!shouldBeActive) {
    if (alertState.warningActive) {
      moduleIO.setLED(AlertModule::LED_WARNING, false);
      alertState.warningActive = false;
      Serial.println("[Alert Module] WARNING LED OFF - No active alerts");
    }
    return;
  }
  
  if (!alertState.warningActive) {
    alertState.warningActive = true;
    alertState.warningLastToggle = currentTime;
    alertState.warningLedState = true;
    Serial.println("[Alert Module] WARNING LED ON - Active alerts detected");
  }
  
  // Fast blink (300ms interval)
  if (currentTime - alertState.warningLastToggle >= WARNING_BLINK_INTERVAL_MS) {
    alertState.warningLedState = !alertState.warningLedState;
    alertState.warningLastToggle = currentTime;
    moduleIO.setLED(AlertModule::LED_WARNING, alertState.warningLedState);
  }
}

void updateAlertModule() {
  unsigned long now = millis();
  bool anyAlertActive = false;
  
  // Update each alert LED
  anyAlertActive |= updateLEDBlink(alertState.atom, AlertModule::LED_ATOM, now);
  anyAlertActive |= updateLEDBlink(alertState.hydro, AlertModule::LED_HYDRO, now);
  anyAlertActive |= updateLEDBlink(alertState.mirv, AlertModule::LED_MIRV, now);
  anyAlertActive |= updateLEDBlink(alertState.land, AlertModule::LED_LAND, now);
  anyAlertActive |= updateLEDBlink(alertState.naval, AlertModule::LED_NAVAL, now);
  
  // Update WARNING LED (fast blink when any alert active)
  updateWarningLED(anyAlertActive, now);
}

void updateMainModule() {
  // Check both WiFi and WebSocket status
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool wsConnected = otsWs.isConnected();
  bool fullyConnected = wifiConnected && wsConnected;
  
  // Update LINK LED if state changed
  if (fullyConnected != mainState.lastConnectionState) {
    moduleIO.setLED(MainModule::LED_LINK, fullyConnected);
    mainState.lastConnectionState = fullyConnected;
    
    if (!wifiConnected) {
      Serial.println("[Main Module] LINK LED OFF - WiFi disconnected");
    } else if (!wsConnected) {
      Serial.println("[Main Module] LINK LED OFF - WebSocket disconnected");
    } else {
      Serial.println("[Main Module] LINK LED ON - Fully connected");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize I2C bus
  Serial.println("[MAIN] Initializing I2C bus...");
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
  
  // Initialize I/O expanders
  if (!io.begin(MCP23017_ADDRESSES, MCP23017_COUNT)) {
    Serial.println("[MAIN] ERROR: Failed to initialize I/O expanders!");
    Serial.println("[MAIN] Check I2C connections and addresses");
  } else {
    Serial.printf("[MAIN] %d I/O expander board(s) ready\n", io.getBoardCount());
    // Configure module pins
    moduleIO.begin();
  }
  
  if (connectWifi()) {
    setupOTA();
    
    // Register nuke launch event handlers
    otsWs.setNukeLaunchedCallback([](GameEventType type) {
      switch (type) {
        case GameEventType::NUKE_LAUNCHED:
          triggerNukeLED(nukeState.atom, "ATOM");
          break;
        case GameEventType::HYDRO_LAUNCHED:
          triggerNukeLED(nukeState.hydro, "HYDRO");
          break;
        case GameEventType::MIRV_LAUNCHED:
          triggerNukeLED(nukeState.mirv, "MIRV");
          break;
        default:
          break;
      }
    });
    
    // Register alert event handlers
    otsWs.setAlertCallback([](GameEventType type) {
      switch (type) {
        case GameEventType::NUKE_ALERT:
          triggerAlertLED(alertState.atom, ALERT_DURATION_ATOM_MS, "NUKE");
          break;
        case GameEventType::HYDRO_ALERT:
          triggerAlertLED(alertState.hydro, ALERT_DURATION_HYDRO_MS, "HYDRO");
          break;
        case GameEventType::MIRV_ALERT:
          triggerAlertLED(alertState.mirv, ALERT_DURATION_MIRV_MS, "MIRV");
          break;
        case GameEventType::LAND_ALERT:
          triggerAlertLED(alertState.land, ALERT_DURATION_LAND_MS, "LAND");
          break;
        case GameEventType::NAVAL_ALERT:
          triggerAlertLED(alertState.naval, ALERT_DURATION_NAVAL_MS, "NAVAL");
          break;
        default:
          break;
      }
    });
    
    // Register game state event handlers
    otsWs.setGameStateCallback([](GameEventType type) {
      switch (type) {
        case GameEventType::GAME_START:
          Serial.println("[MAIN] *** GAME STARTED ***");
          gameInProgress = true;
          resetAllLEDs();
          break;
          
        case GameEventType::GAME_END:
        case GameEventType::WIN:
        case GameEventType::LOOSE:
          if (type == GameEventType::GAME_END) {
            Serial.println("[MAIN] *** GAME ENDED ***");
          } else if (type == GameEventType::WIN) {
            Serial.println("[MAIN] *** GAME WON! ***");
          } else {
            Serial.println("[MAIN] *** GAME LOST ***");
          }
          gameInProgress = false;
          resetAllLEDs();
          break;
          
        case GameEventType::HARDWARE_TEST:
          Serial.println("[MAIN] *** HARDWARE TEST REQUESTED ***");
          runHardwareTest();
          break;
          
        default:
          break;
      }
    });
    
    otsWs.begin();
  } else {
    Serial.println("[MAIN] WARNING: WiFi connection failed, OTA and WebSocket not available");
  }
}

void loop() {
  ArduinoOTA.handle();
  checkWiFi();
  otsWs.loop();
  
  // Check for serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.equalsIgnoreCase("test") || cmd.equalsIgnoreCase("hwtest")) {
      runHardwareTest();
    } else if (cmd.equalsIgnoreCase("help")) {
      Serial.println("\n=== Available Commands ===");
      Serial.println("test / hwtest - Run hardware LED test");
      Serial.println("help          - Show this help message");
      Serial.println("=========================\n");
    } else if (cmd.length() > 0) {
      Serial.printf("Unknown command: %s (type 'help' for commands)\n", cmd.c_str());
    }
  }
  
  if (!hardwareTestInProgress) {
    updateMainModule();
    updateNukeModule();
    updateAlertModule();
    pollButtons();
  }
}
