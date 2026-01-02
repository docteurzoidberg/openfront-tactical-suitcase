/**
 * @file ws_handlers.c
 * @brief WebSocket communication handlers - extracted from ws_server.c
 * 
 * This component provides WebSocket endpoint handlers for real-time game
 * communication with userscript and dashboard UI. Registers with the HTTP
 * server core via ws_handlers_register().
 * 
 * Responsibilities:
 * - WebSocket /ws endpoint handler
 * - Client tracking (dashboard UI and userscript)
 * - Protocol message parsing and routing
 * - Event broadcasting to connected clients
 * - Userscript connection status tracking
 * 
 * Dependencies:
 * - http_server: Registers handlers with HTTP server core
 * - ws_protocol: Message parsing and building
 * - event_dispatcher: Routes protocol events to firmware
 * 
 * This component is independent of webapp UI and configuration handlers.
 */

#include "ws_handlers.h"
#include "ws_protocol.h"
#include "event_dispatcher.h"
#include "protocol.h"
#include "config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if !CONFIG_HTTPD_WS_SUPPORT

static const char *TAG = "WS_HANDLERS";
static ws_connection_callback_t connection_callback = NULL;

esp_err_t ws_handlers_register(httpd_handle_t server) {
    (void)server;
    ESP_LOGW(TAG, "WebSocket support disabled (CONFIG_HTTPD_WS_SUPPORT=0)");
    return ESP_ERR_NOT_SUPPORTED;
}

void ws_handlers_set_connection_callback(ws_connection_callback_t cb) {
    connection_callback = cb;
    (void)connection_callback;
}

bool ws_handlers_has_userscript(void) {
    return false;
}

esp_err_t ws_handlers_send_text(const char *data, size_t len) {
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ws_handlers_send_event(const game_event_t *event) {
    (void)event;
    return ESP_ERR_NOT_SUPPORTED;
}

bool ws_handlers_is_connected(void) {
    return false;
}

int ws_handlers_get_client_count(void) {
    return 0;
}

int ws_handlers_get_userscript_count(void) {
    return 0;
}

#else

static const char *TAG = "WS_HANDLERS";

static httpd_handle_t s_server = NULL;
static ws_connection_callback_t connection_callback = NULL;
static int active_clients = 0;
static int userscript_clients = 0;

// Store client file descriptors
#define MAX_CLIENTS 4
static int client_fds[MAX_CLIENTS] = {0};
static bool client_is_userscript[MAX_CLIENTS] = {0};

static int find_client_index(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == fd) {
            return i;
        }
    }
    return -1;
}

static bool is_client_registered(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == fd) {
            return true;
        }
    }
    return false;
}

static void register_client(int fd) {
    if (fd <= 0 || is_client_registered(fd)) {
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == 0) {
            client_fds[i] = fd;
            client_is_userscript[i] = false;
            active_clients++;
            ESP_LOGI(TAG, "Client connected (fd=%d), total clients: %d", fd, active_clients);
            break;
        }
    }
}

static void unregister_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] == fd) {
            const bool was_userscript = client_is_userscript[i];
            if (was_userscript) {
                client_is_userscript[i] = false;
                if (userscript_clients > 0) {
                    userscript_clients--;
                }
            }

            client_fds[i] = 0;
            active_clients--;
            ESP_LOGI(TAG, "Client disconnected (fd=%d), total clients: %d", fd, active_clients);

            // Align with test-websocket semantics: "connected" means the userscript is
            // connected, not merely that *some* WebSocket client exists.
            if (was_userscript && userscript_clients == 0) {
                ESP_LOGI(TAG, "Last userscript disconnected; userscript_clients=0");
                event_dispatcher_post_simple(INTERNAL_EVENT_WS_DISCONNECTED, EVENT_SOURCE_SYSTEM);
                if (connection_callback) {
                    connection_callback(false);
                }
            }

            // If we have no clients at all, also ensure modules can reset state.
            // (This is a no-op if the userscript transition already fired above.)
            if (active_clients == 0 && userscript_clients == 0) {
                event_dispatcher_post_simple(INTERNAL_EVENT_WS_DISCONNECTED, EVENT_SOURCE_SYSTEM);
                if (connection_callback) {
                    connection_callback(false);
                }
            }
            break;
        }
    }
}

// Ensure clients are unregistered even when the TCP/TLS socket closes without a
// WebSocket CLOSE frame (e.g. abrupt client exit, WiFi drop).
//
// NOTE: When `close_fn` is set, ESP-IDF expects the callback to close the socket.
static void ws_session_close_cb(httpd_handle_t hd, int sockfd) {
    (void)hd;
    unregister_client(sockfd);
    // Best-effort; sockfd may already be invalid per ESP-IDF docs.
    (void)shutdown(sockfd, SHUT_RDWR);
    (void)close(sockfd);
}

static esp_err_t ws_handler(httpd_req_t *req) {
    // ESP-IDF calls this handler once for the initial HTTP upgrade (handshake)
    // and then again for subsequent WebSocket frames.
    // Do not attempt to read a WebSocket frame during the handshake invocation,
    // or the server will complain about masking / framing and we'll drop data.
    int fd = httpd_req_to_sockfd(req);

    // If this invocation is the initial HTTP Upgrade request, it will still have
    // HTTP headers. WebSocket frame invocations do not.
    // Reading ws frames during the upgrade call can trigger esp_http_server logs
    // like: "WS frame is not properly masked".
    const size_t upgrade_len = httpd_req_get_hdr_value_len(req, "Upgrade");
    if (upgrade_len > 0) {
        // Let the server complete the handshake; frames will arrive in later calls.
        return ESP_OK;
    }

    // This URI can be hit by non-WebSocket HTTP GETs as well. Only treat the
    // connection as an active client once the WebSocket protocol is active.
    if (s_server == NULL || httpd_ws_get_fd_info(s_server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
        ESP_LOGD(TAG, "/ws called but not a WebSocket client yet (fd=%d)", fd);
        return ESP_OK;
    }

    if (!is_client_registered(fd)) {
        ESP_LOGI(TAG, "WebSocket client active (fd=%d)", fd);
        register_client(fd);
    }
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    // Receive frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // This can happen on the initial handshake invocation (no frame to read).
        // Treat as non-fatal so the handshake can complete.
        ESP_LOGD(TAG, "httpd_ws_recv_frame (len probe) returned: %s", esp_err_to_name(ret));
        return ESP_OK;
    }

    if (ws_pkt.len == 0 && ws_pkt.type != HTTPD_WS_TYPE_CLOSE) {
        // No payload to read.
        return ESP_OK;
    }
    
    // Special-case CLOSE frames: we don't need to read payload.
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "Client requested close");
        unregister_client(fd);
        return ESP_OK;
    }

    // Allocate buffer for payload (avoid heap churn for common small frames)
    uint8_t small_buf[513];
    uint8_t *buf = NULL;
    if (ws_pkt.len <= 512) {
        buf = small_buf;
    } else {
        buf = malloc(ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for WebSocket payload");
            return ESP_ERR_NO_MEM;
        }
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        if (buf != small_buf) {
            free(buf);
        }
        return ret;
    }

    buf[ws_pkt.len] = '\0';  // Null terminate
    
    // Handle different frame types
    if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        // If control frames are delivered to us, we must respond with PONG.
        httpd_ws_frame_t pong;
        memset(&pong, 0, sizeof(pong));
        pong.type = HTTPD_WS_TYPE_PONG;
        pong.payload = ws_pkt.payload;
        pong.len = ws_pkt.len;
        (void)httpd_ws_send_frame(req, &pong);
    } else if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        // No action needed.
    } else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        // Debug only: raw frames can be very frequent (e.g. TROOP_UPDATE every 100ms).
        const int max_log = 160;
        ESP_LOGD(TAG, "WS TEXT frame len=%d: %.*s", ws_pkt.len, ws_pkt.len > max_log ? max_log : (int)ws_pkt.len, (char *)ws_pkt.payload);
        
        // Parse message using protocol handler
        ws_message_t msg;
        ret = ws_protocol_parse((char *)ws_pkt.payload, ws_pkt.len, &msg);
        if (ret == ESP_OK) {
            if (msg.type == WS_MSG_HANDSHAKE) {
                const int idx = find_client_index(fd);
                const bool is_userscript = (strcmp(msg.payload.handshake.client_type, "userscript") == 0);
                if (idx >= 0 && is_userscript && !client_is_userscript[idx]) {
                    client_is_userscript[idx] = true;
                    const bool was_zero = (userscript_clients == 0);
                    userscript_clients++;
                    ESP_LOGI(TAG, "Identified userscript client (fd=%d), userscript_clients=%d", fd, userscript_clients);

                    // Notify modules/app when the *first* userscript appears.
                    if (was_zero) {
                        event_dispatcher_post_simple(INTERNAL_EVENT_WS_CONNECTED, EVENT_SOURCE_SYSTEM);
                        if (connection_callback) {
                            connection_callback(true);
                        }
                    }
                }
            } else if (msg.type == WS_MSG_EVENT) {
                // Forward protocol events into the firmware event system.
                // NOTE: Userscript can send frequent INFO heartbeats; don't enqueue them
                // or they can fill the event queue and cause important events (GAME_START)
                // to be dropped.
                if (msg.payload.event.event_type == GAME_EVENT_INFO) {
                    // Logging every heartbeat at INFO can starve CPU and destabilize WS.
                    ESP_LOGD(TAG, "Received INFO event: %s", msg.payload.event.message);
                    // Treat the userscript's initial INFO as a secondary handshake.
                    // Userscript sends: {type:'event', payload:{type:'INFO', message:'userscript-connected'}}
                    const int idx = find_client_index(fd);
                    if (idx >= 0 &&
                        strcmp(msg.payload.event.message, "userscript-connected") == 0 &&
                        !client_is_userscript[idx]) {
                        client_is_userscript[idx] = true;
                        const bool was_zero = (userscript_clients == 0);
                        userscript_clients++;
                        ESP_LOGI(TAG, "Identified userscript client via INFO (fd=%d), userscript_clients=%d", fd, userscript_clients);

                        if (was_zero) {
                            event_dispatcher_post_simple(INTERNAL_EVENT_WS_CONNECTED, EVENT_SOURCE_SYSTEM);
                            if (connection_callback) {
                                connection_callback(true);
                            }
                        }
                    }

                    ESP_LOGD(TAG, "Dropping INFO event (no enqueue)");
                } else if (msg.payload.event.event_type == GAME_EVENT_INVALID) {
                    ESP_LOGD(TAG, "Dropping invalid event (no enqueue)");
                } else {
                    ESP_LOGI(TAG, "Received event type=%d msg=%s", (int)msg.payload.event.event_type, msg.payload.event.message);
                    internal_event_t evt = {
                        .type = msg.payload.event.event_type,
                        .source = EVENT_SOURCE_WEBSOCKET,
                        .timestamp = msg.payload.event.timestamp,
                    };
                    strncpy(evt.message, msg.payload.event.message, sizeof(evt.message) - 1);
                    strncpy(evt.data, msg.payload.event.data, sizeof(evt.data) - 1);
                    event_dispatcher_post(&evt);
                }
            } else if (msg.type == WS_MSG_COMMAND) {
                ESP_LOGI(TAG, "Received command: %s", msg.payload.command.action);
                
                // Handle hardware-diagnostic command
                if (strcmp(msg.payload.command.action, "hardware-diagnostic") == 0) {
                    // Build diagnostic response
                    cJSON *root = cJSON_CreateObject();
                    if (root) {
                        cJSON_AddStringToObject(root, "type", "event");
                        
                        cJSON *payload = cJSON_CreateObject();
                        cJSON_AddStringToObject(payload, "type", "HARDWARE_DIAGNOSTIC");
                        cJSON_AddNumberToObject(payload, "timestamp", esp_timer_get_time() / 1000);
                        cJSON_AddStringToObject(payload, "message", "OTS Firmware Diagnostic");
                        
                        cJSON *data = cJSON_CreateObject();
                        cJSON_AddStringToObject(data, "version", OTS_FIRMWARE_VERSION);
                        cJSON_AddStringToObject(data, "deviceType", "firmware");
                        cJSON_AddStringToObject(data, "serialNumber", OTS_DEVICE_SERIAL_NUMBER);
                        cJSON_AddStringToObject(data, "owner", OTS_DEVICE_OWNER);
                        
                        // Hardware component detection
                        cJSON *hardware = cJSON_CreateObject();
                        
                        // LCD: Check if initialized
                        cJSON *lcd = cJSON_CreateObject();
                        extern bool lcd_is_initialized(void);
                        bool lcd_present = lcd_is_initialized();
                        cJSON_AddBoolToObject(lcd, "present", lcd_present);
                        cJSON_AddBoolToObject(lcd, "working", lcd_present);
                        cJSON_AddItemToObject(hardware, "lcd", lcd);
                        
                        // Input Board (MCP23017 board 0)
                        cJSON *inputBoard = cJSON_CreateObject();
                        extern bool io_expander_is_board_present(uint8_t board);
                        bool input_present = io_expander_is_board_present(0);
                        cJSON_AddBoolToObject(inputBoard, "present", input_present);
                        cJSON_AddBoolToObject(inputBoard, "working", input_present);
                        cJSON_AddItemToObject(hardware, "inputBoard", inputBoard);
                        
                        // Output Board (MCP23017 board 1)
                        cJSON *outputBoard = cJSON_CreateObject();
                        bool output_present = io_expander_is_board_present(1);
                        cJSON_AddBoolToObject(outputBoard, "present", output_present);
                        cJSON_AddBoolToObject(outputBoard, "working", output_present);
                        cJSON_AddItemToObject(hardware, "outputBoard", outputBoard);
                        
                        // ADC: Check if initialized
                        cJSON *adc = cJSON_CreateObject();
                        extern bool adc_handler_is_initialized(void);
                        bool adc_present = adc_handler_is_initialized();
                        cJSON_AddBoolToObject(adc, "present", adc_present);
                        cJSON_AddBoolToObject(adc, "working", adc_present);
                        cJSON_AddItemToObject(hardware, "adc", adc);
                        
                        // Sound Module: Not implemented yet
                        cJSON *soundModule = cJSON_CreateObject();
                        cJSON_AddBoolToObject(soundModule, "present", false);
                        cJSON_AddBoolToObject(soundModule, "working", false);
                        cJSON_AddItemToObject(hardware, "soundModule", soundModule);
                        
                        cJSON_AddItemToObject(data, "hardware", hardware);
                        cJSON_AddItemToObject(payload, "data", data);
                        cJSON_AddItemToObject(root, "payload", payload);
                        
                        char *response = cJSON_PrintUnformatted(root);
                        if (response) {
                            ESP_LOGI(TAG, "Sending hardware diagnostic response");
                            ws_handlers_send_text(response, strlen(response));
                            free(response);
                        }
                        cJSON_Delete(root);
                    }
                }
            }

            ESP_LOGD(TAG, "Message parsed successfully");
        } else {
            ESP_LOGW(TAG, "Failed to parse message");
        }
        
    }

    if (buf != small_buf) {
        free(buf);
    }
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
            esp_err_t ret = httpd_ws_send_frame_async(s_server, client_fds[i], &ws_pkt);
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

esp_err_t ws_handlers_register(httpd_handle_t server) {
    if (server == NULL) {
        ESP_LOGE(TAG, "Invalid server handle");
        return ESP_ERR_INVALID_ARG;
    }

    // Store server handle for async send operations
    s_server = server;
    
    // Initialize client tracking
    active_clients = 0;
    userscript_clients = 0;
    memset(client_fds, 0, sizeof(client_fds));
    memset(client_is_userscript, 0, sizeof(client_is_userscript));
    
    // Register WebSocket handler
    static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true
    };
    
    esp_err_t ret = httpd_register_uri_handler(server, &ws);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket handler: %s", esp_err_to_name(ret));
        s_server = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "WebSocket handlers registered successfully");
    ESP_LOGI(TAG, "WebSocket endpoint: %s://<device-ip>:%d/ws", WS_USE_TLS ? "wss" : "ws", 3000);
    
    return ESP_OK;
}

bool ws_handlers_has_userscript(void) {
    return userscript_clients > 0;
}

esp_err_t ws_handlers_send_text(const char *data, size_t len) {
    if (s_server == NULL) {
        ESP_LOGW(TAG, "WebSocket handlers not registered");
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
    if (httpd_queue_work(s_server, ws_async_send, data_copy) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue async send");
        free(data_copy);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t ws_handlers_send_event(const game_event_t *event) {
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char buffer[512];
    esp_err_t ret = ws_protocol_build_event(event, buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build event message");
        return ret;
    }
    
    return ws_handlers_send_text(buffer, strlen(buffer));
}

bool ws_handlers_is_connected(void) {
    return active_clients > 0;
}

int ws_handlers_get_client_count(void) {
    return active_clients;
}

int ws_handlers_get_userscript_count(void) {
    return userscript_clients;
}

void ws_handlers_set_connection_callback(ws_connection_callback_t callback) {
    connection_callback = callback;
}

void ws_handlers_set_session_close_callback(httpd_handle_t server) {
    if (server == NULL) {
        ESP_LOGW(TAG, "Cannot set session close callback on NULL server");
        return;
    }
    // This should be set during HTTP server creation via httpd_config_t.close_fn
    // We provide a getter for the callback function
    ESP_LOGD(TAG, "Session close callback should be set via http_server config");
}

// Public getter for session close callback (for http_server to use)
httpd_close_func_t ws_handlers_get_session_close_callback(void) {
    return ws_session_close_cb;
}

#endif  // CONFIG_HTTPD_WS_SUPPORT
