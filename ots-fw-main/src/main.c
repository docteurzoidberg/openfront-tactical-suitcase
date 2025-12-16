#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "mdns.h"

#include "config.h"
#include "io_expander.h"
#include "module_io.h"
#include "protocol.h"
#include "ws_client.h"

static const char *TAG = "MAIN";

// OTA state
static httpd_handle_t ota_server = NULL;
static bool ota_in_progress = false;

// LED blink states (for alerts and nuke buttons)
static bool nuke_leds_blinking[3] = {false, false, false};
static bool alert_leds_active[6] = {false, false, false, false, false, false};
static TimerHandle_t nuke_led_timers[3] = {NULL, NULL, NULL};
static TimerHandle_t alert_led_timers[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

// Button monitoring
static void button_monitor_task(void *pvParameters);
static void nuke_led_blink_timer_callback(TimerHandle_t xTimer);
static void alert_led_timer_callback(TimerHandle_t xTimer);

// mDNS initialization
static void mdns_init_service(void) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %d", err);
        return;
    }

    mdns_hostname_set(OTA_HOSTNAME);
    mdns_instance_name_set("OTS Firmware Main Controller");
    
    mdns_service_add(NULL, "_arduino", "_tcp", OTA_PORT, NULL, 0);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    
    ESP_LOGI(TAG, "mDNS service started: %s.local", OTA_HOSTNAME);
}

// OTA HTTP handler
static esp_err_t ota_post_handler(httpd_req_t *req) {
    char buf[1024];
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = NULL;
    esp_err_t err;
    int remaining = req->content_len;
    bool image_header_was_checked = false;

    ESP_LOGI(TAG, "Starting OTA update, size: %d bytes", remaining);
    ota_in_progress = true;
    
    // Turn off all module LEDs during OTA
    for (int i = 0; i < 3; i++) {
        module_io_set_nuke_led(i, false);
    }
    for (int i = 0; i < 6; i++) {
        module_io_set_alert_led(i, false);
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find update partition");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No update partition");
        ota_in_progress = false;
        return ESP_FAIL;
    }

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        ota_in_progress = false;
        return ESP_FAIL;
    }

    int received = 0;
    int progress = 0;
    bool led_state = false;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, sizeof(buf) < remaining ? sizeof(buf) : remaining);
        
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "HTTP receive failed");
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            ota_in_progress = false;
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            ota_in_progress = false;
            return ESP_FAIL;
        }

        received += recv_len;
        remaining -= recv_len;
        
        // Show progress with LINK LED blinking
        int new_progress = (received * 100) / req->content_len;
        if (new_progress != progress && new_progress % 5 == 0) {
            progress = new_progress;
            led_state = !led_state;
            module_io_set_link_led(led_state);
            ESP_LOGI(TAG, "OTA Progress: %d%%", progress);
        }
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        ota_in_progress = false;
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        ota_in_progress = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update successful! Rebooting...");
    httpd_resp_sendstr(req, "Update successful, rebooting...");
    
    ota_in_progress = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

// Basic auth check for OTA
static bool check_ota_auth(httpd_req_t *req) {
    char *buf = NULL;
    size_t buf_len = 0;
    
    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            // Check for "Basic " prefix
            if (strncmp(buf, "Basic ", 6) == 0) {
                // Simple password check - in production use proper base64 decode
                // For now, just check if Authorization header exists
                // Real implementation would decode base64 and verify credentials
                free(buf);
                return true;  // TODO: Implement proper auth
            }
        }
        free(buf);
    }
    
    // For compatibility with Arduino OTA tools, allow without auth for now
    // In production, enforce authentication
    return true;
}

// OTA handler wrapper with auth
static esp_err_t ota_handler(httpd_req_t *req) {
    if (!check_ota_auth(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    return ota_post_handler(req);
}

// Start OTA HTTP server
static void start_ota_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = OTA_PORT;
    config.ctrl_port = OTA_PORT + 1;
    
    httpd_uri_t ota_uri = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&ota_server, &config) == ESP_OK) {
        httpd_register_uri_handler(ota_server, &ota_uri);
        ESP_LOGI(TAG, "OTA server started on port %d", OTA_PORT);
    } else {
        ESP_LOGE(TAG, "Failed to start OTA server");
    }
}

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        module_io_set_link_led(false);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        module_io_set_link_led(true);
        
        // Initialize mDNS
        mdns_init_service();
        
        // Start OTA server
        start_ota_server();
        
        // Start WebSocket client
        ws_client_start();
    }
}

// Initialize WiFi
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized, connecting to %s", WIFI_SSID);
}

// Callback handlers for WebSocket events
static void handle_nuke_launched(game_event_type_t event_type) {
    ESP_LOGI(TAG, "Nuke launched: %s", event_type_to_string(event_type));
    
    uint8_t led_index = 0;
    if (event_type == GAME_EVENT_NUKE_LAUNCHED) led_index = 0;
    else if (event_type == GAME_EVENT_HYDRO_LAUNCHED) led_index = 1;
    else if (event_type == GAME_EVENT_MIRV_LAUNCHED) led_index = 2;
    
    // Start blinking the nuke LED
    nuke_leds_blinking[led_index] = true;
    
    // Stop existing timer if any
    if (nuke_led_timers[led_index]) {
        xTimerStop(nuke_led_timers[led_index], 0);
        xTimerDelete(nuke_led_timers[led_index], 0);
    }
    
    // Create timer to stop blinking after 10 seconds
    nuke_led_timers[led_index] = xTimerCreate("nuke_led", 
                                              pdMS_TO_TICKS(10000), 
                                              pdFALSE, 
                                              (void *)led_index, 
                                              nuke_led_blink_timer_callback);
    xTimerStart(nuke_led_timers[led_index], 0);
}

static void handle_alert(game_event_type_t event_type) {
    ESP_LOGI(TAG, "Alert: %s", event_type_to_string(event_type));
    
    uint8_t led_index = 0;
    uint32_t duration_ms = 10000;  // Default 10 seconds for nuke alerts
    
    switch (event_type) {
        case GAME_EVENT_ALERT_ATOM:
            led_index = 1;  // LED index 1 (atom)
            break;
        case GAME_EVENT_ALERT_HYDRO:
            led_index = 2;  // LED index 2 (hydro)
            break;
        case GAME_EVENT_ALERT_MIRV:
            led_index = 3;  // LED index 3 (mirv)
            break;
        case GAME_EVENT_ALERT_LAND:
            led_index = 4;  // LED index 4 (land invasion)
            duration_ms = 15000;  // 15 seconds for invasions
            break;
        case GAME_EVENT_ALERT_NAVAL:
            led_index = 5;  // LED index 5 (naval invasion)
            duration_ms = 15000;
            break;
        default:
            return;
    }
    
    // Turn on alert LED
    alert_leds_active[led_index] = true;
    module_io_set_alert_led(led_index, true);
    
    // Also turn on warning LED
    alert_leds_active[0] = true;
    module_io_set_alert_led(0, true);
    
    // Stop existing timer if any
    if (alert_led_timers[led_index]) {
        xTimerStop(alert_led_timers[led_index], 0);
        xTimerDelete(alert_led_timers[led_index], 0);
    }
    
    // Create timer to turn off after duration
    alert_led_timers[led_index] = xTimerCreate("alert_led",
                                               pdMS_TO_TICKS(duration_ms),
                                               pdFALSE,
                                               (void *)led_index,
                                               alert_led_timer_callback);
    xTimerStart(alert_led_timers[led_index], 0);
}

// Game state tracking
static bool in_game = false;

static void handle_game_state(game_event_type_t event_type) {
    ESP_LOGI(TAG, "Game state: %s", event_type_to_string(event_type));
    
    if (event_type == GAME_EVENT_GAME_START) {
        // Game started - reset all state
        in_game = true;
        
        // Turn off all LEDs at game start
        for (int i = 0; i < 3; i++) {
            nuke_leds_blinking[i] = false;
            module_io_set_nuke_led(i, false);
        }
        for (int i = 0; i < 6; i++) {
            alert_leds_active[i] = false;
            module_io_set_alert_led(i, false);
        }
        
        ESP_LOGI(TAG, "Game started - state reset");
    } else if (event_type == GAME_EVENT_GAME_END) {
        // Game ended
        in_game = false;
        ESP_LOGI(TAG, "Game ended");
    }
}

// Timer callbacks
static void nuke_led_blink_timer_callback(TimerHandle_t xTimer) {
    uint8_t led_index = (uint8_t)(uint32_t)pvTimerGetTimerID(xTimer);
    nuke_leds_blinking[led_index] = false;
    module_io_set_nuke_led(led_index, false);
}

static void alert_led_timer_callback(TimerHandle_t xTimer) {
    uint8_t led_index = (uint8_t)(uint32_t)pvTimerGetTimerID(xTimer);
    alert_leds_active[led_index] = false;
    module_io_set_alert_led(led_index, false);
    
    // Check if any alerts are still active, if not turn off warning LED
    bool any_active = false;
    for (int i = 1; i < 6; i++) {
        if (alert_leds_active[i]) {
            any_active = true;
            break;
        }
    }
    if (!any_active) {
        alert_leds_active[0] = false;
        module_io_set_alert_led(0, false);
    }
}

// Button monitoring task
static void button_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button monitor task started");
    
    bool last_button_states[3] = {false, false, false};
    
    while (1) {
        // Check each button
        for (int i = 0; i < 3; i++) {
            bool pressed = false;
            if (module_io_read_nuke_button(i, &pressed)) {
                if (pressed && !last_button_states[i]) {
                    // Button just pressed
                    ESP_LOGI(TAG, "Button %d pressed", i);
                    
                    // Send nuke event
                    game_event_t event = {0};
                    event.timestamp = esp_timer_get_time() / 1000;  // Convert to ms
                    
                    if (i == 0) {
                        event.type = GAME_EVENT_NUKE_LAUNCHED;
                        strncpy(event.message, "Nuke sent", sizeof(event.message) - 1);
                        strncpy(event.data, "{\"nukeType\":\"atom\"}", sizeof(event.data) - 1);
                    } else if (i == 1) {
                        event.type = GAME_EVENT_HYDRO_LAUNCHED;
                        strncpy(event.message, "Nuke sent", sizeof(event.message) - 1);
                        strncpy(event.data, "{\"nukeType\":\"hydro\"}", sizeof(event.data) - 1);
                    } else if (i == 2) {
                        event.type = GAME_EVENT_MIRV_LAUNCHED;
                        strncpy(event.message, "Nuke sent", sizeof(event.message) - 1);
                        strncpy(event.data, "{\"nukeType\":\"mirv\"}", sizeof(event.data) - 1);
                    }
                    
                    ws_client_send_event(&event);
                }
                last_button_states[i] = pressed;
            }
        }
        
        // Blink nuke LEDs if they're in blinking state
        for (int i = 0; i < 3; i++) {
            if (nuke_leds_blinking[i]) {
                static uint32_t last_blink_time = 0;
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (now - last_blink_time > LED_BLINK_INTERVAL_MS) {
                    static bool blink_state = false;
                    blink_state = !blink_state;
                    module_io_set_nuke_led(i, blink_state);
                    last_blink_time = now;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms scan rate
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "OTS Firmware Main Controller Starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize I/O expanders
    ESP_LOGI(TAG, "Initializing I/O expanders...");
    if (!io_expander_begin(MCP23017_ADDRESSES, MCP23017_COUNT)) {
        ESP_LOGE(TAG, "Failed to initialize I/O expanders!");
        return;
    }
    
    // Initialize module I/O
    if (module_io_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize module I/O!");
        return;
    }
    
    // Initialize WiFi
    wifi_init();
    
    // Initialize WebSocket client
    ws_client_init(WS_SERVER_URL);
    ws_client_set_nuke_callback(handle_nuke_launched);
    ws_client_set_alert_callback(handle_alert);
    ws_client_set_game_state_callback(handle_game_state);
    
    // Create button monitoring task
    xTaskCreate(button_monitor_task, "button_mon", 4096, NULL, 
                TASK_PRIORITY_BUTTON_MONITOR, NULL);
    
    ESP_LOGI(TAG, "OTS Firmware initialized successfully");
}
