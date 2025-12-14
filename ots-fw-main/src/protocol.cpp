#include "protocol.h"

static const char *eventTypeToString(GameEventType t) {
  switch (t) {
  case GameEventType::INFO:
    return "INFO";
  case GameEventType::GAME_START:
    return "GAME_START";
  case GameEventType::GAME_END:
    return "GAME_END";
  case GameEventType::WIN:
    return "WIN";
  case GameEventType::LOOSE:
    return "LOOSE";
  case GameEventType::NUKE_LAUNCHED:
    return "NUKE_LAUNCHED";
  case GameEventType::HYDRO_LAUNCHED:
    return "HYDRO_LAUNCHED";
  case GameEventType::MIRV_LAUNCHED:
    return "MIRV_LAUNCHED";
  case GameEventType::ALERT_ATOM:
    return "ALERT_ATOM";
  case GameEventType::ALERT_HYDRO:
    return "ALERT_HYDRO";
  case GameEventType::ALERT_MIRV:
    return "ALERT_MIRV";
  case GameEventType::ALERT_LAND:
    return "ALERT_LAND";
  case GameEventType::ALERT_NAVAL:
    return "ALERT_NAVAL";
  case GameEventType::HARDWARE_TEST:
    return "HARDWARE_TEST";
  }
  return "INFO";
}

void serializeGameState(const GameState &state, JsonDocument &doc) {
  doc.clear();
  doc["type"] = "state";
  JsonObject payload = doc["payload"].to<JsonObject>();
  payload["timestamp"] = state.timestamp;
  payload["mapName"] = state.mapName;
  payload["mode"] = state.mode;
  payload["playerCount"] = state.playerCount;

  JsonObject hwState = payload["hwState"].to<JsonObject>();
  
  JsonObject general = hwState["m_general"].to<JsonObject>();
  general["m_link"] = state.hwState.m_general.m_link;
  
  JsonObject alert = hwState["m_alert"].to<JsonObject>();
  alert["m_alert_warning"] = state.hwState.m_alert.m_alert_warning;
  alert["m_alert_atom"] = state.hwState.m_alert.m_alert_atom;
  alert["m_alert_hydro"] = state.hwState.m_alert.m_alert_hydro;
  alert["m_alert_mirv"] = state.hwState.m_alert.m_alert_mirv;
  alert["m_alert_land"] = state.hwState.m_alert.m_alert_land;
  alert["m_alert_naval"] = state.hwState.m_alert.m_alert_naval;
  
  JsonObject nuke = hwState["m_nuke"].to<JsonObject>();
  nuke["m_nuke_launched"] = state.hwState.m_nuke.m_nuke_launched;
  nuke["m_hydro_launched"] = state.hwState.m_nuke.m_hydro_launched;
  nuke["m_mirv_launched"] = state.hwState.m_nuke.m_mirv_launched;
}

void serializeGameEvent(const GameEvent &event, JsonDocument &doc) {
  doc.clear();
  doc["type"] = "event";
  JsonObject payload = doc["payload"].to<JsonObject>();
  payload["type"] = eventTypeToString(event.type);
  payload["timestamp"] = event.timestamp;
}

static GameEventType stringToEventType(const char *str) {
  if (strcmp(str, "INFO") == 0) return GameEventType::INFO;
  if (strcmp(str, "GAME_START") == 0) return GameEventType::GAME_START;
  if (strcmp(str, "GAME_END") == 0) return GameEventType::GAME_END;
  if (strcmp(str, "WIN") == 0) return GameEventType::WIN;
  if (strcmp(str, "LOOSE") == 0) return GameEventType::LOOSE;
  if (strcmp(str, "NUKE_LAUNCHED") == 0) return GameEventType::NUKE_LAUNCHED;
  if (strcmp(str, "HYDRO_LAUNCHED") == 0) return GameEventType::HYDRO_LAUNCHED;
  if (strcmp(str, "MIRV_LAUNCHED") == 0) return GameEventType::MIRV_LAUNCHED;
  if (strcmp(str, "ALERT_ATOM") == 0) return GameEventType::ALERT_ATOM;
  if (strcmp(str, "ALERT_HYDRO") == 0) return GameEventType::ALERT_HYDRO;
  if (strcmp(str, "ALERT_MIRV") == 0) return GameEventType::ALERT_MIRV;
  if (strcmp(str, "ALERT_LAND") == 0) return GameEventType::ALERT_LAND;
  if (strcmp(str, "ALERT_NAVAL") == 0) return GameEventType::ALERT_NAVAL;
  if (strcmp(str, "HARDWARE_TEST") == 0) return GameEventType::HARDWARE_TEST;
  return GameEventType::INFO;
}

bool parseGameEvent(const String &jsonString, GameEvent &event) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("[Protocol] JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  // Verify message type is "event"
  if (!doc["type"].is<const char*>()) {
    Serial.println("[Protocol] Missing 'type' field");
    return false;
  }
  
  const char *typeStr = doc["type"];
  if (strcmp(typeStr, "event") != 0) {
    Serial.printf("[Protocol] Expected 'event', got '%s'\n", typeStr);
    return false;
  }
  
  // Parse event payload
  if (!doc["payload"].is<JsonObject>()) {
    Serial.println("[Protocol] Missing 'payload' field");
    return false;
  }
  
  JsonObject payload = doc["payload"];
  
  // Extract event type
  const char *eventTypeStr = payload["type"];
  if (!eventTypeStr) {
    Serial.println("[Protocol] Missing event 'type' in payload");
    return false;
  }
  
  event.type = stringToEventType(eventTypeStr);
  event.timestamp = payload["timestamp"] | 0;
  
  return true;
}
