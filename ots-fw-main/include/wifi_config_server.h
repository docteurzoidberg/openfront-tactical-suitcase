#ifndef WIFI_CONFIG_SERVER_H
#define WIFI_CONFIG_SERVER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_CONFIG_MODE_NORMAL = 0,
    WIFI_CONFIG_MODE_PORTAL = 1,
} wifi_config_mode_t;

esp_err_t wifi_config_server_init(uint16_t port);
esp_err_t wifi_config_server_start(void);
esp_err_t wifi_config_server_stop(void);

void wifi_config_server_set_mode(wifi_config_mode_t mode);
wifi_config_mode_t wifi_config_server_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CONFIG_SERVER_H
