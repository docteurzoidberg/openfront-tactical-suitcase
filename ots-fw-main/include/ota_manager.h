#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief OTA progress callback
 * 
 * @param bytes_received Bytes received so far
 * @param total_bytes Total bytes to receive
 * @param progress_percent Progress percentage (0-100)
 */
typedef void (*ota_progress_callback_t)(int bytes_received, int total_bytes, int progress_percent);

/**
 * @brief OTA completion callback
 * 
 * @param success true if OTA completed successfully
 * @param error_msg Error message if failed (NULL if success)
 */
typedef void (*ota_complete_callback_t)(bool success, const char *error_msg);

/**
 * @brief Initialize OTA manager
 * 
 * Sets up HTTP server for OTA updates
 * Must be called after network is connected
 * 
 * @param port OTA HTTP server port
 * @param hostname mDNS hostname for OTA service
 * @return ESP_OK on success
 */
esp_err_t ota_manager_init(uint16_t port, const char *hostname);

/**
 * @brief Start OTA HTTP server
 * 
 * @return ESP_OK on success
 */
esp_err_t ota_manager_start(void);

/**
 * @brief Stop OTA HTTP server
 * 
 * @return ESP_OK on success
 */
esp_err_t ota_manager_stop(void);

/**
 * @brief Check if OTA update is in progress
 * 
 * @return true if OTA is currently running
 */
bool ota_manager_is_updating(void);

/**
 * @brief Set progress callback
 * 
 * @param callback Function to call with progress updates
 */
void ota_manager_set_progress_callback(ota_progress_callback_t callback);

/**
 * @brief Set completion callback
 * 
 * @param callback Function to call when OTA completes or fails
 */
void ota_manager_set_complete_callback(ota_complete_callback_t callback);

#endif // OTA_MANAGER_H
