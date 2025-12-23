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
 *   - Orange: WiFi connected, waiting for WebSocket
 *   - Green: Fully connected (WiFi + WebSocket)
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
#include "ws_server.h"
#include "rgb_status.h"
#include "protocol.h"
#include "wifi_credentials.h"

// Optional Improv Serial provisioning (USB WebSerial).
// Build with -DENABLE_IMPROV_SERIAL=0 to disable.
#ifndef ENABLE_IMPROV_SERIAL
#define ENABLE_IMPROV_SERIAL 1
#endif

#if ENABLE_IMPROV_SERIAL
#include "improv_serial.h"
#endif

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

static volatile bool ws_server_start_requested = false;

#if ENABLE_IMPROV_SERIAL
static void on_improv_provisioned(bool success, const char *ssid) {
    if (!success) {
        ESP_LOGW(TAG, "Improv provisioning failed");
        return;
    }
    ESP_LOGI(TAG, "Improv provisioned SSID '%s' - rebooting to apply", ssid ? ssid : "");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}
#endif

// Forward declarations
void network_event_handler(network_event_type_t event, const char *ip);
void ws_connection_handler(bool connected);
void test_send_messages(void);
void display_statistics(void);

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
            ESP_LOGI(TAG, "RGB LED: Orange (WiFi only)");
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
            ESP_LOGW(TAG, "RGB LED: OFF (no connection)");
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
        rgb_status_set(RGB_STATUS_CONNECTED);
        ESP_LOGI(TAG, "RGB LED: Green (fully connected)");
        ESP_LOGI(TAG, "Server URL: %s<device-ip>:%d/ws", WS_PROTOCOL, WS_SERVER_PORT);
    } else {
        ESP_LOGW(TAG, "");
        ESP_LOGW(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGW(TAG, "║ WebSocket Disconnected                ║");
        ESP_LOGW(TAG, "╚═══════════════════════════════════════╝");
        test_state.ws_connected = false;
        if (test_state.wifi_connected) {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
            ESP_LOGW(TAG, "RGB LED: Orange (WiFi only)");
        } else {
            rgb_status_set(RGB_STATUS_DISCONNECTED);
            ESP_LOGW(TAG, "RGB LED: OFF (no connection)");
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
    
    esp_err_t ret = ws_server_send_event(&info_event);
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
    
    ret = ws_server_send_event(&test_event);
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
    ESP_LOGI(TAG, "  Server started: %s", ws_server_is_started() ? "Yes" : "No");
    ESP_LOGI(TAG, "  Server: %s<device-ip>:%d/ws", WS_PROTOCOL, WS_SERVER_PORT);
    ESP_LOGI(TAG, "  Clients connected: %d", ws_server_is_connected() ? 1 : 0);
    if (test_state.ws_connected) {
        uint32_t uptime = (esp_log_timestamp() - test_state.ws_connect_time) / 1000;
        ESP_LOGI(TAG, "  Uptime: %lu seconds", uptime);
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Message Statistics:");
    ESP_LOGI(TAG, "  Sent: %lu", test_state.messages_sent);
    ESP_LOGI(TAG, "  Received: %lu", test_state.messages_received);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "RGB LED Status:");
    rgb_status_t current = rgb_status_get();
    const char *status_str[] = {
        "OFF (disconnected)",
        "Orange (WiFi only)",
        "Green (fully connected)",
        "Red (error)"
    };
    ESP_LOGI(TAG, "  Current: %s", status_str[current]);
}

void test_led_cycle(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== RGB LED Manual Test ===");
    ESP_LOGI(TAG, "Testing all LED states manually...");
    
    // Save current state
    rgb_status_t saved_state = rgb_status_get();
    
    ESP_LOGI(TAG, "  OFF (black) - 2 seconds");
    rgb_status_set(RGB_STATUS_DISCONNECTED);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "  Orange - 2 seconds");
    rgb_status_set(RGB_STATUS_WIFI_ONLY);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "  Green - 2 seconds");
    rgb_status_set(RGB_STATUS_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "  Red - 2 seconds");
    rgb_status_set(RGB_STATUS_ERROR);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Restore state
    ESP_LOGI(TAG, "Restoring previous state");
    rgb_status_set(saved_state);
}

void app_main(void) {
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
    
    // Initialize WiFi credentials and Improv Serial
    ESP_LOGI(TAG, "Initializing WiFi credentials...");
    wifi_credentials_init();

#if ENABLE_IMPROV_SERIAL
    improv_serial_init();
    improv_serial_set_callback(on_improv_provisioned);
    improv_serial_start();
    ESP_LOGI(TAG, "✓ Improv Serial enabled");
#else
    ESP_LOGW(TAG, "Improv Serial disabled (ENABLE_IMPROV_SERIAL=0)");
#endif
    ESP_LOGI(TAG, "✓ Shared NVS: 'wifi' namespace");
    
    // Get credentials from NVS or fallback to config.h
    wifi_credentials_t wifi_creds;
        // Get credentials from NVS or fallback to config.h
        // If not provisioned yet, we just wait for Improv Serial.
        esp_err_t creds_ret = wifi_credentials_get(&wifi_creds);
        if (creds_ret == ESP_ERR_NOT_FOUND) {
#if ENABLE_IMPROV_SERIAL
            // Keep the serial stream as clean as possible for Improv tooling.
            ESP_LOGW(TAG, "No WiFi credentials provisioned yet.");
            ESP_LOGW(TAG, "Open https://improv-wifi.com/serial/ and provision WiFi.");
            ESP_LOGW(TAG, "Tip: Close any serial monitor before using WebSerial.");
            // Wait here until provisioning callback triggers a reboot.
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
#else
            // Improv is disabled: fall back to compile-time credentials.
            if (strlen(WIFI_SSID) == 0) {
                ESP_LOGE(TAG, "No WiFi credentials in NVS and WIFI_SSID is empty. Set WIFI_SSID/WIFI_PASSWORD in config.h or enable Improv.");
                return;
            }
            memset(&wifi_creds, 0, sizeof(wifi_creds));
            strncpy(wifi_creds.ssid, WIFI_SSID, sizeof(wifi_creds.ssid) - 1);
            strncpy(wifi_creds.password, WIFI_PASSWORD, sizeof(wifi_creds.password) - 1);
            ESP_LOGW(TAG, "Using fallback WiFi credentials from config.h (Improv disabled)");
#endif
        } else if (creds_ret != ESP_OK) {
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
    
    // Initialize WebSocket client
    ret = ws_server_init(WS_SERVER_PORT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket server!");
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // Register WebSocket connection callback
    ws_server_set_connection_callback(ws_connection_handler);
    
    // Start network (will trigger connection events)
    ret = network_manager_start();
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "WiFi not started (awaiting Improv provisioning)");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start network: %s", esp_err_to_name(ret));
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Network services started");
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    ESP_LOGI(TAG, "RGB LED: OFF (no connection yet)");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Watch the RGB LED:");
    ESP_LOGI(TAG, "  1. OFF → Connecting to WiFi");
    ESP_LOGI(TAG, "  2. Orange → WiFi OK, waiting for client");
    ESP_LOGI(TAG, "  3. Green → Client connected!");
    ESP_LOGI(TAG, "");
    
    // Main test loop
    int cycle = 1;
    while (1) {
        // Start (or retry) WebSocket server as soon as we have an IP.
        if (test_state.wifi_connected && test_state.current_ip[0] != '\0') {
            if (ws_server_start_requested || !ws_server_is_started()) {
                ws_server_start_requested = false;
                ESP_LOGI(TAG, "Starting WebSocket server...");
                esp_err_t start_ret = ws_server_start();
                if (start_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to start WebSocket server: %s", esp_err_to_name(start_ret));
                } else {
                    ESP_LOGI(TAG, "WebSocket server listening on %s%s:%d/ws", WS_PROTOCOL, test_state.current_ip, WS_SERVER_PORT);
                }
            }
        }

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
                ESP_LOGW(TAG, "Provision WiFi via Improv Serial (USB /dev/ttyACM0). Close serial monitor first.");
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
