#include "ws_client.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

void OtsWsClient::begin() {
  setupHandlers();
  server.addHandler(&ws);
  server.begin();
  Serial.println("[WS] Server started on port 80");
}

void OtsWsClient::setupHandlers() {
  ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client connected: %u\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client disconnected: %u\n", client->id());
      break;
    case WS_EVT_DATA: {
      String msg;
      for (size_t i = 0; i < len; i++)
        msg += static_cast<char>(data[i]);
      
      // Parse incoming event
      GameEvent event;
      if (!parseGameEvent(msg, event)) {
        Serial.println("[WS] Failed to parse event");
        break;
      }
      
      Serial.printf("[WS] Event: %d (timestamp: %llu)\n",
                   static_cast<int>(event.type),
                   event.timestamp);
      
      // Handle specific event types
      switch (event.type) {
        case GameEventType::NUKE_LAUNCHED:
        case GameEventType::HYDRO_LAUNCHED:
        case GameEventType::MIRV_LAUNCHED:
          Serial.println("[WS] Nuke launched event received");
          if (onNukeLaunched) {
            onNukeLaunched(event.type);
          }
          break;
          
        case GameEventType::NUKE_ALERT:
        case GameEventType::HYDRO_ALERT:
        case GameEventType::MIRV_ALERT:
        case GameEventType::LAND_ALERT:
        case GameEventType::NAVAL_ALERT:
          Serial.println("[WS] Alert event received");
          if (onAlert) {
            onAlert(event.type);
          }
          break;
          
        case GameEventType::GAME_START:
        case GameEventType::GAME_END:
        case GameEventType::WIN:
        case GameEventType::LOOSE:
          Serial.println("[WS] Game state event received");
          if (onGameState) {
            onGameState(event.type);
          }
          break;
          
        case GameEventType::HARDWARE_TEST:
          Serial.println("[WS] Hardware test event received");
          if (onGameState) {
            onGameState(event.type);
          }
          break;
          
        case GameEventType::INFO:
        default:
          // Info or unknown events - log only
          break;
      }
      break;
    }
    default:
      break;
    }
  });
}

void OtsWsClient::loop() {
  // ESPAsyncWebServer is fully async; no periodic loop required for server.
}

bool OtsWsClient::isConnected() const {
  // Check if any WebSocket clients are connected
  return ws.count() > 0;
}

void OtsWsClient::sendGameState(const GameState &state) {
  JsonDocument doc;
  serializeGameState(state, doc);
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void OtsWsClient::sendGameEvent(const GameEvent &event) {
  JsonDocument doc;
  serializeGameEvent(event, doc);
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}
