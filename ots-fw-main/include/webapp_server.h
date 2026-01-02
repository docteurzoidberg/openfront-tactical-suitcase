#ifndef WEBAPP_SERVER_H
#define WEBAPP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Server mode: normal (station) or portal (AP with captive portal)
typedef enum {
    WIFI_CONFIG_MODE_NORMAL = 0,
    WIFI_CONFIG_MODE_PORTAL = 1,
} wifi_config_mode_t;

// Server lifecycle management
esp_err_t webapp_server_init(uint16_t port);
esp_err_t webapp_server_start(void);
esp_err_t webapp_server_stop(void);

// Mode control (switches between normal and captive portal modes)
void webapp_server_set_mode(wifi_config_mode_t mode);
wifi_config_mode_t webapp_server_get_mode(void);

// HTTP request handlers (exported for potential reuse)
esp_err_t wifi_config_handle_404_redirect(httpd_req_t *req, httpd_err_code_t err);
esp_err_t wifi_config_handle_webapp_get(httpd_req_t *req);
esp_err_t wifi_config_handle_api_status(httpd_req_t *req);
esp_err_t wifi_config_handle_api_scan(httpd_req_t *req);
esp_err_t wifi_config_handle_device(httpd_req_t *req);
esp_err_t wifi_config_handle_factory_reset(httpd_req_t *req);
esp_err_t wifi_config_handle_wifi_post(httpd_req_t *req);
esp_err_t wifi_config_handle_wifi_clear(httpd_req_t *req);
esp_err_t wifi_config_handle_ota_upload(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // WEBAPP_SERVER_H
