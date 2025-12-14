#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// Game event types - must stay in sync with protocol-context.md and ots-shared/src/game.ts
typedef enum {
    GAME_EVENT_INFO = 0,
    GAME_EVENT_GAME_START,
    GAME_EVENT_GAME_END,
    GAME_EVENT_WIN,
    GAME_EVENT_LOOSE,
    GAME_EVENT_NUKE_LAUNCHED,
    GAME_EVENT_HYDRO_LAUNCHED,
    GAME_EVENT_MIRV_LAUNCHED,
    GAME_EVENT_ALERT_ATOM,
    GAME_EVENT_ALERT_HYDRO,
    GAME_EVENT_ALERT_MIRV,
    GAME_EVENT_ALERT_LAND,
    GAME_EVENT_ALERT_NAVAL,
    GAME_EVENT_HARDWARE_TEST,
    GAME_EVENT_INVALID
} game_event_type_t;

// Module states
typedef struct {
    bool link;
} module_general_state_t;

typedef struct {
    bool warning;
    bool atom;
    bool hydro;
    bool mirv;
    bool land;
    bool naval;
} module_alert_state_t;

typedef struct {
    bool nuke_launched;
    bool hydro_launched;
    bool mirv_launched;
} module_nuke_state_t;

typedef struct {
    module_general_state_t general;
    module_alert_state_t alert;
    module_nuke_state_t nuke;
} hw_state_t;

typedef struct {
    uint64_t timestamp;
    char map_name[64];
    char mode[32];
    uint32_t player_count;
    hw_state_t hw_state;
} game_state_t;

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
