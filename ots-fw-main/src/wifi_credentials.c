#include "wifi_credentials.h"
#include "config.h"
#include "nvs_flash.h"
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
    
    // NVS should already be initialized by main.c
    ESP_LOGI(TAG, "WiFi credentials storage initialized");
    credentials_initialized = true;
    return ESP_OK;
}

esp_err_t wifi_credentials_load(wifi_credentials_t *creds) {
    if (!creds) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "No stored credentials found");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read SSID
    size_t ssid_len = sizeof(creds->ssid);
    ret = nvs_get_str(nvs_handle, NVS_KEY_SSID, creds->ssid, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Read password
    size_t password_len = sizeof(creds->password);
    ret = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, creds->password, &password_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded credentials from NVS: SSID=%s", creds->ssid);
    return ESP_OK;
}

esp_err_t wifi_credentials_save(const wifi_credentials_t *creds) {
    if (!creds || strlen(creds->ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Write SSID
    ret = nvs_set_str(nvs_handle, NVS_KEY_SSID, creds->ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Write password
    ret = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, creds->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write password: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Saved credentials to NVS: SSID=%s", creds->ssid);
    return ESP_OK;
}

bool wifi_credentials_exist(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    // Try to read SSID
    char ssid[WIFI_CREDENTIALS_MAX_SSID_LEN];
    size_t ssid_len = sizeof(ssid);
    ret = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &ssid_len);
    nvs_close(nvs_handle);
    
    return (ret == ESP_OK && strlen(ssid) > 0);
}

esp_err_t wifi_credentials_clear(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Erase both keys
    nvs_erase_key(nvs_handle, NVS_KEY_SSID);
    nvs_erase_key(nvs_handle, NVS_KEY_PASSWORD);
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Cleared stored credentials");
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
