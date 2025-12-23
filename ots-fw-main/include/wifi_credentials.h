#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi credential storage using NVS
 * 
 * Provides persistent storage for WiFi credentials with fallback to
 * hardcoded values in config.h
 */

#define WIFI_CREDENTIALS_MAX_SSID_LEN 32
#define WIFI_CREDENTIALS_MAX_PASSWORD_LEN 64

/**
 * @brief WiFi credentials structure
 */
typedef struct {
    char ssid[WIFI_CREDENTIALS_MAX_SSID_LEN];
    char password[WIFI_CREDENTIALS_MAX_PASSWORD_LEN];
} wifi_credentials_t;

/**
 * @brief Initialize WiFi credentials storage
 * @return ESP_OK on success
 */
esp_err_t wifi_credentials_init(void);

/**
 * @brief Load WiFi credentials from NVS
 * @param creds Output credentials structure
 * @return ESP_OK if credentials found, ESP_ERR_NOT_FOUND if not stored
 */
esp_err_t wifi_credentials_load(wifi_credentials_t *creds);

/**
 * @brief Save WiFi credentials to NVS
 * @param creds Credentials to save
 * @return ESP_OK on success
 */
esp_err_t wifi_credentials_save(const wifi_credentials_t *creds);

/**
 * @brief Check if credentials are stored in NVS
 * @return True if credentials exist
 */
bool wifi_credentials_exist(void);

/**
 * @brief Clear stored credentials from NVS
 * @return ESP_OK on success
 */
esp_err_t wifi_credentials_clear(void);

/**
 * @brief Get credentials (NVS or fallback to config.h)
 * @param creds Output credentials structure
 * @return ESP_OK on success
 */
esp_err_t wifi_credentials_get(wifi_credentials_t *creds);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CREDENTIALS_H
