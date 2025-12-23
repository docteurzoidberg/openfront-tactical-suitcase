#include "ws_server.h"
#include "ws_protocol.h"
#include "event_dispatcher.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "OTS_WS_SERVER";

static httpd_handle_t server = NULL;
static uint16_t server_port = 3000;
static ws_connection_callback_t connection_callback = NULL;
static int active_clients = 0;

// Store client file descriptors
#define MAX_CLIENTS 4
static int client_fds[MAX_CLIENTS] = {0};

static void register_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == 0) {
            client_fds[i] = fd;
            active_clients++;
            ESP_LOGI(TAG, "Client connected (fd=%d), total clients: %d", fd, active_clients);
            
            // Post internal event
            event_dispatcher_post_simple(INTERNAL_EVENT_WS_CONNECTED, EVENT_SOURCE_SYSTEM);
            
            // Notify callback
            if (connection_callback) {
                connection_callback(true);
            }
            break;
        }
    }
}

static void unregister_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == fd) {
            client_fds[i] = 0;
            active_clients--;
            ESP_LOGI(TAG, "Client disconnected (fd=%d), total clients: %d", fd, active_clients);
            
            if (active_clients == 0) {
                // Post internal event
                event_dispatcher_post_simple(INTERNAL_EVENT_WS_DISCONNECTED, EVENT_SOURCE_SYSTEM);
                
                // Notify callback
                if (connection_callback) {
                    connection_callback(false);
                }
            }
            break;
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake from client");
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // Receive frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (ws_pkt.len == 0) {
        // Client just connected
        register_client(httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    
    // Allocate buffer for payload
    uint8_t *buf = malloc(ws_pkt.len + 1);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
        return ESP_ERR_NO_MEM;
    }
    
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }
    
    buf[ws_pkt.len] = '\0';  // Null terminate
    
    // Handle different frame types
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        ESP_LOGI(TAG, "Received packet: %.*s", ws_pkt.len, ws_pkt.payload);
        
        // Parse message using protocol handler
        ws_message_t msg;
        ret = ws_protocol_parse((char *)ws_pkt.payload, ws_pkt.len, &msg);
        if (ret == ESP_OK) {
            // TODO: Handle parsed message (dispatch to modules)
            ESP_LOGI(TAG, "Message parsed successfully");
        } else {
            ESP_LOGW(TAG, "Failed to parse message");
        }
        
    } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "Client requested close");
        unregister_client(httpd_req_to_sockfd(req));
    }
    
    free(buf);
    return ESP_OK;
}

static void ws_async_send(void *arg) {
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)arg;
    ws_pkt.len = strlen((char *)arg);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // Send to all connected clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] != 0) {
            esp_err_t ret = httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send to client fd=%d: %s", 
                         client_fds[i], esp_err_to_name(ret));
                // Client may have disconnected
                unregister_client(client_fds[i]);
            }
        }
    }
    
    free(arg);
}

esp_err_t ws_server_init(uint16_t port) {
    server_port = port;
    active_clients = 0;
    memset(client_fds, 0, sizeof(client_fds));
    
    ESP_LOGI(TAG, "Initializing WebSocket server on port %d", server_port);
    return ESP_OK;
}

esp_err_t ws_server_start(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "Server already started");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = server_port;
    config.ctrl_port = server_port + 1;
    config.max_open_sockets = MAX_CLIENTS + 2;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register WebSocket handler
    httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true
    };
    
    ret = httpd_register_uri_handler(server, &ws);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket handler: %s", esp_err_to_name(ret));
        httpd_stop(server);
        server = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "WebSocket server started successfully");
    ESP_LOGI(TAG, "Listening for connections on ws://<device-ip>:%d/ws", server_port);
    return ESP_OK;
}

void ws_server_stop(void) {
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping WebSocket server");
        httpd_stop(server);
        server = NULL;
        active_clients = 0;
        memset(client_fds, 0, sizeof(client_fds));
    }
}

esp_err_t ws_server_send_text(const char *data, size_t len) {
    if (server == NULL) {
        ESP_LOGW(TAG, "Server not started");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (active_clients == 0) {
        ESP_LOGD(TAG, "No clients connected, message not sent");
        return ESP_OK;  // Not an error, just no recipients
    }
    
    // Copy data for async send
    char *data_copy = malloc(len + 1);
    if (data_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for send");
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(data_copy, data, len);
    data_copy[len] = '\0';
    
    // Queue async send
    if (httpd_queue_work(server, ws_async_send, data_copy) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue async send");
        free(data_copy);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t ws_server_send_event(const game_event_t *event) {
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char buffer[512];
    esp_err_t ret = ws_protocol_build_event(event, buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build event message");
        return ret;
    }
    
    return ws_server_send_text(buffer, strlen(buffer));
}

bool ws_server_is_connected(void) {
    return active_clients > 0;
}

void ws_server_set_connection_callback(ws_connection_callback_t callback) {
    connection_callback = callback;
}
