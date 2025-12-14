#include "ws_client.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "WS_CLIENT";

static esp_websocket_client_handle_t client = NULL;
static bool is_connected = false;
static ws_event_callback_t nuke_callback = NULL;
static ws_event_callback_t alert_callback = NULL;
static ws_event_callback_t game_state_callback = NULL;

static void websocket_event_handler(void *handler_args, esp_event_base_t base,
                                   int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket Connected");
        is_connected = true;
        
        // Send handshake to identify as firmware client
        const char *handshake = "{\"type\":\"handshake\",\"clientType\":\"firmware\"}";
        esp_websocket_client_send_text(client, handshake, strlen(handshake), portMAX_DELAY);
        break;
        
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WebSocket Disconnected");
        is_connected = false;
        break;
        
    case WEBSOCKET_EVENT_DATA:
        if (data->data_len > 0) {
            ESP_LOGD(TAG, "Received: %.*s", data->data_len, (char *)data->data_ptr);
            
            // Parse JSON message
            cJSON *root = cJSON_ParseWithLength((char *)data->data_ptr, data->data_len);
            if (root) {
                cJSON *type_obj = cJSON_GetObjectItem(root, "type");
                
                if (type_obj && cJSON_IsString(type_obj)) {
                    const char *type = type_obj->valuestring;
                    
                    // Handle commands from server
                    if (strcmp(type, "cmd") == 0) {
                        cJSON *payload = cJSON_GetObjectItem(root, "payload");
                        if (payload) {
                            cJSON *action = cJSON_GetObjectItem(payload, "action");
                            if (action && cJSON_IsString(action)) {
                                ESP_LOGI(TAG, "Command: %s", action->valuestring);
                                
                                // Handle send-nuke command
                                if (strcmp(action->valuestring, "send-nuke") == 0 && nuke_callback) {
                                    cJSON *params = cJSON_GetObjectItem(payload, "params");
                                    if (params) {
                                        cJSON *nuke_type = cJSON_GetObjectItem(params, "nukeType");
                                        if (nuke_type && cJSON_IsString(nuke_type)) {
                                            game_event_type_t event = string_to_event_type(nuke_type->valuestring);
                                            if (event != GAME_EVENT_INVALID) {
                                                nuke_callback(event);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // Handle event messages
                    else if (strcmp(type, "event") == 0) {
                        cJSON *payload = cJSON_GetObjectItem(root, "payload");
                        if (payload) {
                            cJSON *event_type_obj = cJSON_GetObjectItem(payload, "type");
                            if (event_type_obj && cJSON_IsString(event_type_obj)) {
                                game_event_type_t event = string_to_event_type(event_type_obj->valuestring);
                                
                                // Route to appropriate callback
                                if (event == GAME_EVENT_ALERT_ATOM || 
                                    event == GAME_EVENT_ALERT_HYDRO ||
                                    event == GAME_EVENT_ALERT_MIRV ||
                                    event == GAME_EVENT_ALERT_LAND ||
                                    event == GAME_EVENT_ALERT_NAVAL) {
                                    if (alert_callback) {
                                        alert_callback(event);
                                    }
                                }
                                else if (event == GAME_EVENT_GAME_START ||
                                        event == GAME_EVENT_GAME_END) {
                                    if (game_state_callback) {
                                        game_state_callback(event);
                                    }
                                }
                            }
                        }
                    }
                }
                
                cJSON_Delete(root);
            }
        }
        break;
        
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket Error");
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

esp_err_t ws_client_send_state(const game_state_t *state) {
    if (!is_connected || !state) {
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "state");
    
    cJSON *payload = cJSON_CreateObject();
    // Add state fields as needed
    cJSON_AddItemToObject(root, "payload", payload);
    
    char *json_str = cJSON_PrintUnformatted(root);
    esp_err_t ret = ESP_OK;
    
    if (json_str) {
        ret = esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);
        free(json_str);
    }
    
    cJSON_Delete(root);
    return ret;
}

esp_err_t ws_client_send_event(const game_event_t *event) {
    if (!is_connected || !event) {
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "event");
    
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "type", event_type_to_string(event->type));
    cJSON_AddNumberToObject(payload, "timestamp", event->timestamp);
    cJSON_AddStringToObject(payload, "message", event->message);
    
    if (event->data) {
        cJSON_AddStringToObject(payload, "data", event->data);
    }
    
    cJSON_AddItemToObject(root, "payload", payload);
    
    char *json_str = cJSON_PrintUnformatted(root);
    esp_err_t ret = ESP_OK;
    
    if (json_str) {
        ESP_LOGD(TAG, "Sending: %s", json_str);
        ret = esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);
        free(json_str);
    }
    
    cJSON_Delete(root);
    return ret;
}

bool ws_client_is_connected(void) {
    return is_connected;
}

void ws_client_set_nuke_callback(ws_event_callback_t callback) {
    nuke_callback = callback;
}

void ws_client_set_alert_callback(ws_event_callback_t callback) {
    alert_callback = callback;
}

void ws_client_set_game_state_callback(ws_event_callback_t callback) {
    game_state_callback = callback;
}
