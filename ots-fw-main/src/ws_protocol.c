#include "ws_protocol.h"
#include "event_dispatcher.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "OTS_WS_PROTO";

esp_err_t ws_protocol_init(void) {
    ESP_LOGI(TAG, "WebSocket protocol handler initialized");
    return ESP_OK;
}

esp_err_t ws_protocol_parse(const char *json_str, size_t len, ws_message_t *msg) {
    if (!json_str || !msg || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(msg, 0, sizeof(ws_message_t));
    msg->type = WS_MSG_UNKNOWN;
    
    cJSON *root = cJSON_ParseWithLength(json_str, len);
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }
    
    cJSON *type_obj = cJSON_GetObjectItem(root, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    const char *type_str = type_obj->valuestring;
    
    // Parse based on message type
    if (strcmp(type_str, "handshake") == 0) {
        msg->type = WS_MSG_HANDSHAKE;
        cJSON *client_type = cJSON_GetObjectItem(root, "clientType");
        if (client_type && cJSON_IsString(client_type)) {
            strncpy(msg->payload.handshake.client_type, client_type->valuestring,
                   sizeof(msg->payload.handshake.client_type) - 1);
        }
    }
    else if (strcmp(type_str, "event") == 0) {
        msg->type = WS_MSG_EVENT;
        cJSON *payload = cJSON_GetObjectItem(root, "payload");
        if (payload) {
            cJSON *event_type = cJSON_GetObjectItem(payload, "type");
            if (event_type && cJSON_IsString(event_type)) {
                msg->payload.event.event_type = string_to_event_type(event_type->valuestring);
            }
            
            cJSON *timestamp = cJSON_GetObjectItem(payload, "timestamp");
            if (timestamp && cJSON_IsNumber(timestamp)) {
                msg->payload.event.timestamp = timestamp->valueint;
            }
            
            cJSON *message = cJSON_GetObjectItem(payload, "message");
            if (message && cJSON_IsString(message)) {
                strncpy(msg->payload.event.message, message->valuestring,
                       sizeof(msg->payload.event.message) - 1);
            }
            
            cJSON *data = cJSON_GetObjectItem(payload, "data");
            if (data && cJSON_IsString(data)) {
                strncpy(msg->payload.event.data, data->valuestring,
                       sizeof(msg->payload.event.data) - 1);
            }
        }
    }
    else if (strcmp(type_str, "cmd") == 0) {
        msg->type = WS_MSG_COMMAND;
        cJSON *payload = cJSON_GetObjectItem(root, "payload");
        if (payload) {
            cJSON *action = cJSON_GetObjectItem(payload, "action");
            if (action && cJSON_IsString(action)) {
                strncpy(msg->payload.command.action, action->valuestring,
                       sizeof(msg->payload.command.action) - 1);
            }
            
            cJSON *params = cJSON_GetObjectItem(payload, "params");
            if (params) {
                char *params_str = cJSON_PrintUnformatted(params);
                if (params_str) {
                    strncpy(msg->payload.command.params, params_str,
                           sizeof(msg->payload.command.params) - 1);
                    free(params_str);
                }
            }
        }
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t ws_protocol_build_handshake(const char *client_type, char *out_buffer, size_t buffer_size) {
    if (!client_type || !out_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddStringToObject(root, "type", "handshake");
    cJSON_AddStringToObject(root, "clientType", client_type);
    
    char *json_str = cJSON_PrintUnformatted(root);
    esp_err_t ret = ESP_OK;
    
    if (json_str) {
        if (strlen(json_str) < buffer_size) {
            strcpy(out_buffer, json_str);
        } else {
            ret = ESP_ERR_INVALID_SIZE;
        }
        free(json_str);
    } else {
        ret = ESP_FAIL;
    }
    
    cJSON_Delete(root);
    return ret;
}

esp_err_t ws_protocol_build_event(const game_event_t *event, char *out_buffer, size_t buffer_size) {
    if (!event || !out_buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddStringToObject(root, "type", "event");
    
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "type", event_type_to_string(event->type));
    cJSON_AddNumberToObject(payload, "timestamp", event->timestamp);
    cJSON_AddStringToObject(payload, "message", event->message);
    
    if (event->data && strlen(event->data) > 0) {
        cJSON_AddStringToObject(payload, "data", event->data);
    }
    
    cJSON_AddItemToObject(root, "payload", payload);
    
    char *json_str = cJSON_PrintUnformatted(root);
    esp_err_t ret = ESP_OK;
    
    if (json_str) {
        if (strlen(json_str) < buffer_size) {
            strcpy(out_buffer, json_str);
        } else {
            ESP_LOGW(TAG, "Buffer too small for event message");
            ret = ESP_ERR_INVALID_SIZE;
        }
        free(json_str);
    } else {
        ret = ESP_FAIL;
    }
    
    cJSON_Delete(root);
    return ret;
}

bool ws_protocol_validate(const char *json_str, size_t len) {
    if (!json_str || len == 0) {
        return false;
    }
    
    cJSON *root = cJSON_ParseWithLength(json_str, len);
    if (!root) {
        return false;
    }
    
    // Check for required "type" field
    cJSON *type = cJSON_GetObjectItem(root, "type");
    bool valid = (type != NULL && cJSON_IsString(type));
    
    cJSON_Delete(root);
    return valid;
}
