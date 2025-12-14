#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

typedef void (*ws_event_callback_t)(game_event_type_t event_type);

esp_err_t ws_client_init(const char *server_url);
esp_err_t ws_client_start(void);
void ws_client_stop(void);
esp_err_t ws_client_send_state(const game_state_t *state);
esp_err_t ws_client_send_event(const game_event_t *event);
bool ws_client_is_connected(void);
void ws_client_set_nuke_callback(ws_event_callback_t callback);
void ws_client_set_alert_callback(ws_event_callback_t callback);
void ws_client_set_game_state_callback(ws_event_callback_t callback);

#endif // WS_CLIENT_H
