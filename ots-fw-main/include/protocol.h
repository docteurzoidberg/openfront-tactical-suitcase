#pragma once

#include <ArduinoJson.h>

// C++ representation of the shared OTS protocol.
// This must stay in sync with `protocol-context.md` and
// `ots-shared/src/game.ts`.

struct ModuleGeneralState {
  bool m_link;
};

struct ModuleAlertState {
  bool m_alert_warning;
  bool m_alert_atom;
  bool m_alert_hydro;
  bool m_alert_mirv;
  bool m_alert_land;
  bool m_alert_naval;
}; 

struct ModuleNukeState {
  bool m_nuke_launched;
  bool m_hydro_launched;
  bool m_mirv_launched;
};

struct HWState {
  ModuleGeneralState m_general;
  ModuleAlertState m_alert;
  ModuleNukeState m_nuke;
};

struct GameState {
  uint64_t timestamp;
  String mapName;
  String mode;
  size_t playerCount;
  HWState hwState;
};

enum class GameEventType { 
  INFO,
  GAME_START,
  GAME_END,
  WIN,
  LOOSE, 
  NUKE_LAUNCHED, 
  HYDRO_LAUNCHED, 
  MIRV_LAUNCHED, 
  ALERT_ATOM,
  ALERT_HYDRO,
  ALERT_MIRV,
  ALERT_LAND,
  ALERT_NAVAL,
  HARDWARE_TEST
};

struct GameEvent {
  GameEventType type;
  uint64_t timestamp;
};

// Serialize a minimal demo GameState into a JSON document.
void serializeGameState(const GameState &state, JsonDocument &doc);

// Serialize a minimal GameEvent into a JSON document.
void serializeGameEvent(const GameEvent &event, JsonDocument &doc);

// Parse received GameEvent from WebSocket (JSON string)
// Returns true if parsing succeeded
bool parseGameEvent(const String &jsonString, GameEvent &event);
