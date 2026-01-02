#ifndef DNS_CAPTIVE_PORTAL_H
#define DNS_CAPTIVE_PORTAL_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @file dns_captive_portal.h
 * @brief Captive Portal DNS Server Component
 * 
 * Provides a minimal DNS server that responds to all queries with the
 * device's AP IP address. This enables captive portal functionality
 * by redirecting all DNS queries to the device's web interface.
 */

/**
 * @brief Start the captive portal DNS server
 * 
 * Creates a UDP socket on port 53 and responds to all DNS A queries
 * with the device's AP IP address (typically 192.168.4.1).
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL if task creation failed
 */
esp_err_t dns_captive_portal_start(void);

/**
 * @brief Stop the captive portal DNS server
 * 
 * Shuts down the DNS server task and closes the UDP socket.
 */
void dns_captive_portal_stop(void);

/**
 * @brief Check if captive portal DNS is running
 * 
 * @return true if DNS server is active, false otherwise
 */
bool dns_captive_portal_is_running(void);

#endif // DNS_CAPTIVE_PORTAL_H
