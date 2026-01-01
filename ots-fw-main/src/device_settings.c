#include "device_settings.h"

#include "config.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"

#include "nvs.h"

#include <string.h>

static const char *TAG = "OTS_DEVICE";

static const char *NVS_NAMESPACE = "device";
static const char *NVS_KEY_OWNER = "owner_name";
static const char *NVS_KEY_SERIAL = "serial_number";

static bool s_initialized = false;

esp_err_t device_settings_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    // NVS is initialized by main.
    s_initialized = true;
    return ESP_OK;
}

static esp_err_t nvs_get_str_safe(const char *key, char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = out_len;
    ret = nvs_get_str(handle, key, out, &len);
    nvs_close(handle);
    return ret;
}

bool device_settings_owner_exists(void) {
    char owner[64];
    esp_err_t ret = nvs_get_str_safe(NVS_KEY_OWNER, owner, sizeof(owner));
    return (ret == ESP_OK && owner[0] != '\0');
}

esp_err_t device_settings_get_owner(char *out, size_t out_len) {
    esp_err_t ret = nvs_get_str_safe(NVS_KEY_OWNER, out, out_len);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    // Fallback to build-time owner (config.h) if no user owner stored.
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(out, OTS_DEVICE_OWNER, out_len - 1);
    out[out_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t device_settings_set_owner(const char *owner) {
    if (!owner) {
        return ESP_ERR_INVALID_ARG;
    }

    // Keep it small and safe for later UI/JSON usage.
    const size_t len = strnlen(owner, 63);
    if (len == 0 || owner[0] == ' ') {
        return ESP_ERR_INVALID_ARG;
    }
    if (len >= 63) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(handle, NVS_KEY_OWNER, owner);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Owner name set: %s", owner);
    } else {
        ESP_LOGE(TAG, "Failed to set owner: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void serial_from_mac(char *out, size_t out_len) {
    uint8_t mac[6] = {0};
    // ESP_MAC_WIFI_STA is stable per device.
    (void)esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_len, "OTS-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

esp_err_t device_settings_get_serial(char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_get_str_safe(NVS_KEY_SERIAL, out, out_len);
    if (ret == ESP_OK && out[0] != '\0') {
        return ESP_OK;
    }

    // Prefer build-time serial if defined, else derive from MAC.
    if (OTS_DEVICE_SERIAL_NUMBER[0] != '\0') {
        strncpy(out, OTS_DEVICE_SERIAL_NUMBER, out_len - 1);
        out[out_len - 1] = '\0';
        return ESP_OK;
    }

    serial_from_mac(out, out_len);
    return ESP_OK;
}

esp_err_t device_settings_factory_reset(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Erase owner name only (keep serial number)
    ret = nvs_erase_key(handle, NVS_KEY_OWNER);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase owner: %s", esp_err_to_name(ret));
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Factory reset complete (owner cleared, serial kept)");
    } else {
        ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(ret));
    }

    return ret;
}
