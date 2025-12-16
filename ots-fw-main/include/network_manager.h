#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Network event types
 */
typedef enum {
    NETWORK_EVENT_CONNECTED,
    NETWORK_EVENT_DISCONNECTED,
    NETWORK_EVENT_GOT_IP
} network_event_type_t;

/**
 * @brief Network event callback
 * 
 * @param event_type Type of network event
 * @param ip_address IP address string (only for GOT_IP event), NULL otherwise
 */
typedef void (*network_event_callback_t)(network_event_type_t event_type, const char *ip_address);

/**
 * @brief Initialize network manager
 * 
 * Sets up WiFi, mDNS, and network event handling
 * 
 * @param ssid WiFi SSID to connect to
 * @param password WiFi password
 * @param hostname mDNS hostname
 * @return ESP_OK on success
 */
esp_err_t network_manager_init(const char *ssid, const char *password, const char *hostname);

/**
 * @brief Start network services (WiFi connection)
 * 
 * @return ESP_OK on success
 */
esp_err_t network_manager_start(void);

/**
 * @brief Stop network services
 * 
 * @return ESP_OK on success
 */
esp_err_t network_manager_stop(void);

/**
 * @brief Check if network is connected
 * 
 * @return true if WiFi is connected and has IP
 */
bool network_manager_is_connected(void);

/**
 * @brief Get current IP address
 * 
 * @param ip_str Buffer to store IP address string (min 16 bytes)
 * @param len Length of buffer
 * @return ESP_OK if IP address retrieved, ESP_FAIL otherwise
 */
esp_err_t network_manager_get_ip(char *ip_str, size_t len);

/**
 * @brief Set network event callback
 * 
 * @param callback Function to call on network events
 */
void network_manager_set_event_callback(network_event_callback_t callback);

/**
 * @brief Reconnect to WiFi
 * 
 * @return ESP_OK on success
 */
esp_err_t network_manager_reconnect(void);

#endif // NETWORK_MANAGER_H
