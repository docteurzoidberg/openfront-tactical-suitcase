#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

/**
 * @brief Connection state callback
 * 
 * @param connected true if connected, false if disconnected
 */
typedef void (*ws_connection_callback_t)(bool connected);

/**
 * @brief Initialize WebSocket client (transport layer only)
 * 
 * @param server_url WebSocket server URL
 * @return ESP_OK on success
 */
esp_err_t ws_client_init(const char *server_url);

/**
 * @brief Start WebSocket connection
 * 
 * @return ESP_OK on success
 */
esp_err_t ws_client_start(void);

/**
 * @brief Stop WebSocket connection
 */
void ws_client_stop(void);

/**
 * @brief Send raw text message
 * 
 * @param data Text data to send
 * @param len Length of data
 * @return ESP_OK on success
 */
esp_err_t ws_client_send_text(const char *data, size_t len);

/**
 * @brief Send game event (uses protocol layer)
 * 
 * @param event Game event structure
 * @return ESP_OK on success
 */
esp_err_t ws_client_send_event(const game_event_t *event);

/**
 * @brief Check connection status
 * 
 * @return true if connected
 */
bool ws_client_is_connected(void);

/**
 * @brief Set connection state callback
 * 
 * @param callback Function to call on connection state changes
 */
void ws_client_set_connection_callback(ws_connection_callback_t callback);

#endif // WS_CLIENT_H
