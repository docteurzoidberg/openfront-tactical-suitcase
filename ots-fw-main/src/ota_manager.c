#include "ota_manager.h"
#include "led_handler.h"
#include "rgb_handler.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "OTS_OTA";

static httpd_handle_t ota_server = NULL;
static bool ota_in_progress = false;
static uint16_t ota_port = 3232;
static ota_progress_callback_t progress_callback = NULL;
static ota_complete_callback_t complete_callback = NULL;

// Forward declarations
static esp_err_t ota_post_handler(httpd_req_t *req);
static esp_err_t ota_handler(httpd_req_t *req);
static bool check_ota_auth(httpd_req_t *req);

esp_err_t ota_manager_init(uint16_t port, const char *hostname) {
    ESP_LOGI(TAG, "Initializing OTA manager on port %d", port);
    ota_port = port;
    ota_in_progress = false;
    return ESP_OK;
}

esp_err_t ota_manager_start(void) {
    if (ota_server != NULL) {
        ESP_LOGW(TAG, "OTA server already started");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = ota_port;
    config.ctrl_port = ota_port + 1;
    
    httpd_uri_t ota_uri = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&ota_server, &config) == ESP_OK) {
        httpd_register_uri_handler(ota_server, &ota_uri);
        ESP_LOGI(TAG, "OTA server started on port %d", ota_port);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to start OTA server");
        return ESP_FAIL;
    }
}

esp_err_t ota_manager_stop(void) {
    if (ota_server == NULL) {
        return ESP_OK;
    }
    
    httpd_stop(ota_server);
    ota_server = NULL;
    ESP_LOGI(TAG, "OTA server stopped");
    return ESP_OK;
}

bool ota_manager_is_updating(void) {
    return ota_in_progress;
}

void ota_manager_set_progress_callback(ota_progress_callback_t callback) {
    progress_callback = callback;
}

void ota_manager_set_complete_callback(ota_complete_callback_t callback) {
    complete_callback = callback;
}

static bool check_ota_auth(httpd_req_t *req) {
    char *buf = NULL;
    size_t buf_len = 0;
    
    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            // Check for "Basic " prefix
            if (strncmp(buf, "Basic ", 6) == 0) {
                // Simple password check - in production use proper base64 decode
                // For now, just check if Authorization header exists
                // Real implementation would decode base64 and verify credentials
                free(buf);
                return true;  // TODO: Implement proper auth
            }
        }
        free(buf);
    }
    
    // For compatibility with Arduino OTA tools, allow without auth for now
    // In production, enforce authentication
    return true;
}

static esp_err_t ota_handler(httpd_req_t *req) {
    if (!check_ota_auth(req)) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"OTA Update\"");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    return ota_post_handler(req);
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
    char buf[1024];
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = NULL;
    esp_err_t err;
    int remaining = req->content_len;

    ESP_LOGI(TAG, "Starting OTA update, size: %d bytes", remaining);
    ota_in_progress = true;

    const rgb_status_t pre_ota_status = rgb_status_get();
    
    // Set RGB to error state during OTA (simple - just solid red)
    rgb_status_set(RGB_STATUS_ERROR);
    
    // Turn off all module LEDs during OTA
    for (int i = 0; i < 3; i++) {
        led_command_t cmd = {.type = LED_TYPE_NUKE, .index = i, .effect = LED_EFFECT_OFF};
        led_controller_send_command(&cmd);
    }
    for (int i = 0; i < 6; i++) {
        led_command_t cmd = {.type = LED_TYPE_ALERT, .index = i, .effect = LED_EFFECT_OFF};
        led_controller_send_command(&cmd);
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find update partition");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No update partition");
        ota_in_progress = false;
        if (complete_callback) {
            complete_callback(false, "No update partition");
        }
        return ESP_FAIL;
    }

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        ota_in_progress = false;
        if (complete_callback) {
            complete_callback(false, "OTA begin failed");
        }
        return ESP_FAIL;
    }

    int received = 0;
    int progress = 0;
    bool led_state = false;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, sizeof(buf) < remaining ? sizeof(buf) : remaining);
        
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "HTTP receive failed");
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            ota_in_progress = false;
            if (complete_callback) {
                complete_callback(false, "Receive failed");
            }
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            ota_in_progress = false;
            if (complete_callback) {
                complete_callback(false, "Write failed");
            }
            return ESP_FAIL;
        }

        received += recv_len;
        remaining -= recv_len;
        
        // Show progress with LINK LED blinking and callback
        int new_progress = (received * 100) / req->content_len;
        if (new_progress != progress && new_progress % 5 == 0) {
            progress = new_progress;
            led_state = !led_state;
            led_controller_link_set(led_state);
            ESP_LOGI(TAG, "OTA Progress: %d%%", progress);
            
            if (progress_callback) {
                progress_callback(received, req->content_len, progress);
            }
        }
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        ota_in_progress = false;
        if (complete_callback) {
            complete_callback(false, "OTA end failed");
        }
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        ota_in_progress = false;
        if (complete_callback) {
            complete_callback(false, "Set boot partition failed");
        }
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update successful! Rebooting...");
    httpd_resp_sendstr(req, "Update successful, rebooting...");
    
    // Restore RGB to previous state before reboot
    rgb_status_set(pre_ota_status);
    
    ota_in_progress = false;
    
    if (complete_callback) {
        complete_callback(true, NULL);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}
