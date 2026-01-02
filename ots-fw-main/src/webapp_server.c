/**
 * @file webapp_server.c
 * @brief OTS WebApp HTTP Server
 * 
 * Provides HTTP/HTTPS server for the OTS firmware web interface.
 * Serves the configuration webapp and provides REST APIs for:
 * - Device settings (serial number, owner name)
 * - WiFi network configuration
 * - Network status and scanning
 * - OTA firmware updates
 * - Factory reset
 * 
 * Supports two modes:
 * - NORMAL: Standard web server on station interface
 * - PORTAL: Captive portal mode with DNS redirect (for initial setup)
 */

#include "webapp_server.h"

#include "wifi_credentials.h"
#include "device_settings.h"
#include "network_manager.h"

#include "config.h"
#include "webapp/ots_webapp.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/errno.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "OTS_WEBAPP";

static httpd_handle_t s_server = NULL;
static uint16_t s_port = 80;
static wifi_config_mode_t s_mode = WIFI_CONFIG_MODE_NORMAL;

static TaskHandle_t s_dns_task = NULL;
static int s_dns_sock = -1;
static volatile bool s_dns_running = false;

// Default ESP-IDF softAP IP is 192.168.4.1 unless explicitly changed.
#define CAPTIVE_PORTAL_IP_A 192
#define CAPTIVE_PORTAL_IP_B 168
#define CAPTIVE_PORTAL_IP_C 4
#define CAPTIVE_PORTAL_IP_D 1
#define CAPTIVE_DNS_PORT 53

static void dns_task(void *arg);
static void captive_dns_start(void);
static void captive_dns_stop(void);
// Forward declarations (now exported - non-static)
esp_err_t wifi_config_handle_404_redirect(httpd_req_t *req, httpd_err_code_t err);
esp_err_t wifi_config_handle_webapp_get(httpd_req_t *req);
esp_err_t wifi_config_handle_api_status(httpd_req_t *req);
esp_err_t wifi_config_handle_api_scan(httpd_req_t *req);
esp_err_t wifi_config_handle_device(httpd_req_t *req);
esp_err_t wifi_config_handle_factory_reset(httpd_req_t *req);
esp_err_t wifi_config_handle_wifi_post(httpd_req_t *req);
esp_err_t wifi_config_handle_wifi_clear(httpd_req_t *req);
esp_err_t wifi_config_handle_ota_upload(httpd_req_t *req);

static bool form_get_value(const char *body, const char *key, char *out, size_t out_len) {
    if (!body || !key || !out || out_len == 0) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char *p = body;
    while ((p = strstr(p, key)) != NULL) {
        if ((p == body || *(p - 1) == '&') && p[key_len] == '=') {
            p += key_len + 1;

            size_t o = 0;
            for (size_t i = 0; p[i] != '\0' && p[i] != '&' && o + 1 < out_len;) {
                const char c = p[i];
                if (c == '+') {
                    out[o++] = ' ';
                    i++;
                    continue;
                }

                if (c == '%' && p[i + 1] != '\0' && p[i + 2] != '\0') {
                    const char h1 = p[i + 1];
                    const char h2 = p[i + 2];
                    int v1 = -1;
                    int v2 = -1;
                    if (h1 >= '0' && h1 <= '9') v1 = h1 - '0';
                    else if (h1 >= 'a' && h1 <= 'f') v1 = 10 + (h1 - 'a');
                    else if (h1 >= 'A' && h1 <= 'F') v1 = 10 + (h1 - 'A');
                    if (h2 >= '0' && h2 <= '9') v2 = h2 - '0';
                    else if (h2 >= 'a' && h2 <= 'f') v2 = 10 + (h2 - 'a');
                    else if (h2 >= 'A' && h2 <= 'F') v2 = 10 + (h2 - 'A');

                    if (v1 >= 0 && v2 >= 0) {
                        out[o++] = (char)((v1 << 4) | v2);
                        i += 3;
                        continue;
                    }
                    // Invalid percent encoding; fall through and copy '%'.
                }

                out[o++] = c;
                i++;
            }
            out[o] = '\0';
            return true;
        }
        p += key_len;
    }

    return false;
}

esp_err_t wifi_config_handle_webapp_get(httpd_req_t *req) {
    const char *uri = (req && req->uri) ? req->uri : "/";
    const char *path = uri;

    if (strcmp(uri, "/") == 0 || strcmp(uri, "/wifi") == 0) {
        path = "/index.html";
    }

    const ots_webapp_asset_t *asset = ots_webapp_find(path);
    if (!asset) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_OK;
    }

    httpd_resp_set_type(req, asset->content_type);
    if (asset->gzip) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-OTS-WebApp", OTS_WEBAPP_FINGERPRINT);

    return httpd_resp_send(req, (const char *)asset->data, (ssize_t)asset->len);
}

esp_err_t wifi_config_handle_wifi_post(httpd_req_t *req) {
    const int content_len = req->content_len;
    if (content_len <= 0 || content_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_OK;
    }

    char body[513];
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_OK;
    }
    body[received] = '\0';

    wifi_credentials_t creds;
    memset(&creds, 0, sizeof(creds));

    (void)form_get_value(body, "ssid", creds.ssid, sizeof(creds.ssid));
    (void)form_get_value(body, "password", creds.password, sizeof(creds.password));

    if (strlen(creds.ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_OK;
    }

    esp_err_t ret = wifi_credentials_save(&creds);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save creds: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Saved. Rebooting...\n");

    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

esp_err_t wifi_config_handle_wifi_clear(httpd_req_t *req) {
    (void)req;

    esp_err_t ret = wifi_credentials_clear();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear creds: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to clear");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Cleared. Rebooting...\n");

    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

esp_err_t wifi_config_handle_factory_reset(httpd_req_t *req) {
    ESP_LOGI(TAG, "=== Factory reset requested ===");

    // Clear WiFi credentials
    ESP_LOGI(TAG, "Clearing WiFi credentials...");
    esp_err_t ret = wifi_credentials_clear();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear WiFi: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi credentials cleared");
    }

    // Clear device settings (owner only)
    ESP_LOGI(TAG, "Clearing device settings...");
    ret = device_settings_factory_reset();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear device settings: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Device settings cleared");
    }

    ESP_LOGI(TAG, "Sending response and rebooting...");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Factory reset complete. Rebooting...\n");

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
    return ESP_OK;
}

esp_err_t wifi_config_handle_ota_upload(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA upload started, content length: %d", req->content_len);

    if (req->content_len <= 0 || req->content_len > 3 * 1024 * 1024) {
        ESP_LOGW(TAG, "Invalid content length: %d", req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware size");
        return ESP_OK;
    }

    // Initialize OTA
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Writing to partition: %s at 0x%lx", update_partition->label, update_partition->address);

    esp_ota_handle_t ota_handle;
    esp_err_t ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_OK;
    }

    // Receive and write firmware data
    char buf[1024];
    int remaining = req->content_len;
    int received = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, (remaining < sizeof(buf)) ? remaining : sizeof(buf));
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Receive failed: %d", recv_len);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_OK;
        } else if (recv_len > 0) {
            ret = esp_ota_write(ota_handle, buf, recv_len);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
                esp_ota_abort(ota_handle);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
                return ESP_OK;
            }
            remaining -= recv_len;
            received += recv_len;

            // Log progress every ~256KB
            if ((received % (256 * 1024)) < 1024) {
                ESP_LOGI(TAG, "OTA progress: %d / %d bytes", received, req->content_len);
            }
        }
    }

    ESP_LOGI(TAG, "OTA write complete: %d bytes", received);

    // Finalize OTA
    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_OK;
    }

    // Set boot partition
    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA update successful, rebooting...");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Upload successful. Rebooting...\n");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

esp_err_t wifi_config_handle_404_redirect(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;

    // In portal mode, redirect *any* unknown path to the WiFi config page.
    if (s_mode == WIFI_CONFIG_MODE_PORTAL) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        // Empty body is fine; most captive portal detectors only care about Location.
        return httpd_resp_send(req, NULL, 0);
    }

    // Normal mode: keep default 404 behavior.
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    return ESP_OK;
}

static const char *auth_mode_to_str(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK: return "WAPI_PSK";
        default: return "UNKNOWN";
    }
}

static size_t json_escape(const char *in, char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return 0;
    }
    if (!in) {
        out[0] = '\0';
        return 0;
    }

    size_t o = 0;
    for (size_t i = 0; in[i] != '\0'; i++) {
        const unsigned char c = (unsigned char)in[i];
        if (o + 2 >= out_len) {
            break;
        }

        if (c == '\\' || c == '"') {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c >= 0x20 && c <= 0x7E) {
            out[o++] = (char)c;
        } else {
            // Control/non-ASCII -> \u00XX
            if (o + 6 >= out_len) {
                break;
            }
            static const char *HEX = "0123456789abcdef";
            out[o++] = '\\';
            out[o++] = 'u';
            out[o++] = '0';
            out[o++] = '0';
            out[o++] = HEX[(c >> 4) & 0xF];
            out[o++] = HEX[c & 0xF];
        }
    }
    out[o] = '\0';
    return o;
}

esp_err_t wifi_config_handle_api_status(httpd_req_t *req) {
    const char *mode_str = (s_mode == WIFI_CONFIG_MODE_PORTAL) ? "portal" : "normal";
    const bool has_creds = wifi_credentials_exist();

    char ip[16] = {0};
    if (s_mode == WIFI_CONFIG_MODE_PORTAL) {
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", CAPTIVE_PORTAL_IP_A, CAPTIVE_PORTAL_IP_B, CAPTIVE_PORTAL_IP_C,
                 CAPTIVE_PORTAL_IP_D);
    } else {
        (void)network_manager_get_ip(ip, sizeof(ip));
    }

    wifi_credentials_t creds;
    memset(&creds, 0, sizeof(creds));
    if (wifi_credentials_load(&creds) != ESP_OK) {
        creds.ssid[0] = '\0';
    }

    char ssid_esc[96];
    (void)json_escape(creds.ssid, ssid_esc, sizeof(ssid_esc));

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"mode\":\"%s\",\"ip\":\"%s\",\"hasCredentials\":%s,\"savedSsid\":\"%s\"}\n",
             mode_str, ip, has_creds ? "true" : "false", ssid_esc);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

// Helper function for GET requests (internal)
static esp_err_t handle_device_get(httpd_req_t *req) {
    const char *mode_str = (s_mode == WIFI_CONFIG_MODE_PORTAL) ? "portal" : "normal";
    const bool has_creds = wifi_credentials_exist();

    char ip[16] = {0};
    if (s_mode == WIFI_CONFIG_MODE_PORTAL) {
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", CAPTIVE_PORTAL_IP_A, CAPTIVE_PORTAL_IP_B, CAPTIVE_PORTAL_IP_C,
                 CAPTIVE_PORTAL_IP_D);
    } else {
        (void)network_manager_get_ip(ip, sizeof(ip));
    }

    wifi_credentials_t creds;
    memset(&creds, 0, sizeof(creds));
    if (wifi_credentials_load(&creds) != ESP_OK) {
        creds.ssid[0] = '\0';
    }

    char ssid_esc[96];
    (void)json_escape(creds.ssid, ssid_esc, sizeof(ssid_esc));

    char serial[64] = {0};
    char owner[64] = {0};
    (void)device_settings_init();
    const bool owner_configured = device_settings_owner_exists();
    (void)device_settings_get_serial(serial, sizeof(serial));
    (void)device_settings_get_owner(owner, sizeof(owner));

    char serial_esc[128];
    char owner_esc[128];
    (void)json_escape(serial, serial_esc, sizeof(serial_esc));
    (void)json_escape(owner, owner_esc, sizeof(owner_esc));

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{"
             "\"mode\":\"%s\","
             "\"ip\":\"%s\","
             "\"hasCredentials\":%s,"
             "\"savedSsid\":\"%s\","
             "\"serialNumber\":\"%s\","
             "\"ownerName\":\"%s\","
             "\"ownerConfigured\":%s,"
             "\"firmwareVersion\":\"%s\""
             "}\n",
             mode_str,
             ip,
             has_creds ? "true" : "false",
             ssid_esc,
             serial_esc,
             owner_esc,
             owner_configured ? "true" : "false",
             OTS_FIRMWARE_VERSION);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

// Helper function for combined GET/POST requests (internal)
static esp_err_t handle_device(httpd_req_t *req) {
    // Combined GET/POST handler for /device
    if (req->method == HTTP_GET) {
        return handle_device_get(req);
    }
    
    if (req->method == HTTP_POST) {
        ESP_LOGI(TAG, "POST /device called");
        
        const int content_len = req->content_len;
        ESP_LOGI(TAG, "Content length: %d", content_len);
        
        if (content_len <= 0 || content_len > 256) {
            ESP_LOGW(TAG, "Invalid content length");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
            return ESP_OK;
        }

        char body[257];
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received <= 0) {
            ESP_LOGW(TAG, "Failed to receive body");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
            return ESP_OK;
        }
        body[received] = '\0';
        ESP_LOGI(TAG, "Received body: %s", body);

        char name[64];
        name[0] = '\0';
        (void)form_get_value(body, "ownerName", name, sizeof(name));
        ESP_LOGI(TAG, "Parsed name: %s", name);

        // Trim leading/trailing spaces (simple).
        while (name[0] == ' ') {
            memmove(name, name + 1, strlen(name));
        }
        size_t n = strlen(name);
        while (n > 0 && name[n - 1] == ' ') {
            name[n - 1] = '\0';
            n--;
        }

        if (name[0] == '\0') {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ownerName required");
            return ESP_OK;
        }

        (void)device_settings_init();
        esp_err_t ret = device_settings_set_owner(name);
        if (ret != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save");
            return ESP_OK;
        }

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Saved.\n");
        return ESP_OK;
    }
    
    // Method not supported
    httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
    return ESP_OK;
}

// Public wrapper for handle_device
esp_err_t wifi_config_handle_device(httpd_req_t *req) {
    return handle_device(req);
}

esp_err_t wifi_config_handle_api_scan(httpd_req_t *req) {
    // In portal mode, WiFi is AP-only; enable APSTA for scans.
    if (s_mode == WIFI_CONFIG_MODE_PORTAL) {
        esp_err_t mode_ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (mode_ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_set_mode(APSTA) failed: %s", esp_err_to_name(mode_ret));
        }
    }

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {.min = 50, .max = 150},
        },
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true /* block */);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ESP_OK;
    }

    uint16_t ap_num = 0;
    (void)esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num > 20) {
        ap_num = 20;
    }

    wifi_ap_record_t *recs = NULL;
    if (ap_num > 0) {
        recs = (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
        if (!recs) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
            return ESP_OK;
        }
        (void)esp_wifi_scan_get_ap_records(&ap_num, recs);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    httpd_resp_sendstr_chunk(req, "{\"aps\":[");
    bool first = true;
    for (uint16_t i = 0; i < ap_num; i++) {
        const wifi_ap_record_t *r = &recs[i];
        const char *ssid = (const char *)r->ssid;
        if (!ssid || ssid[0] == '\0') {
            continue;
        }

        char ssid_esc[96];
        (void)json_escape(ssid, ssid_esc, sizeof(ssid_esc));

        char item[220];
        snprintf(item, sizeof(item),
                 "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\"}",
                 first ? "" : ",", ssid_esc, (int)r->rssi, auth_mode_to_str(r->authmode));
        httpd_resp_sendstr_chunk(req, item);
        first = false;
    }
    httpd_resp_sendstr_chunk(req, "]}\n");
    httpd_resp_sendstr_chunk(req, NULL);

    free(recs);
    return ESP_OK;
}

static void captive_dns_start(void) {
    if (s_dns_task) {
        return;
    }
    s_dns_running = true;

    BaseType_t ok = xTaskCreate(dns_task, "captive_dns", 4096, NULL, 4, &s_dns_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create captive DNS task");
        s_dns_task = NULL;
        s_dns_running = false;
    }
}

static void captive_dns_stop(void) {
    s_dns_running = false;
    if (s_dns_sock >= 0) {
        shutdown(s_dns_sock, SHUT_RDWR);
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    // Task will exit on its own.
}

static void dns_task(void *arg) {
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket() failed: errno=%d", errno);
        s_dns_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    s_dns_sock = sock;

    int reuse = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CAPTIVE_DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "DNS bind() failed: errno=%d", errno);
        close(sock);
        s_dns_sock = -1;
        s_dns_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Captive DNS started on UDP/%u", (unsigned)CAPTIVE_DNS_PORT);

    // Receive buffer large enough for typical DNS queries.
    uint8_t rx[256];
    uint8_t tx[256];

    while (s_dns_running) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &from_len);
        if (n <= 0) {
            // socket closed or error; exit if we are stopping.
            if (!s_dns_running) {
                break;
            }
            continue;
        }

        // Minimal DNS parsing/response.
        // Header: 12 bytes
        if (n < 12) {
            continue;
        }

        // Only handle standard queries with exactly 1 question.
        uint16_t qdcount = (uint16_t)((rx[4] << 8) | rx[5]);
        if (qdcount != 1) {
            continue;
        }

        // Find end of QNAME.
        int offset = 12;
        while (offset < n && rx[offset] != 0) {
            offset += (int)rx[offset] + 1;
        }
        if (offset + 5 >= n) {
            continue;
        }
        // offset points at 0 terminator
        offset += 1;

        // QTYPE/QCLASS are 4 bytes
        const int question_end = offset + 4;
        if (question_end > n) {
            continue;
        }

        // Build response: header + original question + single A answer.
        memset(tx, 0, sizeof(tx));
        // Transaction ID
        tx[0] = rx[0];
        tx[1] = rx[1];
        // Flags: response, recursion not available, no error
        tx[2] = 0x81;
        tx[3] = 0x80;
        // QDCOUNT = 1
        tx[4] = 0x00;
        tx[5] = 0x01;
        // ANCOUNT = 1
        tx[6] = 0x00;
        tx[7] = 0x01;
        // NSCOUNT/ARCOUNT = 0
        tx[8] = 0x00;
        tx[9] = 0x00;
        tx[10] = 0x00;
        tx[11] = 0x00;

        int tx_len = 12;
        const int q_len = question_end - 12;
        if (tx_len + q_len + 16 > (int)sizeof(tx)) {
            continue;
        }
        memcpy(&tx[tx_len], &rx[12], (size_t)q_len);
        tx_len += q_len;

        // Answer section
        // NAME: pointer to 0x0c (start of QNAME)
        tx[tx_len++] = 0xC0;
        tx[tx_len++] = 0x0C;
        // TYPE: A
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x01;
        // CLASS: IN
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x01;
        // TTL: 0
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x00;
        // RDLENGTH: 4
        tx[tx_len++] = 0x00;
        tx[tx_len++] = 0x04;
        // RDATA: AP IP
        tx[tx_len++] = CAPTIVE_PORTAL_IP_A;
        tx[tx_len++] = CAPTIVE_PORTAL_IP_B;
        tx[tx_len++] = CAPTIVE_PORTAL_IP_C;
        tx[tx_len++] = CAPTIVE_PORTAL_IP_D;

        (void)sendto(sock, tx, (size_t)tx_len, 0, (struct sockaddr *)&from, from_len);
    }

    if (sock >= 0) {
        close(sock);
    }
    s_dns_sock = -1;
    s_dns_task = NULL;
    ESP_LOGI(TAG, "Captive DNS stopped");
    vTaskDelete(NULL);
}

esp_err_t webapp_server_init(uint16_t port) {
    s_port = (port == 0) ? 80 : port;
    return ESP_OK;
}

esp_err_t webapp_server_start(void) {
    if (s_server) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = s_port;
    config.ctrl_port = s_port + 1;
    config.max_uri_handlers = 16;  // Increase from default (8) to accommodate all handlers
    config.uri_match_fn = httpd_uri_match_wildcard;  // Enable wildcard matching

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
        s_server = NULL;
        return ret;
    }

    // API endpoints (highest priority - must match before wildcards)
    static const httpd_uri_t api_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = wifi_config_handle_api_status,
        .user_ctx = NULL,
    };

    static const httpd_uri_t api_scan = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = wifi_config_handle_api_scan,
        .user_ctx = NULL,
    };

    static const httpd_uri_t device_get = {
        .uri = "/device",
        .method = HTTP_GET,
        .handler = wifi_config_handle_device,
        .user_ctx = NULL,
    };
    
    static const httpd_uri_t device_post = {
        .uri = "/device",
        .method = HTTP_POST,
        .handler = wifi_config_handle_device,
        .user_ctx = NULL,
    };

    static const httpd_uri_t wifi_post = {
        .uri = "/wifi",
        .method = HTTP_POST,
        .handler = wifi_config_handle_wifi_post,
        .user_ctx = NULL,
    };

    static const httpd_uri_t wifi_clear = {
        .uri = "/wifi/clear",
        .method = HTTP_POST,
        .handler = wifi_config_handle_wifi_clear,
        .user_ctx = NULL,
    };

    static const httpd_uri_t factory_reset = {
        .uri = "/factory-reset",
        .method = HTTP_POST,
        .handler = wifi_config_handle_factory_reset,
        .user_ctx = NULL,
    };

    static const httpd_uri_t ota_upload = {
        .uri = "/ota/upload",
        .method = HTTP_POST,
        .handler = wifi_config_handle_ota_upload,
        .user_ctx = NULL,
    };

    // Webapp static files (specific paths)
    static const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = wifi_config_handle_webapp_get,
        .user_ctx = NULL,
    };

    static const httpd_uri_t index_html = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = wifi_config_handle_webapp_get,
        .user_ctx = NULL,
    };

    static const httpd_uri_t style_css = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = wifi_config_handle_webapp_get,
        .user_ctx = NULL,
    };

    static const httpd_uri_t app_js = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = wifi_config_handle_webapp_get,
        .user_ctx = NULL,
    };

    static const httpd_uri_t wifi_alias = {
        .uri = "/wifi",
        .method = HTTP_GET,
        .handler = wifi_config_handle_webapp_get,
        .user_ctx = NULL,
    };

    // Register handlers in order of specificity (most specific first)
    ret = httpd_register_uri_handler(s_server, &api_status);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /api/status: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &api_scan);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /api/scan: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &device_get);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /device GET: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &device_post);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /device POST: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &wifi_post);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /wifi POST: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &wifi_clear);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /wifi/clear: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &factory_reset);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /factory-reset: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &ota_upload);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /ota/upload: %s", esp_err_to_name(ret));
    }

    // Register webapp handlers (less specific, should come after API endpoints)
    ret = httpd_register_uri_handler(s_server, &root);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &index_html);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /index.html: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &style_css);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /style.css: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &app_js);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /app.js: %s", esp_err_to_name(ret));
    }

    ret = httpd_register_uri_handler(s_server, &wifi_alias);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register /wifi GET: %s", esp_err_to_name(ret));
    }

    // In portal mode, redirect all unknown paths to /.
    ret = httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, wifi_config_handle_404_redirect);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register 404 handler: %s", esp_err_to_name(ret));
    }

    // Only start captive DNS once the HTTP server (and thus LWIP/esp-netif) is up.
    // This avoids early-boot LWIP asserts when socket() is called too soon.
    if (s_mode == WIFI_CONFIG_MODE_PORTAL) {
        captive_dns_start();
    }

    ESP_LOGI(TAG, "HTTP server started on port %u with %d handlers registered", 
             (unsigned)s_port, 13);
    return ESP_OK;
}

esp_err_t webapp_server_stop(void) {
    if (!s_server) {
        return ESP_OK;
    }
    captive_dns_stop();
    httpd_stop(s_server);
    s_server = NULL;
    return ESP_OK;
}

void webapp_server_set_mode(wifi_config_mode_t mode) {
    const wifi_config_mode_t prev = s_mode;
    s_mode = mode;

    // Start/stop captive DNS based on portal mode, but only once the HTTP server
    // is running (i.e. after LWIP is initialized). When called early during boot,
    // defer DNS startup to webapp_server_start().
    if (!s_server) {
        return;
    }

    if (s_mode == WIFI_CONFIG_MODE_PORTAL && prev != WIFI_CONFIG_MODE_PORTAL) {
        captive_dns_start();
    } else if (s_mode != WIFI_CONFIG_MODE_PORTAL && prev == WIFI_CONFIG_MODE_PORTAL) {
        captive_dns_stop();
    }
}

wifi_config_mode_t webapp_server_get_mode(void) {
    return s_mode;
}
