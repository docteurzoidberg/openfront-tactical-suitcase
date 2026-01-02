# Network Provisioning - Implementation Prompt

## Overview

This prompt guides the implementation of **WiFi provisioning** in the OTS firmware. The system provides multiple ways to configure WiFi credentials: captive portal web UI, serial commands, and persistent storage.

## Purpose

Network provisioning provides:
- **Easy setup**: User-friendly captive portal for WiFi configuration
- **Credential storage**: Persistent WiFi credentials in NVS (non-volatile storage)
- **Fallback modes**: Serial commands for debugging and recovery
- **Auto-recovery**: Automatic fallback to AP mode on connection failure

## Provisioning Flow

```
Power On
    ↓
Check NVS for saved credentials
    ↓
    ├─ Credentials exist?
    │   ├─ YES → Try STA mode
    │   │         ↓
    │   │    Connection success?
    │   │         ├─ YES → Normal operation
    │   │         └─ NO → Start AP mode (fallback)
    │   │
    │   └─ NO → Start AP mode (initial setup)
    │
    ↓
AP Mode (SoftAP)
    ↓
Captive Portal at 192.168.4.1
    ↓
User configures WiFi
    ↓
Save to NVS
    ↓
Reboot or reconnect in STA mode
    ↓
Normal operation
```

## WiFi Modes

### 1. Station (STA) Mode - Normal Operation

Connect to existing WiFi network:

```c
esp_err_t wifi_start_sta(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Starting STA mode: %s", ssid);
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Wait for connection (with timeout)
    return wait_for_connection(30000);  // 30 second timeout
}
```

### 2. Access Point (AP) Mode - Provisioning

Create WiFi hotspot for configuration:

```c
esp_err_t wifi_start_ap(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Starting AP mode: %s", ssid);
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = 1,
            .max_connection = 4,
            .authmode = password[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    return ESP_OK;
}
```

### Configuration

```c
// AP mode defaults (in config.h)
#define WIFI_AP_SSID "OTS-Setup"
#define WIFI_AP_PASS ""  // Open network
#define WIFI_AP_IP   "192.168.4.1"
```

## NVS Storage

### Save Credentials

```c
#include "nvs_flash.h"
#include "nvs.h"

esp_err_t wifi_save_credentials(const char* ssid, const char* password) {
    nvs_handle_t handle;
    esp_err_t ret;
    
    // Open NVS
    ret = nvs_open("wifi", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Write SSID
    ret = nvs_set_str(handle, "ssid", ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    // Write password
    ret = nvs_set_str(handle, "password", password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write password: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit: %s", esp_err_to_name(ret));
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "WiFi credentials saved");
    return ret;
}
```

### Load Credentials

```c
esp_err_t wifi_load_credentials(char* ssid, size_t ssid_len, 
                                 char* password, size_t pass_len) {
    nvs_handle_t handle;
    esp_err_t ret;
    
    // Open NVS
    ret = nvs_open("wifi", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved credentials found");
        return ret;
    }
    
    // Read SSID
    size_t required_size = ssid_len;
    ret = nvs_get_str(handle, "ssid", ssid, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read SSID: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    // Read password
    required_size = pass_len;
    ret = nvs_get_str(handle, "password", password, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read password: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "WiFi credentials loaded: %s", ssid);
    return ESP_OK;
}
```

### Clear Credentials

```c
esp_err_t wifi_clear_credentials(void) {
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open("wifi", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Erase WiFi namespace
    ret = nvs_erase_all(handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "WiFi credentials cleared");
    return ret;
}
```

## Captive Portal

### HTTP Server Setup

The captive portal runs on port 80 and serves a WiFi configuration page:

```c
esp_err_t start_captive_portal(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.stack_size = 8192;
    
    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register handlers
    httpd_register_uri_handler(server, &wifi_config_page);
    httpd_register_uri_handler(server, &wifi_scan_handler);
    httpd_register_uri_handler(server, &wifi_connect_handler);
    
    ESP_LOGI(TAG, "Captive portal started on http://192.168.4.1");
    return ESP_OK;
}
```

### WiFi Configuration Page

```c
static const char* WIFI_CONFIG_HTML = R"(
<!DOCTYPE html>
<html>
<head>
    <title>OTS WiFi Setup</title>
    <style>
        body { font-family: Arial; max-width: 600px; margin: 50px auto; padding: 20px; }
        input, select { width: 100%; padding: 10px; margin: 10px 0; }
        button { background: #4CAF50; color: white; padding: 15px; border: none; cursor: pointer; width: 100%; }
        button:hover { background: #45a049; }
        .network { padding: 10px; border: 1px solid #ddd; margin: 5px 0; cursor: pointer; }
        .network:hover { background: #f0f0f0; }
    </style>
</head>
<body>
    <h1>OTS WiFi Setup</h1>
    <div id="networks"></div>
    <form action="/wifi/connect" method="POST">
        <input type="text" name="ssid" placeholder="WiFi Network" required>
        <input type="password" name="password" placeholder="Password" required>
        <button type="submit">Connect</button>
    </form>
    <script>
        fetch('/wifi/scan')
            .then(r => r.json())
            .then(networks => {
                networks.forEach(n => {
                    document.getElementById('networks').innerHTML += 
                        `<div class="network" onclick="document.querySelector('input[name=ssid]').value='${n.ssid}'">
                            ${n.ssid} (${n.rssi} dBm)
                        </div>`;
                });
            });
    </script>
</body>
</html>
)";

static esp_err_t wifi_config_page_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WIFI_CONFIG_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t wifi_config_page = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = wifi_config_page_handler,
};
```

### WiFi Scan Handler

```c
static esp_err_t wifi_scan_handler(httpd_req_t *req) {
    // Trigger WiFi scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };
    
    esp_wifi_scan_start(&scan_config, true);
    
    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t* ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    // Build JSON response
    cJSON* root = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) {
        cJSON* network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", (char*)ap_list[i].ssid);
        cJSON_AddNumberToObject(network, "rssi", ap_list[i].rssi);
        cJSON_AddItemToArray(root, network);
    }
    
    char* json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    free(ap_list);
    
    return ESP_OK;
}

static const httpd_uri_t wifi_scan_handler_uri = {
    .uri       = "/wifi/scan",
    .method    = HTTP_GET,
    .handler   = wifi_scan_handler,
};
```

### WiFi Connect Handler

```c
static esp_err_t wifi_connect_handler(httpd_req_t *req) {
    char buf[256];
    char ssid[33] = {0};
    char password[64] = {0};
    
    // Read POST data
    int total_len = req->content_len;
    int cur_len = 0;
    
    while (cur_len < total_len) {
        int received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    
    // Parse form data (ssid=xxx&password=yyy)
    char* ssid_start = strstr(buf, "ssid=");
    char* pass_start = strstr(buf, "password=");
    
    if (ssid_start) {
        ssid_start += 5;  // Skip "ssid="
        char* ssid_end = strchr(ssid_start, '&');
        int ssid_len = ssid_end ? (ssid_end - ssid_start) : strlen(ssid_start);
        strncpy(ssid, ssid_start, ssid_len);
        url_decode(ssid);
    }
    
    if (pass_start) {
        pass_start += 9;  // Skip "password="
        strncpy(password, pass_start, sizeof(password) - 1);
        url_decode(password);
    }
    
    // Save credentials
    wifi_save_credentials(ssid, password);
    
    // Send response
    const char* resp = "<html><body><h1>Connecting...</h1><p>Device will reboot</p></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    
    // Schedule reboot
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

static const httpd_uri_t wifi_connect_handler_uri = {
    .uri       = "/wifi/connect",
    .method    = HTTP_POST,
    .handler   = wifi_connect_handler,
};
```

## Serial Commands

### Command Interface

```c
void handle_serial_command(const char* cmd) {
    if (strcmp(cmd, "wifi-status") == 0) {
        print_wifi_status();
    } else if (strncmp(cmd, "wifi-provision ", 15) == 0) {
        // Format: wifi-provision SSID PASSWORD
        char ssid[33], password[64];
        if (sscanf(cmd + 15, "%32s %63s", ssid, password) == 2) {
            wifi_save_credentials(ssid, password);
            ESP_LOGI(TAG, "Credentials saved via serial");
        }
    } else if (strcmp(cmd, "wifi-clear") == 0) {
        wifi_clear_credentials();
        ESP_LOGI(TAG, "Credentials cleared via serial");
    }
}
```

### Status Display

```c
void print_wifi_status(void) {
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    
    if (mode == WIFI_MODE_STA) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            printf("Mode: STA (connected)\n");
            printf("SSID: %s\n", ap_info.ssid);
            printf("RSSI: %d dBm\n", ap_info.rssi);
        } else {
            printf("Mode: STA (disconnected)\n");
        }
    } else if (mode == WIFI_MODE_AP) {
        printf("Mode: AP (provisioning)\n");
        printf("SSID: %s\n", WIFI_AP_SSID);
        printf("URL: http://192.168.4.1\n");
    }
    
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    printf("IP: " IPSTR "\n", IP2STR(&ip_info.ip));
}
```

## Auto-Recovery

### Connection Monitoring

```c
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    static int retry_count = 0;
    
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from AP, retry %d/%d", retry_count, MAX_RETRY);
        
        if (retry_count < MAX_RETRY) {
            esp_wifi_connect();
            retry_count++;
        } else {
            ESP_LOGE(TAG, "Failed to connect after %d attempts", MAX_RETRY);
            ESP_LOGI(TAG, "Switching to AP mode for re-provisioning");
            
            // Clear invalid credentials
            wifi_clear_credentials();
            
            // Start AP mode
            wifi_start_ap(WIFI_AP_SSID, WIFI_AP_PASS);
            start_captive_portal();
            
            retry_count = 0;
        }
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Connected to AP");
        retry_count = 0;
    }
}
```

## Complete Initialization

```c
esp_err_t network_manager_init(void) {
    ESP_LOGI(TAG, "Initializing network manager");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create network interfaces
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler, NULL));
    
    // Try to load saved credentials
    char ssid[33], password[64];
    if (wifi_load_credentials(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK) {
        ESP_LOGI(TAG, "Found saved credentials, connecting to: %s", ssid);
        return wifi_start_sta(ssid, password);
    } else {
        ESP_LOGI(TAG, "No saved credentials, starting AP mode");
        wifi_start_ap(WIFI_AP_SSID, WIFI_AP_PASS);
        return start_captive_portal();
    }
}
```

## Testing Checklist

### NVS Storage
- [ ] Save credentials works
- [ ] Load credentials works
- [ ] Clear credentials works
- [ ] Survives reboot
- [ ] Handles missing credentials gracefully

### WiFi Connection
- [ ] STA mode connects successfully
- [ ] AP mode starts correctly
- [ ] Auto-retry on disconnect (up to MAX_RETRY)
- [ ] Fallback to AP mode after failed retries
- [ ] IP address assigned correctly

### Captive Portal
- [ ] HTTP server starts on 192.168.4.1
- [ ] WiFi scan returns networks
- [ ] Configuration page loads
- [ ] Form submission saves credentials
- [ ] Device reboots after configuration

### Serial Commands
- [ ] `wifi-status` shows correct info
- [ ] `wifi-provision` saves credentials
- [ ] `wifi-clear` removes credentials

### Edge Cases
- [ ] Handle empty SSID/password
- [ ] Handle very long SSID/password
- [ ] Handle special characters in credentials
- [ ] Handle connection to hidden network
- [ ] Handle WPA/WPA2/WPA3 networks

## Configuration

```c
// In config.h
#define WIFI_MAX_RETRY 5
#define WIFI_AP_SSID "OTS-Setup"
#define WIFI_AP_PASS ""  // Open network
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONN 4
```

## References

- WiFi provisioning docs: `docs/WIFI_PROVISIONING.md`
- Web app implementation: `docs/WIFI_WEBAPP.md`
- Network manager: `src/network_manager.c`
- HTTP server: `src/http_server.c`
- ESP-IDF WiFi API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html
- ESP-IDF NVS API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
