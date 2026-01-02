# WebSocket Protocol - Implementation Prompt

## Overview

This prompt guides the implementation of **WebSocket message handling** in the OTS firmware. The firmware acts as a WebSocket **server** (not client), receiving connections from the dashboard UI and userscript.

## Purpose

The WebSocket protocol implementation provides:
- **Bidirectional communication**: Send and receive messages between firmware and clients
- **Message serialization**: Convert between C structures and JSON
- **Event routing**: Dispatch messages to appropriate hardware modules
- **Client management**: Track connected clients (UI, userscript)

## Architecture

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│  Dashboard  │◄───────►│   Firmware   │◄───────►│ Userscript  │
│   (UI)      │  WSS    │  WS Server   │  WSS    │  (Client)   │
└─────────────┘         └──────────────┘         └─────────────┘
                              │
                              ▼
                        ┌──────────────┐
                        │   Protocol   │
                        │   Handler    │
                        └──────────────┘
                              │
                              ▼
                        ┌──────────────┐
                        │    Event     │
                        │  Dispatcher  │
                        └──────────────┘
                              │
                ┌─────────────┼─────────────┐
                ▼             ▼             ▼
        ┌──────────┐  ┌──────────┐  ┌──────────┐
        │  Alert   │  │   Nuke   │  │  Troops  │
        │  Module  │  │  Module  │  │  Module  │
        └──────────┘  └──────────┘  └──────────┘
```

## Protocol Specification

The protocol is defined in `/prompts/protocol-context.md`. All implementations must stay in sync.

### Message Envelope

All messages follow this structure:

```json
{
  "type": "event" | "cmd" | "state" | "handshake",
  "payload": { ... }
}
```

### Message Types

1. **Handshake** - Client identification
2. **Event** - Game events and notifications
3. **Command** - User actions (button presses, etc.)
4. **State** - Game state updates

## WebSocket Server Implementation

### 1. Server Initialization

Located in `src/ws_server.c`:

```c
#include "ws_server.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char* TAG = "OTS_WS_SERVER";

// Client tracking
typedef struct {
    int fd;
    char client_type[16];  // "ui" or "userscript"
} ws_client_t;

static ws_client_t clients[MAX_WS_CLIENTS];
static int client_count = 0;

esp_err_t ws_server_init(httpd_handle_t server) {
    ESP_LOGI(TAG, "Initializing WebSocket server");
    
    // WebSocket handler configuration
    httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
    };
    
    return httpd_register_uri_handler(server, &ws);
}
```

### 2. WebSocket Handler

```c
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake from fd=%d", httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    // First call to get frame length
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (ws_pkt.len == 0) {
        ESP_LOGW(TAG, "Empty frame received");
        return ESP_OK;
    }
    
    // Allocate buffer and receive full frame
    uint8_t* buf = malloc(ws_pkt.len + 1);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }
    
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive frame: %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }
    
    buf[ws_pkt.len] = '\0';  // Null-terminate
    
    // Handle message
    handle_ws_message(req, (char*)buf, ws_pkt.len);
    
    free(buf);
    return ESP_OK;
}
```

### 3. Message Parsing

```c
static void handle_ws_message(httpd_req_t *req, const char* data, size_t len) {
    ESP_LOGD(TAG, "Received: %.*s", (int)len, data);
    
    // Parse JSON
    cJSON* root = cJSON_Parse(data);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error");
        return;
    }
    
    cJSON* type_obj = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type_obj)) {
        ESP_LOGE(TAG, "Missing or invalid 'type' field");
        cJSON_Delete(root);
        return;
    }
    
    const char* type = type_obj->valuestring;
    
    if (strcmp(type, "handshake") == 0) {
        handle_handshake(req, root);
    } else if (strcmp(type, "event") == 0) {
        handle_event_message(req, root);
    } else if (strcmp(type, "cmd") == 0) {
        handle_command_message(req, root);
    } else if (strcmp(type, "state") == 0) {
        handle_state_message(req, root);
    } else {
        ESP_LOGW(TAG, "Unknown message type: %s", type);
    }
    
    cJSON_Delete(root);
}
```

### 4. Handshake Handler

```c
static void handle_handshake(httpd_req_t *req, cJSON* root) {
    cJSON* payload = cJSON_GetObjectItem(root, "payload");
    if (!payload) {
        ESP_LOGW(TAG, "Handshake missing payload");
        return;
    }
    
    cJSON* client_type = cJSON_GetObjectItem(payload, "clientType");
    if (!cJSON_IsString(client_type)) {
        ESP_LOGW(TAG, "Handshake missing clientType");
        return;
    }
    
    int fd = httpd_req_to_sockfd(req);
    const char* type = client_type->valuestring;
    
    // Add to client list
    if (client_count < MAX_WS_CLIENTS) {
        clients[client_count].fd = fd;
        strncpy(clients[client_count].client_type, type, sizeof(clients[0].client_type) - 1);
        client_count++;
        
        ESP_LOGI(TAG, "Client identified: fd=%d, type=%s", fd, type);
        
        // Post connection event
        if (strcmp(type, "userscript") == 0) {
            post_internal_event(INTERNAL_EVENT_WS_CONNECTED, "userscript");
        }
    } else {
        ESP_LOGW(TAG, "Max clients reached");
    }
}
```

### 5. Event Message Handler

```c
static void handle_event_message(httpd_req_t *req, cJSON* root) {
    cJSON* payload = cJSON_GetObjectItem(root, "payload");
    if (!payload) {
        ESP_LOGW(TAG, "Event missing payload");
        return;
    }
    
    // Extract event type
    cJSON* event_type_obj = cJSON_GetObjectItem(payload, "type");
    if (!cJSON_IsString(event_type_obj)) {
        ESP_LOGW(TAG, "Event missing type");
        return;
    }
    
    const char* event_type_str = event_type_obj->valuestring;
    game_event_type_t event_type = string_to_event_type(event_type_str);
    
    if (event_type == GAME_EVENT_UNKNOWN) {
        ESP_LOGW(TAG, "Unknown event type: %s", event_type_str);
        return;
    }
    
    // Extract event data (optional)
    cJSON* event_data = cJSON_GetObjectItem(payload, "data");
    char* event_data_json = event_data ? cJSON_PrintUnformatted(event_data) : NULL;
    
    // Post to event dispatcher
    event_dispatcher_post(event_type, event_data_json);
    
    if (event_data_json) {
        free(event_data_json);
    }
    
    ESP_LOGI(TAG, "Event received: %s", event_type_str);
}
```

### 6. Command Message Handler

```c
static void handle_command_message(httpd_req_t *req, cJSON* root) {
    cJSON* payload = cJSON_GetObjectItem(root, "payload");
    if (!payload) {
        ESP_LOGW(TAG, "Command missing payload");
        return;
    }
    
    cJSON* action_obj = cJSON_GetObjectItem(payload, "action");
    if (!cJSON_IsString(action_obj)) {
        ESP_LOGW(TAG, "Command missing action");
        return;
    }
    
    const char* action = action_obj->valuestring;
    cJSON* params = cJSON_GetObjectItem(payload, "params");
    
    // Route command to appropriate handler
    if (strcmp(action, "send-nuke") == 0) {
        handle_send_nuke_command(params);
    } else if (strcmp(action, "set-troops-percent") == 0) {
        handle_set_troops_percent_command(params);
    } else if (strcmp(action, "hardware-diagnostic") == 0) {
        handle_hardware_diagnostic_command();
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", action);
    }
}
```

## Message Broadcasting

### Send to All Clients

```c
esp_err_t ws_server_broadcast(const char* message) {
    if (!message) return ESP_ERR_INVALID_ARG;
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)message;
    ws_pkt.len = strlen(message);
    
    int sent = 0;
    for (int i = 0; i < client_count; i++) {
        esp_err_t ret = httpd_ws_send_frame_async(
            server_handle,
            clients[i].fd,
            &ws_pkt
        );
        
        if (ret == ESP_OK) {
            sent++;
        } else {
            ESP_LOGW(TAG, "Failed to send to fd=%d: %s", 
                     clients[i].fd, esp_err_to_name(ret));
        }
    }
    
    ESP_LOGD(TAG, "Broadcast sent to %d/%d clients", sent, client_count);
    return sent > 0 ? ESP_OK : ESP_FAIL;
}
```

### Send to Specific Client Type

```c
esp_err_t ws_server_send_to_type(const char* message, const char* client_type) {
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)message;
    ws_pkt.len = strlen(message);
    
    int sent = 0;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].client_type, client_type) == 0) {
            esp_err_t ret = httpd_ws_send_frame_async(
                server_handle,
                clients[i].fd,
                &ws_pkt
            );
            
            if (ret == ESP_OK) {
                sent++;
            }
        }
    }
    
    return sent > 0 ? ESP_OK : ESP_FAIL;
}
```

## Event Serialization

### Event to JSON

```c
char* serialize_game_event(game_event_type_t event_type, const char* message, cJSON* data) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "event");
    
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "type", event_type_to_string(event_type));
    cJSON_AddNumberToObject(payload, "timestamp", esp_timer_get_time() / 1000);
    
    if (message) {
        cJSON_AddStringToObject(payload, "message", message);
    }
    
    if (data) {
        cJSON_AddItemToObject(payload, "data", cJSON_Duplicate(data, true));
    }
    
    cJSON_AddItemToObject(root, "payload", payload);
    
    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}
```

### Send Game Event

```c
void ws_server_send_game_event(game_event_type_t event_type, const char* message, cJSON* data) {
    char* json = serialize_game_event(event_type, message, data);
    if (json) {
        ws_server_broadcast(json);
        free(json);
    }
}
```

## Protocol Synchronization

### Event Type Mapping

Keep `protocol.h` in sync with `/prompts/protocol-context.md`:

```c
typedef enum {
    GAME_EVENT_UNKNOWN = 0,
    
    // Game lifecycle
    GAME_EVENT_GAME_START,
    GAME_EVENT_GAME_END,
    GAME_EVENT_WIN,
    GAME_EVENT_LOOSE,
    
    // Nuke events
    GAME_EVENT_NUKE_LAUNCHED,
    GAME_EVENT_HYDRO_LAUNCHED,
    GAME_EVENT_MIRV_LAUNCHED,
    GAME_EVENT_NUKE_EXPLODED,
    GAME_EVENT_NUKE_INTERCEPTED,
    
    // Alert events
    GAME_EVENT_ALERT_ATOM,
    GAME_EVENT_ALERT_HYDRO,
    GAME_EVENT_ALERT_MIRV,
    GAME_EVENT_ALERT_LAND,
    GAME_EVENT_ALERT_NAVAL,
    
    // Sound events
    GAME_EVENT_SOUND_PLAY,
    
    // Info events
    GAME_EVENT_INFO,
    GAME_EVENT_HARDWARE_TEST,
    GAME_EVENT_HARDWARE_DIAGNOSTIC,
    
    // Internal events
    INTERNAL_EVENT_NETWORK_CONNECTED,
    INTERNAL_EVENT_NETWORK_DISCONNECTED,
    INTERNAL_EVENT_WS_CONNECTED,
    INTERNAL_EVENT_WS_DISCONNECTED,
    INTERNAL_EVENT_BUTTON_PRESSED,
} game_event_type_t;
```

### String Conversion

```c
const char* event_type_to_string(game_event_type_t type) {
    switch (type) {
        case GAME_EVENT_GAME_START: return "GAME_START";
        case GAME_EVENT_GAME_END: return "GAME_END";
        case GAME_EVENT_WIN: return "WIN";
        case GAME_EVENT_LOOSE: return "LOOSE";
        case GAME_EVENT_NUKE_LAUNCHED: return "NUKE_LAUNCHED";
        case GAME_EVENT_HYDRO_LAUNCHED: return "HYDRO_LAUNCHED";
        case GAME_EVENT_MIRV_LAUNCHED: return "MIRV_LAUNCHED";
        // ... etc ...
        default: return "UNKNOWN";
    }
}

game_event_type_t string_to_event_type(const char* str) {
    if (strcmp(str, "GAME_START") == 0) return GAME_EVENT_GAME_START;
    if (strcmp(str, "GAME_END") == 0) return GAME_EVENT_GAME_END;
    if (strcmp(str, "WIN") == 0) return GAME_EVENT_WIN;
    if (strcmp(str, "LOOSE") == 0) return GAME_EVENT_LOOSE;
    if (strcmp(str, "NUKE_LAUNCHED") == 0) return GAME_EVENT_NUKE_LAUNCHED;
    // ... etc ...
    return GAME_EVENT_UNKNOWN;
}
```

## Client Connection Management

### Tracking Connected Clients

```c
typedef struct {
    int fd;
    char client_type[16];
    bool active;
    uint64_t last_heartbeat;
} ws_client_t;

static ws_client_t clients[MAX_WS_CLIENTS];

void update_client_heartbeat(int fd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == fd) {
            clients[i].last_heartbeat = esp_timer_get_time() / 1000;
            return;
        }
    }
}
```

### Client Cleanup

```c
void remove_client(int fd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == fd) {
            ESP_LOGI(TAG, "Removing client: fd=%d, type=%s", 
                     fd, clients[i].client_type);
            
            // Post disconnection event
            if (strcmp(clients[i].client_type, "userscript") == 0) {
                post_internal_event(INTERNAL_EVENT_WS_DISCONNECTED, "userscript");
            }
            
            clients[i].active = false;
            return;
        }
    }
}
```

## Error Handling

### Connection Errors

```c
if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Client disconnected during send");
    remove_client(fd);
} else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Send error: %s", esp_err_to_name(ret));
}
```

### Parsing Errors

```c
cJSON* root = cJSON_Parse(data);
if (root == NULL) {
    const char* error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
        ESP_LOGE(TAG, "JSON parse error before: %s", error_ptr);
    }
    return;
}
```

## Testing

### Manual Testing with websocat

```bash
# Connect to WebSocket server
websocat wss://192.168.x.x/ws --insecure

# Send handshake
{"type":"handshake","payload":{"clientType":"test"}}

# Send event
{"type":"event","payload":{"type":"NUKE_LAUNCHED","timestamp":1234567890}}

# Send command
{"type":"cmd","payload":{"action":"hardware-diagnostic"}}
```

### Python Test Script

See `tools/tests/ws_test.py`:

```python
import asyncio
import websockets
import json

async def test_websocket():
    uri = "wss://192.168.x.x/ws"
    
    async with websockets.connect(uri, ssl=ssl_context) as websocket:
        # Send handshake
        await websocket.send(json.dumps({
            "type": "handshake",
            "payload": {"clientType": "test"}
        }))
        
        # Send event
        await websocket.send(json.dumps({
            "type": "event",
            "payload": {
                "type": "NUKE_LAUNCHED",
                "timestamp": 1234567890
            }
        }))
        
        # Receive response
        response = await websocket.recv()
        print(f"Received: {response}")

asyncio.run(test_websocket())
```

## Integration Checklist

- [ ] WebSocket server initialized in HTTP server
- [ ] Handshake handler tracks client types
- [ ] Event messages parsed and dispatched
- [ ] Command messages routed to handlers
- [ ] State messages update game state
- [ ] Event serialization matches protocol spec
- [ ] String conversion functions complete
- [ ] Client connection tracking works
- [ ] Client cleanup on disconnect
- [ ] Broadcast works to all clients
- [ ] Send to specific client type works
- [ ] Error handling for all code paths
- [ ] Tested with real dashboard and userscript

## References

- Protocol specification: `/prompts/protocol-context.md`
- Event dispatcher: `src/event_dispatcher.c`, `include/event_dispatcher.h`
- Game state: `include/game_state.h`
- HTTP server: `src/http_server.c`
- Testing tools: `tools/tests/ws_test.py`, `tools/tests/test_websocket.sh`
- ESP-IDF WebSocket API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html
