#ifndef WS_PROTOCOL_H
#define WS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

/**
 * @brief WebSocket message types
 */
typedef enum {
    WS_MSG_HANDSHAKE,
    WS_MSG_EVENT,
    WS_MSG_STATE,
    WS_MSG_COMMAND,
    WS_MSG_RESPONSE,
    WS_MSG_UNKNOWN
} ws_message_type_t;

/**
 * @brief Parsed WebSocket message
 */
typedef struct {
    ws_message_type_t type;
    union {
        struct {
            char client_type[32];
        } handshake;
        
        struct {
            game_event_type_t event_type;
            uint32_t timestamp;
            char message[64];
            // TROOP_UPDATE carries JSON data; 128 bytes is too small and causes truncation.
            char data[512];
        } event;
        
        struct {
            char action[32];
            char params[128];
        } command;
    } payload;
} ws_message_t;

/**
 * @brief Initialize WebSocket protocol handler
 * 
 * @return ESP_OK on success
 */
esp_err_t ws_protocol_init(void);

/**
 * @brief Parse incoming WebSocket message
 * 
 * @param json_str JSON string to parse
 * @param len Length of JSON string
 * @param msg Output parsed message structure
 * @return ESP_OK if parsed successfully
 */
esp_err_t ws_protocol_parse(const char *json_str, size_t len, ws_message_t *msg);

/**
 * @brief Build handshake message
 * 
 * @param client_type Client type identifier
 * @param out_buffer Buffer to store JSON string
 * @param buffer_size Size of output buffer
 * @return ESP_OK on success
 */
esp_err_t ws_protocol_build_handshake(const char *client_type, char *out_buffer, size_t buffer_size);

/**
 * @brief Build event message from game_event_t
 * 
 * @param event Game event structure
 * @param out_buffer Buffer to store JSON string
 * @param buffer_size Size of output buffer
 * @return ESP_OK on success
 */
esp_err_t ws_protocol_build_event(const game_event_t *event, char *out_buffer, size_t buffer_size);

/**
 * @brief Validate JSON message format
 * 
 * @param json_str JSON string to validate
 * @param len Length of string
 * @return true if valid
 */
bool ws_protocol_validate(const char *json_str, size_t len);

#endif // WS_PROTOCOL_H
