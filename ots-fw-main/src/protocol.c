#include "protocol.h"
#include <string.h>

const char* event_type_to_string(game_event_type_t type) {
    switch (type) {
        case GAME_EVENT_INFO: return "INFO";
        case GAME_EVENT_GAME_START: return "GAME_START";
        case GAME_EVENT_GAME_END: return "GAME_END";
        case GAME_EVENT_WIN: return "WIN";
        case GAME_EVENT_LOOSE: return "LOOSE";
        case GAME_EVENT_NUKE_LAUNCHED: return "NUKE_LAUNCHED";
        case GAME_EVENT_HYDRO_LAUNCHED: return "HYDRO_LAUNCHED";
        case GAME_EVENT_MIRV_LAUNCHED: return "MIRV_LAUNCHED";
        case GAME_EVENT_ALERT_ATOM: return "ALERT_ATOM";
        case GAME_EVENT_ALERT_HYDRO: return "ALERT_HYDRO";
        case GAME_EVENT_ALERT_MIRV: return "ALERT_MIRV";
        case GAME_EVENT_ALERT_LAND: return "ALERT_LAND";
        case GAME_EVENT_ALERT_NAVAL: return "ALERT_NAVAL";
        case GAME_EVENT_HARDWARE_TEST: return "HARDWARE_TEST";
        default: return "INVALID";
    }
}

game_event_type_t string_to_event_type(const char *str) {
    if (!str) return GAME_EVENT_INVALID;
    
    if (strcmp(str, "INFO") == 0) return GAME_EVENT_INFO;
    if (strcmp(str, "GAME_START") == 0) return GAME_EVENT_GAME_START;
    if (strcmp(str, "GAME_END") == 0) return GAME_EVENT_GAME_END;
    if (strcmp(str, "WIN") == 0) return GAME_EVENT_WIN;
    if (strcmp(str, "LOOSE") == 0) return GAME_EVENT_LOOSE;
    if (strcmp(str, "NUKE_LAUNCHED") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    if (strcmp(str, "HYDRO_LAUNCHED") == 0) return GAME_EVENT_HYDRO_LAUNCHED;
    if (strcmp(str, "MIRV_LAUNCHED") == 0) return GAME_EVENT_MIRV_LAUNCHED;
    if (strcmp(str, "ALERT_ATOM") == 0) return GAME_EVENT_ALERT_ATOM;
    if (strcmp(str, "ALERT_HYDRO") == 0) return GAME_EVENT_ALERT_HYDRO;
    if (strcmp(str, "ALERT_MIRV") == 0) return GAME_EVENT_ALERT_MIRV;
    if (strcmp(str, "ALERT_LAND") == 0) return GAME_EVENT_ALERT_LAND;
    if (strcmp(str, "ALERT_NAVAL") == 0) return GAME_EVENT_ALERT_NAVAL;
    if (strcmp(str, "HARDWARE_TEST") == 0) return GAME_EVENT_HARDWARE_TEST;
    
    // Handle nuke type shortcuts (for send-nuke command)
    if (strcmp(str, "atom") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    if (strcmp(str, "hydro") == 0) return GAME_EVENT_HYDRO_LAUNCHED;
    if (strcmp(str, "mirv") == 0) return GAME_EVENT_MIRV_LAUNCHED;
    
    return GAME_EVENT_INVALID;
}
