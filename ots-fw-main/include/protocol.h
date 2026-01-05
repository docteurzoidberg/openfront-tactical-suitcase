#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// Game event types - must stay in sync with WEBSOCKET_MESSAGE_SPEC.md and ots-shared/src/game.ts
typedef enum {
    GAME_EVENT_INFO = 0,
    GAME_EVENT_ERROR,
    GAME_EVENT_GAME_SPAWNING,
    GAME_EVENT_GAME_START,
    GAME_EVENT_GAME_END,
    GAME_EVENT_SOUND_PLAY,
    GAME_EVENT_HARDWARE_DIAGNOSTIC,
    GAME_EVENT_NUKE_LAUNCHED,
    GAME_EVENT_NUKE_EXPLODED,
    GAME_EVENT_NUKE_INTERCEPTED,
    GAME_EVENT_ALERT_ATOM,
    GAME_EVENT_ALERT_HYDRO,
    GAME_EVENT_ALERT_MIRV,
    GAME_EVENT_ALERT_LAND,
    GAME_EVENT_ALERT_NAVAL,
    GAME_EVENT_TROOP_UPDATE,
    GAME_EVENT_HARDWARE_TEST,
    // Internal-only events (not in protocol)
    INTERNAL_EVENT_NETWORK_CONNECTED,
    INTERNAL_EVENT_NETWORK_DISCONNECTED,
    INTERNAL_EVENT_WS_CONNECTED,
    INTERNAL_EVENT_WS_DISCONNECTED,
    INTERNAL_EVENT_WS_ERROR,
    INTERNAL_EVENT_BUTTON_PRESSED,
    GAME_EVENT_INVALID
} game_event_type_t;

// Game event structure
typedef struct {
    game_event_type_t type;
    uint64_t timestamp;
    char message[128];
    char data[256];  // JSON string for additional data
} game_event_t;

// Protocol helper functions
const char* event_type_to_string(game_event_type_t type);
game_event_type_t string_to_event_type(const char *str);

#endif // PROTOCOL_H
