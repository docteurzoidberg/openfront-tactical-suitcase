#include "wifi_credentials.h"
#include "config.h"
#include "nvs_storage.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OTS_WIFI_CREDS";
static const char *NVS_NAMESPACE = "wifi";
static const char *NVS_KEY_SSID = "ssid";
static const char *NVS_KEY_PASSWORD = "password";

static bool credentials_initialized = false;

esp_err_t wifi_credentials_init(void) {
    if (credentials_initialized) {
        return ESP_OK;
    }
    
    // NVS should already be initialized by nvs_storage_init() in main.c
    ESP_LOGI(TAG, "WiFi credentials storage initialized");
    credentials_initialized = true;
    return ESP_OK;
}

esp_err_t wifi_credentials_load(wifi_credentials_t *creds) {
    if (!creds) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear credentials first
    memset(creds, 0, sizeof(wifi_credentials_t));
    
    // Read SSID
    esp_err_t ret = nvs_storage_get_string(NVS_NAMESPACE, NVS_KEY_SSID,
                                            creds->ssid, sizeof(creds->ssid));
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "No stored credentials found");
        }
        return (ret == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NOT_FOUND : ret;
    }
    
    // Read password
    ret = nvs_storage_get_string(NVS_NAMESPACE, NVS_KEY_PASSWORD,
                                  creds->password, sizeof(creds->password));
    if (ret != ESP_OK) {
        return ret;
    }
    
    ESP_LOGI(TAG, "Loaded credentials from NVS: SSID=%s", creds->ssid);
    return ESP_OK;
}

esp_err_t wifi_credentials_save(const wifi_credentials_t *creds) {
    if (!creds || strlen(creds->ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Write SSID
    esp_err_t ret = nvs_storage_set_string(NVS_NAMESPACE, NVS_KEY_SSID, creds->ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Write password
    ret = nvs_storage_set_string(NVS_NAMESPACE, NVS_KEY_PASSWORD, creds->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write password: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Saved credentials to NVS: SSID=%s", creds->ssid);
    return ESP_OK;
}

bool wifi_credentials_exist(void) {
    return nvs_storage_exists(NVS_NAMESPACE, NVS_KEY_SSID);
}

esp_err_t wifi_credentials_clear(void) {
    ESP_LOGI(TAG, "=== Clearing WiFi credentials ===");
    
    esp_err_t ret = nvs_storage_erase_namespace(NVS_NAMESPACE);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ WiFi credentials cleared successfully");
    } else {
        ESP_LOGE(TAG, "✗ Failed to clear WiFi credentials: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t wifi_credentials_get(wifi_credentials_t *creds) {
    if (!creds) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Try to load from NVS first
    esp_err_t ret = wifi_credentials_load(creds);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Using credentials from NVS");
        return ESP_OK;
    }
    
    // Fall back to config.h
    ESP_LOGI(TAG, "Using fallback credentials from config.h");
    strncpy(creds->ssid, WIFI_SSID, sizeof(creds->ssid) - 1);
    strncpy(creds->password, WIFI_PASSWORD, sizeof(creds->password) - 1);
    creds->ssid[sizeof(creds->ssid) - 1] = '\0';
    creds->password[sizeof(creds->password) - 1] = '\0';
    
    return ESP_OK;
}
