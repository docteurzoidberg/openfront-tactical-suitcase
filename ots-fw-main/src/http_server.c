/**
 * @file http_server.c
 * @brief Generic HTTP/HTTPS Server Core Implementation
 * 
 * Provides a reusable HTTP/HTTPS server with TLS support.
 * Manages a single httpd_handle_t instance that components can register with.
 */

#include "http_server.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "ws_handlers.h"
#include <string.h>

static const char *TAG = "HTTP_SERVER";

// Server state
static httpd_handle_t s_server = NULL;
static http_server_config_t s_config = {0};
static bool s_initialized = false;

esp_err_t http_server_init(const http_server_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        ESP_LOGW(TAG, "Server already initialized");
        return ESP_OK;
    }

    // Validate TLS configuration
    if (config->use_tls) {
        if (!config->cert_pem || config->cert_len == 0) {
            ESP_LOGE(TAG, "TLS enabled but no certificate provided");
            return ESP_ERR_INVALID_ARG;
        }
        if (!config->key_pem || config->key_len == 0) {
            ESP_LOGE(TAG, "TLS enabled but no private key provided");
            return ESP_ERR_INVALID_ARG;
        }
    }

    // Copy configuration
    memcpy(&s_config, config, sizeof(http_server_config_t));
    s_initialized = true;

    ESP_LOGI(TAG, "HTTP server initialized (port=%u, tls=%s)", 
             s_config.port, s_config.use_tls ? "yes" : "no");
    return ESP_OK;
}

esp_err_t http_server_start(void) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "Server not initialized - call http_server_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_server != NULL) {
        ESP_LOGW(TAG, "Server already started");
        return ESP_OK;
    }

    esp_err_t ret;

    // If no close_fn provided, use WebSocket handler's callback for proper cleanup
    httpd_close_func close_fn = s_config.close_fn;
    if (close_fn == NULL) {
#if CONFIG_HTTPD_WS_SUPPORT
        close_fn = ws_handlers_get_session_close_callback();
        ESP_LOGD(TAG, "Using WebSocket session close callback");
#endif
    }

    if (s_config.use_tls) {
        // HTTPS server configuration
        ESP_LOGI(TAG, "Starting HTTPS server on port %u", s_config.port);
        
        httpd_ssl_config_t ssl_config = HTTPD_SSL_CONFIG_DEFAULT();
        ssl_config.port_secure = s_config.port;
        ssl_config.httpd.ctrl_port = s_config.port + 1;
        ssl_config.httpd.max_open_sockets = s_config.max_open_sockets > 0 ? s_config.max_open_sockets : 7;
        ssl_config.httpd.max_uri_handlers = s_config.max_uri_handlers > 0 ? s_config.max_uri_handlers : 32;
        ssl_config.httpd.lru_purge_enable = true;
        ssl_config.httpd.close_fn = close_fn;
        
        // Set TLS credentials
        ssl_config.servercert = s_config.cert_pem;
        ssl_config.servercert_len = s_config.cert_len;
        ssl_config.prvtkey_pem = s_config.key_pem;
        ssl_config.prvtkey_len = s_config.key_len;
        
        ret = httpd_ssl_start(&s_server, &ssl_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTPS server: %s", esp_err_to_name(ret));
            s_server = NULL;
            return ret;
        }
        
        ESP_LOGI(TAG, "HTTPS server started successfully");
        ESP_LOGW(TAG, "Self-signed certificate - browsers will show security warning");
    } else {
        // HTTP server configuration
        ESP_LOGI(TAG, "Starting HTTP server on port %u", s_config.port);
        
        httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
        http_config.server_port = s_config.port;
        http_config.ctrl_port = s_config.port + 1;
        http_config.max_open_sockets = s_config.max_open_sockets > 0 ? s_config.max_open_sockets : 7;
        http_config.max_uri_handlers = s_config.max_uri_handlers > 0 ? s_config.max_uri_handlers : 32;
        http_config.lru_purge_enable = true;
        http_config.close_fn = close_fn;
        
        ret = httpd_start(&s_server, &http_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
            s_server = NULL;
            return ret;
        }
        
        ESP_LOGI(TAG, "HTTP server started successfully");
    }

    return ESP_OK;
}

esp_err_t http_server_stop(void) {
    if (s_server == NULL) {
        ESP_LOGW(TAG, "Server not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping HTTP server");
    
    if (s_config.use_tls) {
        httpd_ssl_stop(s_server);
    } else {
        httpd_stop(s_server);
    }
    
    s_server = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
    
    return ESP_OK;
}

bool http_server_is_running(void) {
    return s_server != NULL;
}

httpd_handle_t http_server_get_handle(void) {
    return s_server;
}

esp_err_t http_server_register_handler(const httpd_uri_t *uri_handler) {
    if (!uri_handler) {
        ESP_LOGE(TAG, "Handler cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_server == NULL) {
        ESP_LOGE(TAG, "Server not started - call http_server_start() first");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = httpd_register_uri_handler(s_server, uri_handler);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register handler for %s: %s", 
                 uri_handler->uri, esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "Registered handler: %s %s", 
                 uri_handler->method == HTTP_GET ? "GET" : 
                 uri_handler->method == HTTP_POST ? "POST" : "OTHER",
                 uri_handler->uri);
    }
    
    return ret;
}

esp_err_t http_server_register_err_handler(httpd_err_code_t error, httpd_err_handler_func_t handler) {
    if (!handler) {
        ESP_LOGE(TAG, "Error handler cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_server == NULL) {
        ESP_LOGE(TAG, "Server not started - call http_server_start() first");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = httpd_register_err_handler(s_server, error, handler);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register error handler: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "Registered error handler for code %d", error);
    }
    
    return ret;
}

uint16_t http_server_get_port(void) {
    return s_config.port;
}

bool http_server_is_secure(void) {
    return s_config.use_tls;
}
