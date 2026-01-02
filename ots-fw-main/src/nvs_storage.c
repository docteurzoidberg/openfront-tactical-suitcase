/**
 * @file nvs_storage.c
 * @brief Centralized NVS (Non-Volatile Storage) implementation
 *
 * This module consolidates all NVS operations to eliminate duplicate code
 * across wifi_credentials.c, device_settings.c, and serial_commands.c.
 */

#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "nvs_storage";

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t nvs_storage_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS storage subsystem");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_LOGW(TAG, "NVS partition needs erasing, performing erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "NVS storage initialized successfully");
    return ESP_OK;
}

esp_err_t nvs_storage_get_string(const char *namespace, const char *key,
                                  char *value, size_t max_len)
{
    if (!namespace || !key || !value || max_len == 0) {
        ESP_LOGE(TAG, "Invalid arguments to nvs_storage_get_string");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s",
                     namespace, esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Get required size first
    size_t required_size;
    ret = nvs_get_str(handle, key, NULL, &required_size);
    if (ret != ESP_OK) {
        nvs_close(handle);
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to get size for key '%s' in namespace '%s': %s",
                     key, namespace, esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Check if buffer is large enough
    if (required_size > max_len) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Buffer too small for key '%s' in namespace '%s': "
                 "need %zu bytes, have %zu bytes",
                 key, namespace, required_size, max_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Read the actual value
    ret = nvs_get_str(handle, key, value, &required_size);
    nvs_close(handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read key '%s' from namespace '%s': %s",
                 key, namespace, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Read key '%s' from namespace '%s': %zu bytes",
             key, namespace, strlen(value));
    return ESP_OK;
}

esp_err_t nvs_storage_set_string(const char *namespace, const char *key,
                                  const char *value)
{
    if (!namespace || !key || !value) {
        ESP_LOGE(TAG, "Invalid arguments to nvs_storage_set_string");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' for write: %s",
                 namespace, esp_err_to_name(ret));
        return ret;
    }
    
    // Write the value
    ret = nvs_set_str(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write key '%s' to namespace '%s': %s",
                 key, namespace, esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(handle);
    nvs_close(handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit key '%s' in namespace '%s': %s",
                 key, namespace, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Wrote key '%s' to namespace '%s': %zu bytes",
             key, namespace, strlen(value));
    return ESP_OK;
}

bool nvs_storage_exists(const char *namespace, const char *key)
{
    if (!namespace || !key) {
        ESP_LOGE(TAG, "Invalid arguments to nvs_storage_exists");
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    // Try to get size - if key exists, this succeeds
    size_t required_size;
    ret = nvs_get_str(handle, key, NULL, &required_size);
    nvs_close(handle);
    
    return (ret == ESP_OK);
}

esp_err_t nvs_storage_erase_key(const char *namespace, const char *key)
{
    if (!namespace || !key) {
        ESP_LOGE(TAG, "Invalid arguments to nvs_storage_erase_key");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' for erase: %s",
                 namespace, esp_err_to_name(ret));
        return ret;
    }
    
    // Erase the key (ESP_ERR_NVS_NOT_FOUND is OK)
    ret = nvs_erase_key(handle, key);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase key '%s' from namespace '%s': %s",
                 key, namespace, esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(handle);
    nvs_close(handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit erase of key '%s' in namespace '%s': %s",
                 key, namespace, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Erased key '%s' from namespace '%s'", key, namespace);
    return ESP_OK;
}

esp_err_t nvs_storage_erase_namespace(const char *namespace)
{
    if (!namespace) {
        ESP_LOGE(TAG, "Invalid argument to nvs_storage_erase_namespace");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "Namespace '%s' doesn't exist (already empty)", namespace);
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' for erase: %s",
                 namespace, esp_err_to_name(ret));
        return ret;
    }
    
    // Erase all keys in namespace
    ret = nvs_erase_all(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase namespace '%s': %s",
                 namespace, esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(handle);
    nvs_close(handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit erase of namespace '%s': %s",
                 namespace, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Erased all keys in namespace '%s'", namespace);
    return ESP_OK;
}
