/**
 * @file nvs_storage.h
 * @brief Centralized NVS (Non-Volatile Storage) abstraction layer
 *
 * This module provides a unified interface for NVS operations across the firmware,
 * reducing code duplication and providing consistent error handling.
 *
 * Supported namespaces:
 * - "wifi": WiFi credentials (ssid, password)
 * - "device": Device settings (owner_name, serial_number)
 *
 * @note This is NOT an IDF component - it's internal firmware code in src/
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NVS storage subsystem
 *
 * Must be called once during firmware boot before any NVS operations.
 * Initializes NVS flash and handles partition erasing if needed.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_storage_init(void);

/**
 * @brief Read a string value from NVS
 *
 * @param[in] namespace NVS namespace (e.g., "wifi", "device")
 * @param[in] key Key name within the namespace
 * @param[out] value Buffer to store the string value
 * @param[in] max_len Maximum length of the buffer (including null terminator)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 * @return ESP_ERR_INVALID_ARG if parameters are NULL
 * @return Other ESP-IDF NVS error codes
 */
esp_err_t nvs_storage_get_string(const char *namespace, const char *key,
                                  char *value, size_t max_len);

/**
 * @brief Write a string value to NVS
 *
 * @param[in] namespace NVS namespace (e.g., "wifi", "device")
 * @param[in] key Key name within the namespace
 * @param[in] value String value to store (null-terminated)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if parameters are NULL
 * @return Other ESP-IDF NVS error codes
 */
esp_err_t nvs_storage_set_string(const char *namespace, const char *key,
                                  const char *value);

/**
 * @brief Check if a key exists in NVS
 *
 * @param[in] namespace NVS namespace (e.g., "wifi", "device")
 * @param[in] key Key name within the namespace
 *
 * @return true if key exists, false otherwise
 */
bool nvs_storage_exists(const char *namespace, const char *key);

/**
 * @brief Erase a specific key from NVS
 *
 * @param[in] namespace NVS namespace (e.g., "wifi", "device")
 * @param[in] key Key name within the namespace
 *
 * @return ESP_OK on success (also if key didn't exist)
 * @return ESP_ERR_INVALID_ARG if parameters are NULL
 * @return Other ESP-IDF NVS error codes
 */
esp_err_t nvs_storage_erase_key(const char *namespace, const char *key);

/**
 * @brief Erase all keys in a namespace
 *
 * @param[in] namespace NVS namespace to clear (e.g., "wifi", "device")
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if namespace is NULL
 * @return Other ESP-IDF NVS error codes
 */
esp_err_t nvs_storage_erase_namespace(const char *namespace);

#ifdef __cplusplus
}
#endif

#endif // NVS_STORAGE_H
