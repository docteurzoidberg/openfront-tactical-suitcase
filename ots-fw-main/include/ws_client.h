#pragma once

#include "protocol.h"
#include <ESPAsyncWebServer.h>
#include <functional>

// Simple WebSocket server wrapper for the OTS firmware using ESPAsyncWebServer.

class OtsWsClient {
public:
  void begin();
  void loop();

  // Send helpers
  void sendGameState(const GameState &state);
  void sendGameEvent(const GameEvent &event);
  
  // Connection status
  bool isConnected() const;
  
  // Event callbacks
  void setNukeLaunchedCallback(std::function<void(GameEventType)> callback) {
    onNukeLaunched = callback;
  }
  void setAlertCallback(std::function<void(GameEventType)> callback) {
    onAlert = callback;
  }
  void setGameStateCallback(std::function<void(GameEventType)> callback) {
    onGameState = callback;
  }

private:
  AsyncWebServer server{80};
  AsyncWebSocket ws{"/ws"};
  void setupHandlers();
  
  std::function<void(GameEventType)> onNukeLaunched;
  std::function<void(GameEventType)> onAlert;
  std::function<void(GameEventType)> onGameState;
};
