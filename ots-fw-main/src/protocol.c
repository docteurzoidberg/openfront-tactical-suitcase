#include "protocol.h"
#include <string.h>

const char* event_type_to_string(game_event_type_t type) {
    switch (type) {
        case GAME_EVENT_INFO: return "INFO";
        case GAME_EVENT_ERROR: return "ERROR";
        case GAME_EVENT_GAME_SPAWNING: return "GAME_SPAWNING";
        case GAME_EVENT_GAME_START: return "GAME_START";
        case GAME_EVENT_GAME_END: return "GAME_END";
        case GAME_EVENT_SOUND_PLAY: return "SOUND_PLAY";
        case GAME_EVENT_HARDWARE_DIAGNOSTIC: return "HARDWARE_DIAGNOSTIC";
        case GAME_EVENT_NUKE_LAUNCHED: return "NUKE_LAUNCHED";
        case GAME_EVENT_NUKE_EXPLODED: return "NUKE_EXPLODED";
        case GAME_EVENT_NUKE_INTERCEPTED: return "NUKE_INTERCEPTED";
        case GAME_EVENT_ALERT_ATOM: return "ALERT_ATOM";
        case GAME_EVENT_ALERT_HYDRO: return "ALERT_HYDRO";
        case GAME_EVENT_ALERT_MIRV: return "ALERT_MIRV";
        case GAME_EVENT_ALERT_LAND: return "ALERT_LAND";
        case GAME_EVENT_ALERT_NAVAL: return "ALERT_NAVAL";
        case GAME_EVENT_TROOP_UPDATE: return "TROOP_UPDATE";
        case GAME_EVENT_HARDWARE_TEST: return "HARDWARE_TEST";
        case INTERNAL_EVENT_NETWORK_CONNECTED: return "INTERNAL:NET_CONNECTED";
        case INTERNAL_EVENT_NETWORK_DISCONNECTED: return "INTERNAL:NET_DISCONNECTED";
        case INTERNAL_EVENT_WS_CONNECTED: return "INTERNAL:WS_CONNECTED";
        case INTERNAL_EVENT_WS_DISCONNECTED: return "INTERNAL:WS_DISCONNECTED";
        case INTERNAL_EVENT_WS_ERROR: return "INTERNAL:WS_ERROR";
        case INTERNAL_EVENT_BUTTON_PRESSED: return "INTERNAL:BUTTON_PRESSED";
        default: return "INVALID";
    }
}

game_event_type_t string_to_event_type(const char *str) {
    if (!str) return GAME_EVENT_INVALID;
    
    if (strcmp(str, "INFO") == 0) return GAME_EVENT_INFO;
    if (strcmp(str, "ERROR") == 0) return GAME_EVENT_ERROR;
    if (strcmp(str, "GAME_SPAWNING") == 0) return GAME_EVENT_GAME_SPAWNING;
    if (strcmp(str, "GAME_START") == 0) return GAME_EVENT_GAME_START;
    if (strcmp(str, "GAME_END") == 0) return GAME_EVENT_GAME_END;
    if (strcmp(str, "SOUND_PLAY") == 0) return GAME_EVENT_SOUND_PLAY;
    if (strcmp(str, "HARDWARE_DIAGNOSTIC") == 0) return GAME_EVENT_HARDWARE_DIAGNOSTIC;
    if (strcmp(str, "NUKE_LAUNCHED") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    if (strcmp(str, "NUKE_EXPLODED") == 0) return GAME_EVENT_NUKE_EXPLODED;
    if (strcmp(str, "NUKE_INTERCEPTED") == 0) return GAME_EVENT_NUKE_INTERCEPTED;
    if (strcmp(str, "ALERT_ATOM") == 0) return GAME_EVENT_ALERT_ATOM;
    if (strcmp(str, "ALERT_HYDRO") == 0) return GAME_EVENT_ALERT_HYDRO;
    if (strcmp(str, "ALERT_MIRV") == 0) return GAME_EVENT_ALERT_MIRV;
    if (strcmp(str, "ALERT_LAND") == 0) return GAME_EVENT_ALERT_LAND;
    if (strcmp(str, "ALERT_NAVAL") == 0) return GAME_EVENT_ALERT_NAVAL;
    if (strcmp(str, "TROOP_UPDATE") == 0) return GAME_EVENT_TROOP_UPDATE;
    if (strcmp(str, "HARDWARE_TEST") == 0) return GAME_EVENT_HARDWARE_TEST;
    
    // Handle nuke type shortcuts (for send-nuke command) - all map to NUKE_LAUNCHED
    if (strcmp(str, "atom") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    if (strcmp(str, "hydro") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    if (strcmp(str, "mirv") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    
    return GAME_EVENT_INVALID;
}
