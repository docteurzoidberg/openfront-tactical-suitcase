#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file http_server.h
 * @brief Generic HTTP/HTTPS Server Core
 * 
 * Provides a reusable HTTP/HTTPS server component with TLS support.
 * Other components can register their handlers with this core server.
 * 
 * Architecture:
 * - Single httpd_handle_t instance managed by this core
 * - Components register handlers via http_server_register_handler()
 * - Supports both HTTP and HTTPS (TLS) modes
 * - Lifecycle: init → start → [register handlers] → stop
 */

/**
 * HTTP server configuration
 */
typedef struct {
    uint16_t port;                  // Server port (80 for HTTP, 443 or 3000 for HTTPS)
    bool use_tls;                   // Enable TLS (HTTPS)
    const uint8_t *cert_pem;        // TLS certificate (PEM format, NULL if !use_tls)
    size_t cert_len;                // Certificate length
    const uint8_t *key_pem;         // TLS private key (PEM format, NULL if !use_tls)
    size_t key_len;                 // Private key length
    uint8_t max_open_sockets;       // Maximum concurrent connections
    uint16_t max_uri_handlers;      // Maximum number of URI handlers
    httpd_close_func close_fn;      // Optional session close callback (for WebSocket cleanup)
} http_server_config_t;

/**
 * Initialize HTTP server with configuration
 * 
 * @param config Server configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_server_init(const http_server_config_t *config);

/**
 * Start the HTTP server
 * Must be called after http_server_init() and before registering handlers
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_server_start(void);

/**
 * Stop the HTTP server
 * Unregisters all handlers and releases resources
 * 
 * @return ESP_OK on success
 */
esp_err_t http_server_stop(void);

/**
 * Check if server is running
 * 
 * @return true if server is started, false otherwise
 */
bool http_server_is_running(void);

/**
 * Get the server handle for advanced operations
 * Use this to register handlers or perform low-level operations
 * 
 * @return httpd_handle_t or NULL if server not started
 */
httpd_handle_t http_server_get_handle(void);

/**
 * Register a URI handler
 * 
 * @param uri_handler Handler configuration (uri, method, handler function)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_server_register_handler(const httpd_uri_t *uri_handler);

/**
 * Register an error handler
 * 
 * @param error HTTP error code (e.g., HTTPD_404_NOT_FOUND)
 * @param handler Error handler function
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_server_register_err_handler(httpd_err_code_t error, httpd_err_handler_func_t handler);

/**
 * Get server port
 * 
 * @return Configured server port
 */
uint16_t http_server_get_port(void);

/**
 * Check if server is using TLS
 * 
 * @return true if HTTPS, false if HTTP
 */
bool http_server_is_secure(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H
