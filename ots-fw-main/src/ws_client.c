#include "ws_client.h"
#include "ws_protocol.h"
#include "event_dispatcher.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "OTS_WS_CLIENT";

static esp_websocket_client_handle_t client = NULL;
static bool is_connected = false;
static ws_connection_callback_t connection_callback = NULL;

static void websocket_event_handler(void *handler_args, esp_event_base_t base,
                                   int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket Connected");
        is_connected = true;
        
        // Post internal event
        event_dispatcher_post_simple(INTERNAL_EVENT_WS_CONNECTED, EVENT_SOURCE_SYSTEM);
        
        // Notify connection callback
        if (connection_callback) {
            connection_callback(true);
        }
        
        // Send handshake
        char handshake[128];
        if (ws_protocol_build_handshake("firmware", handshake, sizeof(handshake)) == ESP_OK) {
            esp_websocket_client_send_text(client, handshake, strlen(handshake), portMAX_DELAY);
        }
        break;
        
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WebSocket Disconnected");
        is_connected = false;
        
        // Post internal event
        event_dispatcher_post_simple(INTERNAL_EVENT_WS_DISCONNECTED, EVENT_SOURCE_SYSTEM);
        
        // Notify connection callback
        if (connection_callback) {
            connection_callback(false);
        }
        break;
        
    case WEBSOCKET_EVENT_DATA:
        if (data->data_len > 0) {
            ESP_LOGD(TAG, "Received: %.*s", data->data_len, (char *)data->data_ptr);
            
            // Validate and parse message
            if (!ws_protocol_validate((char *)data->data_ptr, data->data_len)) {
                ESP_LOGW(TAG, "Invalid message format");
                break;
            }
            
            ws_message_t msg;
            if (ws_protocol_parse((char *)data->data_ptr, data->data_len, &msg) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to parse message");
                break;
            }
            
            // Route parsed message to event dispatcher
            if (msg.type == WS_MSG_EVENT) {
                internal_event_t event = {
                    .type = msg.payload.event.event_type,
                    .source = EVENT_SOURCE_WEBSOCKET,
                    .timestamp = msg.payload.event.timestamp
                };
                strncpy(event.message, msg.payload.event.message, sizeof(event.message) - 1);
                strncpy(event.data, msg.payload.event.data, sizeof(event.data) - 1);
                
                event_dispatcher_post(&event);
            }
            else if (msg.type == WS_MSG_COMMAND) {
                ESP_LOGI(TAG, "Command received: %s", msg.payload.command.action);
                
                // Handle send-nuke command by parsing params and triggering button action
                if (strcmp(msg.payload.command.action, "send-nuke") == 0) {
                    ESP_LOGI(TAG, "Nuke command params: %s", msg.payload.command.params);
                    
                    // Parse nukeType from params JSON: {"nukeType":"atom"}
                    cJSON *params_json = cJSON_Parse(msg.payload.command.params);
                    if (params_json) {
                        cJSON *nuke_type = cJSON_GetObjectItem(params_json, "nukeType");
                        if (nuke_type && cJSON_IsString(nuke_type)) {
                            uint8_t button_index = 0;
                            game_event_type_t event_type = GAME_EVENT_NUKE_LAUNCHED;
                            
                            // Map nuke type to button index - all use unified NUKE_LAUNCHED event
                            if (strcmp(nuke_type->valuestring, "atom") == 0) {
                                button_index = 0;
                                event_type = GAME_EVENT_NUKE_LAUNCHED;
                            } else if (strcmp(nuke_type->valuestring, "hydro") == 0) {
                                button_index = 1;
                                event_type = GAME_EVENT_NUKE_LAUNCHED;
                            } else if (strcmp(nuke_type->valuestring, "mirv") == 0) {
                                button_index = 2;
                                event_type = GAME_EVENT_NUKE_LAUNCHED;
                            } else {
                                ESP_LOGW(TAG, "Unknown nuke type: %s", nuke_type->valuestring);
                                cJSON_Delete(params_json);
                                break;
                            }
                            
                            ESP_LOGI(TAG, "Triggering nuke button %d (%s)", button_index, nuke_type->valuestring);
                            
                            // Post button press event (simulates physical button press)
                            internal_event_t button_event = {
                                .type = INTERNAL_EVENT_BUTTON_PRESSED,
                                .source = EVENT_SOURCE_WEBSOCKET,
                                .timestamp = esp_timer_get_time() / 1000,
                                .data = {button_index}
                            };
                            event_dispatcher_post(&button_event);
                        } else {
                            ESP_LOGW(TAG, "send-nuke command missing nukeType parameter");
                        }
                        cJSON_Delete(params_json);
                    } else {
                        ESP_LOGW(TAG, "Failed to parse send-nuke params JSON");
                    }
                }
            }
        }
        break;
        
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket Error");
        event_dispatcher_post_simple(INTERNAL_EVENT_WS_ERROR, EVENT_SOURCE_SYSTEM);
        break;
        
    default:
        break;
    }
}

esp_err_t ws_client_init(const char *server_url) {
    if (!server_url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_websocket_client_config_t config = {
        .uri = server_url,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
    };
    
    client = esp_websocket_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init WebSocket client");
        return ESP_FAIL;
    }
    
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, 
                                 websocket_event_handler, NULL);
    
    ESP_LOGI(TAG, "WebSocket client initialized: %s", server_url);
    return ESP_OK;
}

esp_err_t ws_client_start(void) {
    if (!client) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return esp_websocket_client_start(client);
}

void ws_client_stop(void) {
    if (client) {
        esp_websocket_client_stop(client);
        esp_websocket_client_destroy(client);
        client = NULL;
        is_connected = false;
    }
}

esp_err_t ws_client_send_text(const char *data, size_t len) {
    if (!is_connected || !data || len == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return esp_websocket_client_send_text(client, data, len, portMAX_DELAY);
}

esp_err_t ws_client_send_event(const game_event_t *event) {
    if (!is_connected || !event) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char buffer[512];
    if (ws_protocol_build_event(event, buffer, sizeof(buffer)) != ESP_OK) {
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sending event: %s", buffer);
    return ws_client_send_text(buffer, strlen(buffer));
}

bool ws_client_is_connected(void) {
    return is_connected;
}

void ws_client_set_connection_callback(ws_connection_callback_t callback) {
    connection_callback = callback;
}
