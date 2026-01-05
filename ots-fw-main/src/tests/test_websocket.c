// Only compile when TEST_WEBSOCKET is defined
#ifdef TEST_WEBSOCKET

/*
 * WebSocket + Status LED Test
 * 
 * Tests complete networking stack:
 *   - WiFi connection
 *   - WebSocket client connection
 *   - RGB status LED indicators
 *   - Message sending/receiving
 *   - Connection lifecycle
 * 
 * Expected hardware:
 *   - ESP32-S3 with onboard RGB LED (GPIO48)
 *   - WiFi network configured in config.h
 *   - OTS server running at configured URL
 * 
 * LED States:
 *   - OFF (black): No WiFi
 *   - Blue: WiFi connecting
 *   - Yellow: WiFi connected, no WebSocket clients
 *   - Purple: Userscript connected
 *   - Green: Game started
 *   - Red: Error state
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "config.h"
#include "network_manager.h"
#include "http_server.h"
#include "ws_handlers.h"
#include "rgb_handler.h"
#include "protocol.h"
#include "wifi_credentials.h"
#include "event_dispatcher.h"

#include "ots_logging.h"

static const char *TAG = "TEST_WS";

// Test state
typedef struct {
    bool wifi_connected;
    bool ws_connected;
    uint32_t wifi_connect_time;
    uint32_t ws_connect_time;
    uint32_t messages_sent;
    uint32_t messages_received;
    char current_ip[16];
} test_state_t;

static test_state_t test_state = {0};

static volatile bool http_server_start_requested = false;

// Forward declarations
void network_event_handler(network_event_type_t event, const char *ip);
void ws_connection_handler(bool connected);
void test_send_messages(void);
void display_statistics(void);

static bool test_event_handler(const internal_event_t *event) {
    if (!event) {
        return false;
    }

    if (event->type == GAME_EVENT_GAME_START) {
        rgb_status_set(RGB_STATUS_GAME_STARTED);
        return true;
    }

    if (event->type == GAME_EVENT_GAME_END) {
        if (ws_handlers_has_userscript()) {
            rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
        } else if (test_state.wifi_connected) {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        } else {
            rgb_status_set(RGB_STATUS_DISCONNECTED);
        }
        return true;
    }

    return false;
}

void network_event_handler(network_event_type_t event, const char *ip) {
    switch (event) {
        case NETWORK_EVENT_CONNECTED:
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
            ESP_LOGI(TAG, "║ WiFi Connected                        ║");
            ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
            test_state.wifi_connected = true;
            test_state.wifi_connect_time = esp_log_timestamp();
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
            break;
            
        case NETWORK_EVENT_GOT_IP:
            if (ip) {
                strncpy(test_state.current_ip, ip, sizeof(test_state.current_ip) - 1);
                ESP_LOGI(TAG, "IP Address: %s", ip);
                // Request server start from the main task (avoid heavy work in event callback)
                ws_server_start_requested = true;
            }
            break;
            
        case NETWORK_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "");
            ESP_LOGW(TAG, "╔═══════════════════════════════════════╗");
            ESP_LOGW(TAG, "║ WiFi Disconnected                     ║");
            ESP_LOGW(TAG, "╚═══════════════════════════════════════╝");
            test_state.wifi_connected = false;
            test_state.ws_connected = false;
            rgb_status_set(RGB_STATUS_DISCONNECTED);
            break;
    }
}

void ws_connection_handler(bool connected) {
    if (connected) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGI(TAG, "║ WebSocket Connected                   ║");
        ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
        test_state.ws_connected = true;
        test_state.ws_connect_time = esp_log_timestamp();
        if (ws_server_has_userscript()) {
            rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
        } else {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        }
        ESP_LOGI(TAG, "Server URL: %s<device-ip>:%d/ws", WS_PROTOCOL, WS_SERVER_PORT);
    } else {
        ESP_LOGW(TAG, "");
        ESP_LOGW(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGW(TAG, "║ WebSocket Disconnected                ║");
        ESP_LOGW(TAG, "╚═══════════════════════════════════════╝");
        test_state.ws_connected = false;
        if (test_state.wifi_connected) {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        } else {
            rgb_status_set(RGB_STATUS_DISCONNECTED);
        }
    }
}

void test_send_messages(void) {
    if (!test_state.ws_connected) {
        ESP_LOGW(TAG, "Cannot send - not connected");
        return;
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Sending Test Messages ===");
    
    // Test 1: Send INFO event
    game_event_t info_event = {
        .type = GAME_EVENT_INFO,
        .timestamp = esp_log_timestamp()
    };
    snprintf(info_event.message, sizeof(info_event.message), 
             "Test message from firmware");
    
    esp_err_t ret = ws_handlers_send_event(&info_event);
    if (ret == ESP_OK) {
        test_state.messages_sent++;
        ESP_LOGI(TAG, "  ✓ Sent INFO event");
    } else {
        ESP_LOGE(TAG, "  ✗ Failed to send INFO event");
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 2: Send HARDWARE_TEST event
    game_event_t test_event = {
        .type = GAME_EVENT_HARDWARE_TEST,
        .timestamp = esp_log_timestamp()
    };
    snprintf(test_event.message, sizeof(test_event.message), 
             "Hardware test in progress");
    
    ret = ws_handlers_send_event(&test_event);
    if (ret == ESP_OK) {
        test_state.messages_sent++;
        ESP_LOGI(TAG, "  ✓ Sent HARDWARE_TEST event");
    } else {
        ESP_LOGE(TAG, "  ✗ Failed to send HARDWARE_TEST event");
    }
    
    ESP_LOGI(TAG, "Messages sent this cycle: 2");
}

void display_statistics(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║ Connection Statistics                 ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Network Status:");
    ESP_LOGI(TAG, "  WiFi SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "  WiFi: %s", test_state.wifi_connected ? "Connected" : "Disconnected");
    if (test_state.wifi_connected) {
        ESP_LOGI(TAG, "  IP Address: %s", test_state.current_ip);
        uint32_t uptime = (esp_log_timestamp() - test_state.wifi_connect_time) / 1000;
        ESP_LOGI(TAG, "  Uptime: %lu seconds", uptime);
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "WebSocket Status:");
    ESP_LOGI(TAG, "  Server started: %s", http_server_is_started() ? "Yes" : "No");
    ESP_LOGI(TAG, "  Server: %s<device-ip>:%d/ws", WS_PROTOCOL, WS_SERVER_PORT);
    ESP_LOGI(TAG, "  Clients connected: %d", ws_handlers_is_connected() ? 1 : 0);
    if (test_state.ws_connected) {
        uint32_t uptime = (esp_log_timestamp() - test_state.ws_connect_time) / 1000;
        ESP_LOGI(TAG, "  Uptime: %lu seconds", uptime);
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Message Statistics:");
    ESP_LOGI(TAG, "  Sent: %lu", test_state.messages_sent);
    ESP_LOGI(TAG, "  Received: %lu", test_state.messages_received);
    
    // Intentionally do not log RGB LED status transitions/states.
}

void test_led_cycle(void) {
    // Cycle through all LED states without logging status names.
    
    // Save current state
    rgb_status_t saved_state = rgb_status_get();
    
    rgb_status_set(RGB_STATUS_DISCONNECTED);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    rgb_status_set(RGB_STATUS_WIFI_CONNECTING);
    vTaskDelay(pdMS_TO_TICKS(2000));

    rgb_status_set(RGB_STATUS_WIFI_ONLY);
    vTaskDelay(pdMS_TO_TICKS(2000));

    rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(2000));

    rgb_status_set(RGB_STATUS_GAME_STARTED);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    rgb_status_set(RGB_STATUS_ERROR);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Restore state
    rgb_status_set(saved_state);
}

void app_main(void) {
    (void)ots_logging_init();

    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    OTS WebSocket + LED Test           ║");
    ESP_LOGI(TAG, "║    Network Stack Verification         ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // Initialize NVS (required for WiFi)
    ESP_LOGI(TAG, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi credentials
    ESP_LOGI(TAG, "Initializing WiFi credentials...");
    wifi_credentials_init();
    ESP_LOGI(TAG, "✓ Shared NVS: 'wifi' namespace");
    
    // Get credentials from NVS or fallback to config.h
    wifi_credentials_t wifi_creds;
    esp_err_t creds_ret = wifi_credentials_get(&wifi_creds);
    if (creds_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi credentials: %s", esp_err_to_name(creds_ret));
        return;
    }
    ESP_LOGI(TAG, "Using WiFi SSID: %s", wifi_creds.ssid);
    ESP_LOGI(TAG, "");
    
    // Initialize RGB status LED
    ESP_LOGI(TAG, "Initializing RGB status LED (GPIO%d)...", RGB_LED_GPIO);
    ret = rgb_status_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RGB LED!");
        ESP_LOGE(TAG, "Continuing without LED...");
    } else {
        ESP_LOGI(TAG, "RGB LED initialized successfully");
        rgb_status_set(RGB_STATUS_DISCONNECTED);
    }

    // Initialize event dispatcher (used to react to GAME_START/GAME_END)
    if (event_dispatcher_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize event dispatcher");
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    event_dispatcher_register(GAME_EVENT_INVALID, test_event_handler);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  WiFi SSID (fallback): %s", (strlen(WIFI_SSID) > 0 ? WIFI_SSID : "<empty>"));
    ESP_LOGI(TAG, "  WebSocket Server Port: %d", WS_SERVER_PORT);
    ESP_LOGI(TAG, "  RGB LED Pin: GPIO%d", RGB_LED_GPIO);
    ESP_LOGI(TAG, "");
    
    // Do LED cycle test before connecting
    test_led_cycle();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting network services...");
    
    // Initialize network manager with credentials from NVS
    ret = network_manager_init(wifi_creds.ssid, wifi_creds.password, MDNS_HOSTNAME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network manager!");
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // Register network event callback
    network_manager_set_event_callback(network_event_handler);
    
    // Initialize HTTP server with TLS
    extern const char ots_server_cert_pem[];
    extern const size_t ots_server_cert_pem_len;
    extern const char ots_server_key_pem[];
    extern const size_t ots_server_key_pem_len;
    
    http_server_config_t server_config = {
        .port = WS_SERVER_PORT,
        .use_tls = WS_USE_TLS,
        .cert_pem = (const uint8_t *)ots_server_cert_pem,
        .cert_len = ots_server_cert_pem_len,
        .key_pem = (const uint8_t *)ots_server_key_pem,
        .key_len = ots_server_key_pem_len,
        .max_open_sockets = 4,
        .max_uri_handlers = 8,
        .close_fn = NULL
    };
    
    ret = http_server_init(&server_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HTTP server!");
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    ret = http_server_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server!");
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // Register WebSocket handlers
    ret = ws_handlers_register(http_server_get_handle());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket handlers!");
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // Register WebSocket connection callback
    ws_handlers_set_connection_callback(ws_connection_handler);
    
    // Start network (will trigger connection events)
    rgb_status_set(RGB_STATUS_WIFI_CONNECTING);
    ret = network_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start network: %s", esp_err_to_name(ret));
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Network services started");
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    // Intentionally do not log RGB LED status.
    ESP_LOGI(TAG, "");
    // Intentionally do not log RGB LED status.
    
    // Main test loop
    int cycle = 1;
    while (1) {
        // HTTP server is already started with WebSocket handlers registered
        // No need for dynamic start logic

        vTaskDelay(pdMS_TO_TICKS(10000));  // Every 10 seconds
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "═══════════════════════════════════════");
        ESP_LOGI(TAG, "Test Cycle %d", cycle++);
        ESP_LOGI(TAG, "═══════════════════════════════════════");
        
        display_statistics();
        
        if (test_state.ws_connected) {
            test_send_messages();
        } else {
            ESP_LOGW(TAG, "");
            ESP_LOGW(TAG, "Not connected - skipping message test");
            if (!test_state.wifi_connected) {
                ESP_LOGW(TAG, "WiFi not connected. Check stored WiFi credentials and signal.");
            } else {
                ESP_LOGW(TAG, "Waiting for client connection");
                ESP_LOGW(TAG, "Connect userscript to: %s<device-ip>:%d/ws", WS_PROTOCOL, WS_SERVER_PORT);
            }
        }
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Next cycle in 10 seconds...");
    }
}

#endif // TEST_WEBSOCKET
