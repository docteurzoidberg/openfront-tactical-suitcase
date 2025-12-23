#ifndef IMPROV_SERIAL_H
#define IMPROV_SERIAL_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Improv WiFi provisioning via serial interface
 * 
 * Implements the Improv Serial protocol for WiFi provisioning:
 * https://www.improv-wifi.com/serial/
 * 
 * Features:
 * - Device identification
 * - WiFi credential provisioning
 * - NVS credential storage
 * - Status reporting
 * - Error reporting
 */

/**
 * @brief Improv Serial state
 */
typedef enum {
    IMPROV_STATE_READY = 0x02,           // Ready to provision
    IMPROV_STATE_PROVISIONING = 0x03,    // Currently provisioning
    IMPROV_STATE_PROVISIONED = 0x04      // Already provisioned
} improv_state_t;

/**
 * @brief Improv Serial error codes
 */
typedef enum {
    IMPROV_ERROR_NONE = 0x00,
    IMPROV_ERROR_INVALID_RPC = 0x01,
    IMPROV_ERROR_UNKNOWN_RPC = 0x02,
    IMPROV_ERROR_UNABLE_TO_CONNECT = 0x03,
    IMPROV_ERROR_NOT_AUTHORIZED = 0x04,
    IMPROV_ERROR_BAD_HOSTNAME = 0x05,
    IMPROV_ERROR_UNKNOWN = 0xFF
} improv_error_t;

/**
 * @brief Provisioning callback
 * @param success True if provisioning succeeded
 * @param ssid SSID that was provisioned (or attempted)
 */
typedef void (*improv_provision_callback_t)(bool success, const char *ssid);

/**
 * @brief Initialize Improv Serial
 * @return ESP_OK on success
 */
esp_err_t improv_serial_init(void);

/**
 * @brief Start Improv Serial task
 * @return ESP_OK on success
 */
esp_err_t improv_serial_start(void);

/**
 * @brief Stop Improv Serial task
 */
void improv_serial_stop(void);

/**
 * @brief Set provisioning callback
 * @param callback Callback function
 */
void improv_serial_set_callback(improv_provision_callback_t callback);

/**
 * @brief Update Improv state
 * @param state New state
 */
void improv_serial_set_state(improv_state_t state);

/**
 * @brief Send error to client
 * @param error Error code
 */
void improv_serial_send_error(improv_error_t error);

/**
 * @brief Check if device is provisioned
 * @return True if WiFi credentials exist in NVS
 */
bool improv_serial_is_provisioned(void);

/**
 * @brief Clear stored WiFi credentials (factory reset)
 * @return ESP_OK on success
 */
esp_err_t improv_serial_clear_credentials(void);

#ifdef __cplusplus
}
#endif

#endif // IMPROV_SERIAL_H
