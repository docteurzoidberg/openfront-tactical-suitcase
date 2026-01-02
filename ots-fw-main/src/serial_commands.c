#include "serial_commands.h"

#include "config.h"
#include "wifi_credentials.h"
#include "device_settings.h"
#include "nvs_storage.h"

#include "esp_log.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "OTS_SERIAL_CMD";

#define CMD_UART UART_NUM_0
#define CMD_BUF_LEN 192

static TaskHandle_t s_task = NULL;

static void trim_trailing(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || isspace((unsigned char)s[n - 1]))) {
        s[n - 1] = '\0';
        n--;
    }
}

static char *skip_ws(char *s) {
    while (s && *s && isspace((unsigned char)*s)) s++;
    return s;
}

static char *next_token(char **inout) {
    if (!inout || !*inout) return NULL;
    char *s = skip_ws(*inout);
    if (*s == '\0') {
        *inout = s;
        return NULL;
    }
    char *start = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (*s) {
        *s = '\0';
        s++;
    }
    *inout = s;
    return start;
}

static void handle_line(char *line) {
    trim_trailing(line);
    char *p = skip_ws(line);
    if (!p || *p == '\0') return;

    char *cursor = p;
    char *cmd = next_token(&cursor);
    if (!cmd) return;

    if (strcmp(cmd, "wifi-clear") == 0) {
        esp_err_t ret = wifi_credentials_clear();
        ESP_LOGI(TAG, "wifi-clear: %s", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
        return;
    }

    if (strcmp(cmd, "wifi-provision") == 0 ||
        strcmp(cmd, "wifi-provisioning") == 0 ||
        strcmp(cmd, "wifi-provisionning") == 0) {
        char *ssid = next_token(&cursor);
        char *password = next_token(&cursor);
        if (!ssid || !password) {
            ESP_LOGW(TAG, "Usage: wifi-provision <ssid> <password>");
            return;
        }

        wifi_credentials_t creds;
        memset(&creds, 0, sizeof(creds));
        strncpy(creds.ssid, ssid, sizeof(creds.ssid) - 1);
        strncpy(creds.password, password, sizeof(creds.password) - 1);

        esp_err_t ret = wifi_credentials_save(&creds);
        ESP_LOGI(TAG, "wifi-provision: %s (ssid=%s)", esp_err_to_name(ret), creds.ssid);
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
        }
        return;
    }

    if (strcmp(cmd, "wifi-status") == 0) {
        wifi_credentials_t creds;
        memset(&creds, 0, sizeof(creds));
        esp_err_t ret = wifi_credentials_load(&creds);
        ESP_LOGI(TAG, "wifi-status: stored=%s ssid=%s", (ret == ESP_OK) ? "yes" : "no", (ret == ESP_OK) ? creds.ssid : "<none>");
        return;
    }

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "fw-version") == 0) {
        ESP_LOGI(TAG, "Firmware: %s v%s", OTS_FIRMWARE_NAME, OTS_FIRMWARE_VERSION);
        return;
    }

    if (strcmp(cmd, "reboot") == 0 || strcmp(cmd, "reset") == 0) {
        ESP_LOGI(TAG, "%s: rebooting...", cmd);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
        return;
    }

    if (strcmp(cmd, "nvs") == 0) {
        char *subcmd = next_token(&cursor);
        if (!subcmd) {
            ESP_LOGW(TAG, "Usage: nvs set <owner_name|serial_number> <value> | nvs erase <owner_name|serial_number> | nvs get <owner_name|serial_number>");
            return;
        }

        if (strcmp(subcmd, "set") == 0) {
            char *key = next_token(&cursor);
            if (!key) {
                ESP_LOGW(TAG, "Usage: nvs set <owner_name|serial_number> <value>");
                return;
            }
            char *value = skip_ws(cursor); // rest of line is the value
            if (!value || *value == '\0') {
                ESP_LOGW(TAG, "Usage: nvs set <owner_name|serial_number> <value>");
                return;
            }
            
            if (strcmp(key, "owner_name") == 0) {
                esp_err_t ret = device_settings_set_owner(value);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Owner name set to: %s", value);
                } else {
                    ESP_LOGE(TAG, "Failed to set owner name: %s", esp_err_to_name(ret));
                }
            } else if (strcmp(key, "serial_number") == 0) {
                esp_err_t ret = device_settings_set_serial(value);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Serial number set to: %s", value);
                } else {
                    ESP_LOGE(TAG, "Failed to set serial number: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGW(TAG, "Unknown key: %s", key);
                ESP_LOGW(TAG, "Usage: nvs set <owner_name|serial_number> <value>");
            }
            return;
        }

        if (strcmp(subcmd, "erase") == 0) {
            char *key = next_token(&cursor);
            if (!key) {
                ESP_LOGW(TAG, "Usage: nvs erase <owner_name|serial_number>");
                return;
            }
            
            const char *nvs_key = NULL;
            if (strcmp(key, "owner_name") == 0) {
                nvs_key = "owner_name";
            } else if (strcmp(key, "serial_number") == 0) {
                nvs_key = "serial_number";
            } else {
                ESP_LOGW(TAG, "Unknown key: %s", key);
                ESP_LOGW(TAG, "Usage: nvs erase <owner_name|serial_number>");
                return;
            }
            
            // Use centralized NVS storage API
            esp_err_t ret = nvs_storage_erase_key("device", nvs_key);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "%s cleared", key);
            } else {
                ESP_LOGE(TAG, "Failed to clear %s: %s", key, esp_err_to_name(ret));
            }
            return;
        }

        if (strcmp(subcmd, "get") == 0) {
            char *key = next_token(&cursor);
            if (!key) {
                ESP_LOGW(TAG, "Usage: nvs get <owner_name|serial_number>");
                return;
            }
            
            if (strcmp(key, "owner_name") == 0) {
                char name[64];
                esp_err_t ret = device_settings_get_owner(name, sizeof(name));
                if (ret == ESP_OK && name[0] != '\0') {
                    ESP_LOGI(TAG, "Owner name: %s", name);
                } else {
                    ESP_LOGI(TAG, "Owner name not set");
                }
            } else if (strcmp(key, "serial_number") == 0) {
                char serial[64];
                esp_err_t ret = device_settings_get_serial(serial, sizeof(serial));
                if (ret == ESP_OK && serial[0] != '\0') {
                    ESP_LOGI(TAG, "Serial number: %s", serial);
                } else {
                    ESP_LOGI(TAG, "Serial number not set");
                }
            } else {
                ESP_LOGW(TAG, "Unknown key: %s", key);
                ESP_LOGW(TAG, "Usage: nvs get <owner_name|serial_number>");
            }
            return;
        }

        ESP_LOGW(TAG, "Unknown nvs subcommand: %s", subcmd);
        ESP_LOGW(TAG, "Usage: nvs set <owner_name|serial_number> <value> | nvs erase <owner_name|serial_number> | nvs get <owner_name|serial_number>");
        return;
    }

    ESP_LOGW(TAG, "Unknown command: %s", cmd);
    ESP_LOGW(TAG, "Supported: wifi-status | wifi-clear | wifi-provision <ssid> <password> | version | reboot | nvs set/erase/get <owner_name|serial_number>");
}

static void serial_task(void *arg) {
    (void)arg;

    char line[CMD_BUF_LEN];
    size_t pos = 0;

    while (true) {
        uint8_t ch;

    #if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
        int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100));
    #else
        int n = uart_read_bytes(CMD_UART, &ch, 1, pdMS_TO_TICKS(100));
#endif
        if (n <= 0) {
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            if (pos > 0) {
                line[pos] = '\0';
                handle_line(line);
                pos = 0;
            }
            continue;
        }

        if (pos + 1 < sizeof(line)) {
            line[pos++] = (char)ch;
        } else {
            // overflow; reset line
            pos = 0;
        }
    }
}

esp_err_t serial_commands_init(void) {
    if (s_task) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    // Ensure the USB Serial/JTAG driver is installed so we can read bytes.
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        ret = usb_serial_jtag_driver_install(&cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "usb_serial_jtag_driver_install failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }
#else
    // Ensure UART driver is installed so we can read bytes.
    ret = uart_driver_install(CMD_UART, 2048, 0, 0, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    BaseType_t ok = xTaskCreate(serial_task, "serial_cmd", 4096, NULL, 5, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create serial command task");
        s_task = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Serial commands ready (wifi-status, wifi-clear, wifi-provision, version, reboot, nvs)");
    return ESP_OK;
}
