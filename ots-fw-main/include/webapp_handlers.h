#ifndef WEBAPP_HANDLERS_H
#define WEBAPP_HANDLERS_H

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file webapp_handlers.h
 * @brief WebApp UI and Configuration API Handlers
 * 
 * Provides HTTP request handlers for:
 * - Embedded webapp static files (HTML/CSS/JS)
 * - Device configuration REST API
 * - WiFi provisioning and scanning
 * - Factory reset
 * - Captive portal mode support
 * 
 * This component is independent of WebSocket/game logic.
 */

/**
 * Webapp operation mode
 */
typedef enum {
    WEBAPP_MODE_NORMAL = 0,          // Normal operation mode
    WEBAPP_MODE_CAPTIVE_PORTAL = 1,  // Captive portal mode (redirects all requests)
} webapp_mode_t;

/**
 * Register webapp handlers with HTTP server
 * 
 * Registers all webapp static files, API endpoints, and error handlers.
 * Must be called after HTTP server is started.
 * 
 * @param server HTTP server handle from http_server_get_handle()
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t webapp_handlers_register(httpd_handle_t server);

/**
 * Set webapp operation mode
 * 
 * @param mode Operation mode (normal or captive portal)
 */
void webapp_handlers_set_mode(webapp_mode_t mode);

/**
 * Get current webapp operation mode
 * 
 * @return Current operation mode
 */
webapp_mode_t webapp_handlers_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif // WEBAPP_HANDLERS_H
