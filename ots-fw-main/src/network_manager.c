#include "network_manager.h"
#include "led_controller.h"
#include "event_dispatcher.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mdns.h"
#include <string.h>

static const char *TAG = "OTS_NETWORK";

static bool is_connected = false;
static bool has_ip = false;
static char current_ip[16] = {0};
static network_event_callback_t event_callback = NULL;

static bool portal_mode = false;
static uint8_t sta_fail_count = 0;

#define NETWORK_MANAGER_MAX_STA_RETRIES 3

static char wifi_ssid[32] = {0};
static char wifi_password[64] = {0};
static char mdns_hostname[32] = {0};

static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void mdns_init_service(void);

esp_err_t network_manager_init(const char *ssid, const char *password, const char *hostname) {
    ESP_LOGI(TAG, "Initializing network manager...");
    
    if (!ssid || !password || !hostname) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_FAIL;
    }
    
    // Store credentials
    strncpy(wifi_ssid, ssid, sizeof(wifi_ssid) - 1);
    strncpy(wifi_password, password, sizeof(wifi_password) - 1);
    strncpy(mdns_hostname, hostname, sizeof(mdns_hostname) - 1);
    
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &wifi_event_handler, NULL));

    ESP_LOGI(TAG, "Network manager initialized");
    return ESP_OK;
}

esp_err_t network_manager_start(void) {
    ESP_LOGI(TAG, "Starting network services...");

    portal_mode = false;
    sta_fail_count = 0;
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    
    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Disable WiFi power save to avoid periodic STA sleep/disconnects that can
    // drop long-lived connections like WSS/WebSocket.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started, connecting to %s", wifi_ssid);
    return ESP_OK;
}

esp_err_t network_manager_start_captive_portal(const char *ap_ssid, const char *ap_password) {
    if (!ap_ssid || strlen(ap_ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting captive portal AP: %s", ap_ssid);

    // Stop STA connection attempts if any.
    (void)esp_wifi_stop();

    // Create AP netif if needed.
    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = (uint8_t)strlen(ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval = 100;

    if (ap_password && strlen(ap_password) > 0) {
        strncpy((char *)ap_config.ap.password, ap_password, sizeof(ap_config.ap.password));
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_config.ap.password[0] = '\0';
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    portal_mode = true;
    is_connected = false;
    has_ip = false;
    sta_fail_count = 0;
    led_controller_link_set(false);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Captive portal AP started (connect to SSID '%s')", ap_ssid);
    return ESP_OK;
}

esp_err_t network_manager_stop(void) {
    ESP_LOGI(TAG, "Stopping network services...");
    
    esp_wifi_stop();
    is_connected = false;
    has_ip = false;
    portal_mode = false;
    sta_fail_count = 0;
    led_controller_link_set(false);
    
    return ESP_OK;
}

bool network_manager_is_portal_mode(void) {
    return portal_mode;
}

bool network_manager_is_connected(void) {
    return is_connected && has_ip;
}

esp_err_t network_manager_get_ip(char *ip_str, size_t len) {
    if (!has_ip || len < 16) {
        return ESP_FAIL;
    }
    
    strncpy(ip_str, current_ip, len - 1);
    ip_str[len - 1] = '\0';
    return ESP_OK;
}

void network_manager_set_event_callback(network_event_callback_t callback) {
    event_callback = callback;
}

esp_err_t network_manager_reconnect(void) {
    ESP_LOGI(TAG, "Reconnecting to WiFi...");
    return esp_wifi_connect();
}

static void mdns_init_service(void) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %d", err);
        return;
    }

    mdns_hostname_set(mdns_hostname);
    mdns_instance_name_set("OTS Firmware Main Controller");
    
    // Register services
    mdns_service_add(NULL, "_arduino", "_tcp", 3232, NULL, 0);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    
    ESP_LOGI(TAG, "mDNS service started: %s.local", mdns_hostname);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (portal_mode) {
            // Captive portal runs AP-only, but we may temporarily enable APSTA to
            // perform WiFi scans. Do not auto-connect while in portal mode.
            ESP_LOGI(TAG, "[WiFi] STA_START (portal mode) - ignoring auto-connect");
            return;
        }

        ESP_LOGI(TAG, "[WiFi] STA_START - initiating connection...");
        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[WiFi] esp_wifi_connect() failed: %s (0x%x)", esp_err_to_name(ret), ret);
        }
        // Don't set is_connected = true yet! Wait for WIFI_EVENT_STA_CONNECTED
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "[WiFi] STA_CONNECTED - WiFi connected to AP!");
        is_connected = true;
        sta_fail_count = 0;
        // Don't turn on LED yet - wait for IP address
        
        // Notify callback about WiFi connection
        if (event_callback) {
            event_callback(NETWORK_EVENT_CONNECTED, NULL);
        }
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "[WiFi] STA_DISCONNECTED - reason: %d", event->reason);
        
        // Log common disconnect reasons
        switch (event->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
            case WIFI_REASON_AUTH_FAIL:
                ESP_LOGE(TAG, "  → Authentication failed! Check password.");
                break;
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGE(TAG, "  → SSID '%s' not found! Check SSID.", wifi_ssid);
                break;
            case WIFI_REASON_ASSOC_LEAVE:
                ESP_LOGW(TAG, "  → Disconnected from AP");
                break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                ESP_LOGE(TAG, "  → 4-way handshake timeout");
                break;
            default:
                ESP_LOGW(TAG, "  → Reason code: %d", event->reason);
                break;
        }
        
        ESP_LOGI(TAG, "[WiFi] Reconnecting...");
        is_connected = false;
        has_ip = false;
        led_controller_link_set(false);
        
        // Post internal event
        event_dispatcher_post_simple(INTERNAL_EVENT_NETWORK_DISCONNECTED, EVENT_SOURCE_SYSTEM);
        
        if (event_callback) {
            event_callback(NETWORK_EVENT_DISCONNECTED, NULL);
        }
        
        // If we can't connect to the stored SSID after several attempts, request provisioning.
        if (!portal_mode && strlen(wifi_ssid) > 0) {
            sta_fail_count++;
            ESP_LOGW(TAG, "[WiFi] STA connect attempt failed (%u/%u)", (unsigned)sta_fail_count, (unsigned)NETWORK_MANAGER_MAX_STA_RETRIES);
            if (sta_fail_count >= NETWORK_MANAGER_MAX_STA_RETRIES) {
                ESP_LOGE(TAG, "[WiFi] Max retries reached; entering provisioning/portal mode");
                if (event_callback) {
                    event_callback(NETWORK_EVENT_PROVISIONING_REQUIRED, NULL);
                }
                return;
            }
        }

        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[WiFi] Reconnect failed: %s (0x%x)", esp_err_to_name(ret), ret);
        }
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        
        ESP_LOGI(TAG, "[IP] GOT_IP: %s", current_ip);
        has_ip = true;
        sta_fail_count = 0;
        led_controller_link_set(true);
        
        // Initialize mDNS
        mdns_init_service();
        
        // Post internal event
        event_dispatcher_post_simple(INTERNAL_EVENT_NETWORK_CONNECTED, EVENT_SOURCE_SYSTEM);
        
        if (event_callback) {
            event_callback(NETWORK_EVENT_GOT_IP, current_ip);
        }
    }
}
