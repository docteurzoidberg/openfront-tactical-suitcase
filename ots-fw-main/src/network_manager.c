#include "network_manager.h"
#include "led_controller.h"
#include "event_dispatcher.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mdns.h"
#include <string.h>

static const char *TAG = "NET_MGR";

static bool is_connected = false;
static bool has_ip = false;
static char current_ip[16] = {0};
static network_event_callback_t event_callback = NULL;

static char wifi_ssid[32] = {0};
static char wifi_password[64] = {0};
static char mdns_hostname[32] = {0};

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
    esp_netif_create_default_wifi_sta();

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
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    
    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started, connecting to %s", wifi_ssid);
    return ESP_OK;
}

esp_err_t network_manager_stop(void) {
    ESP_LOGI(TAG, "Stopping network services...");
    
    esp_wifi_stop();
    is_connected = false;
    has_ip = false;
    led_controller_link_set(false);
    
    return ESP_OK;
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
        esp_wifi_connect();
        is_connected = true;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        is_connected = false;
        has_ip = false;
        led_controller_link_set(false);
        
        // Post internal event
        event_dispatcher_post_simple(INTERNAL_EVENT_NETWORK_DISCONNECTED, EVENT_SOURCE_SYSTEM);
        
        if (event_callback) {
            event_callback(NETWORK_EVENT_DISCONNECTED, NULL);
        }
        
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        
        ESP_LOGI(TAG, "Got IP: %s", current_ip);
        has_ip = true;
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
