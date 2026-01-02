#ifndef WS_HANDLERS_H
#define WS_HANDLERS_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ws_handlers.h
 * @brief WebSocket Communication Handlers
 * 
 * Provides WebSocket endpoint for real-time game communication:
 * - WebSocket frame handling (text/binary/ping/pong)
 * - Client connection tracking (UI and userscript clients)
 * - Game event protocol (JSON parsing/serialization)
 * - Bidirectional messaging
 * 
 * This component is independent of webapp UI and configuration logic.
 */

/**
 * WebSocket connection callback
 * Called when connection state changes
 * 
 * @param connected true if connected, false if disconnected
 */
typedef void (*ws_connection_callback_t)(bool connected);

/**
 * Register WebSocket handlers with HTTP server
 * 
 * Registers the /ws WebSocket endpoint.
 * Must be called after HTTP server is started.
 * 
 * @param server HTTP server handle from http_server_get_handle()
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws_handlers_register(httpd_handle_t server);

/**
 * Set connection state change callback
 * 
 * @param cb Callback function (NULL to disable)
 */
void ws_handlers_set_connection_callback(ws_connection_callback_t cb);

/**
 * Check if userscript client is connected
 * 
 * @return true if at least one userscript client connected
 */
bool ws_handlers_has_userscript(void);

/**
 * Check if any client is connected
 * 
 * @return true if at least one client connected
 */
bool ws_handlers_is_connected(void);

/**
 * Get total client count
 * 
 * @return Number of connected clients
 */
int ws_handlers_get_client_count(void);

/**
 * Send text message to all clients
 * 
 * @param data Text data to send
 * @param len Data length
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws_handlers_send_text(const char *data, size_t len);

/**
 * Send game event to all clients
 * 
 * @param event Game event to send
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws_handlers_send_event(const game_event_t *event);

/**
 * Broadcast text to all clients asynchronously
 * 
 * @param data Text data to send (will be copied)
 * @param len Data length
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws_handlers_broadcast_text(const char *data, size_t len);

/**
 * @brief Get session close callback function for HTTP server config
 * 
 * This callback is used by the HTTP server to clean up client state when
 * a WebSocket connection closes abruptly (without CLOSE frame).
 * 
 * @return Function pointer for httpd_config_t.close_fn
 */
httpd_close_func ws_handlers_get_session_close_callback(void);

#ifdef __cplusplus
}
#endif

#endif // WS_HANDLERS_H
