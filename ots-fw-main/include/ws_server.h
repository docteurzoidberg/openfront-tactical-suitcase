#ifndef WS_SERVER_H
#define WS_SERVER_H

#include "esp_err.h"
#include "protocol.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WebSocket connection callback
 * @param connected True if client connected, false if disconnected
 */
typedef void (*ws_connection_callback_t)(bool connected);

/**
 * @brief Initialize WebSocket server
 * @param port Port number to listen on (default: 3000)
 * @return ESP_OK on success
 */
esp_err_t ws_server_init(uint16_t port);

/**
 * @brief Start WebSocket server
 * @return ESP_OK on success
 */
esp_err_t ws_server_start(void);

/**
 * @brief Stop WebSocket server
 */
void ws_server_stop(void);

/**
 * @brief Send text data to all connected clients
 * @param data Text data to send
 * @param len Length of data
 * @return ESP_OK on success
 */
esp_err_t ws_server_send_text(const char *data, size_t len);

/**
 * @brief Send game event to all connected clients
 * @param event Game event to send
 * @return ESP_OK on success
 */
esp_err_t ws_server_send_event(const game_event_t *event);

/**
 * @brief Check if any client is connected
 * @return True if at least one client is connected
 */
bool ws_server_is_connected(void);

/**
 * @brief Check if the WebSocket server has been started (listening)
 * @return True if the server handle is initialized and running
 */
bool ws_server_is_started(void);

/**
 * @brief Check if at least one connected WebSocket client identified as a userscript
 * (via handshake {"type":"handshake","clientType":"userscript"}).
 */
bool ws_server_has_userscript(void);

/**
 * @brief Set connection callback
 * @param callback Callback function
 */
void ws_server_set_connection_callback(ws_connection_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // WS_SERVER_H
