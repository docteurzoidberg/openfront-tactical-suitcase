#include "improv_serial.h"
#include "wifi_credentials.h"
#include "config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "OTS_IMPROV";

// Improv Serial protocol constants
#define IMPROV_SERIAL_VERSION 1
#define IMPROV_UART_NUM UART_NUM_0
#define IMPROV_UART_BUF_SIZE (1024)
#define IMPROV_TASK_STACK_SIZE (4096)
#define IMPROV_TASK_PRIORITY (5)

// Protocol frame markers
#define IMPROV_FRAME_START 0x49  // 'I'
#define IMPROV_FRAME_END 0x4D    // 'M'

// RPC Commands
#define IMPROV_CMD_WIFI_SETTINGS 0x01
#define IMPROV_CMD_IDENTIFY 0x02
#define IMPROV_CMD_GET_CURRENT_STATE 0x03
#define IMPROV_CMD_GET_DEVICE_INFO 0x04
#define IMPROV_CMD_GET_WIFI_NETWORKS 0x05

// Response types
#define IMPROV_RESP_CURRENT_STATE 0x01
#define IMPROV_RESP_ERROR_STATE 0x02
#define IMPROV_RESP_RPC_RESULT 0x03

// State
static improv_state_t current_state = IMPROV_STATE_READY;
static TaskHandle_t improv_task_handle = NULL;
static improv_provision_callback_t provision_callback = NULL;
static bool improv_running = false;

// Forward declarations
static void improv_serial_task(void *pvParameters);
static void handle_frame(const uint8_t *data, size_t len);
static void send_response(uint8_t type, const uint8_t *data, size_t len);
static void send_state(improv_state_t state);
static void handle_identify(void);
static void handle_get_device_info(void);
static void handle_wifi_settings(const uint8_t *data, size_t len);

esp_err_t improv_serial_init(void) {
    ESP_LOGI(TAG, "Initializing Improv Serial on UART0...");
    
    // UART is already initialized by console, we just listen to it
    // Check if provisioned
    if (wifi_credentials_exist()) {
        current_state = IMPROV_STATE_PROVISIONED;
        ESP_LOGI(TAG, "Device already provisioned");
    } else {
        current_state = IMPROV_STATE_READY;
        ESP_LOGI(TAG, "Device ready for provisioning");
    }
    
    return ESP_OK;
}

esp_err_t improv_serial_start(void) {
    if (improv_running) {
        ESP_LOGW(TAG, "Improv Serial already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting Improv Serial task...");
    
    BaseType_t ret = xTaskCreate(
        improv_serial_task,
        "improv_serial",
        IMPROV_TASK_STACK_SIZE,
        NULL,
        IMPROV_TASK_PRIORITY,
        &improv_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Improv Serial task");
        return ESP_FAIL;
    }
    
    improv_running = true;
    ESP_LOGI(TAG, "Improv Serial task started");
    
    // Send initial state
    send_state(current_state);
    
    return ESP_OK;
}

void improv_serial_stop(void) {
    if (improv_task_handle) {
        improv_running = false;
        vTaskDelete(improv_task_handle);
        improv_task_handle = NULL;
        ESP_LOGI(TAG, "Improv Serial task stopped");
    }
}

void improv_serial_set_callback(improv_provision_callback_t callback) {
    provision_callback = callback;
}

void improv_serial_set_state(improv_state_t state) {
    if (current_state != state) {
        current_state = state;
        send_state(state);
        ESP_LOGI(TAG, "State changed to: %d", state);
    }
}

void improv_serial_send_error(improv_error_t error) {
    uint8_t data[] = {error};
    send_response(IMPROV_RESP_ERROR_STATE, data, sizeof(data));
    ESP_LOGW(TAG, "Sent error: %d", error);
}

bool improv_serial_is_provisioned(void) {
    return wifi_credentials_exist();
}

esp_err_t improv_serial_clear_credentials(void) {
    esp_err_t ret = wifi_credentials_clear();
    if (ret == ESP_OK) {
        current_state = IMPROV_STATE_READY;
        send_state(current_state);
        ESP_LOGI(TAG, "Credentials cleared, factory reset complete");
    }
    return ret;
}

static void improv_serial_task(void *pvParameters) {
    uint8_t buffer[256];
    int len;
    
    ESP_LOGI(TAG, "Improv Serial listening on UART0...");
    ESP_LOGI(TAG, "Connect with Improv WiFi tools or web interface");
    ESP_LOGI(TAG, "Visit: https://www.improv-wifi.com/");
    
    while (improv_running) {
        // Read from UART
        len = uart_read_bytes(IMPROV_UART_NUM, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
        
        if (len > 0) {
            // Look for Improv frame start
            for (int i = 0; i < len; i++) {
                if (buffer[i] == IMPROV_FRAME_START && i + 6 < len) {
                    // Found potential frame start
                    uint8_t version = buffer[i + 1];
                    uint8_t type = buffer[i + 2];
                    uint8_t data_len = buffer[i + 3];
                    
                    // Validate frame
                    if (version == IMPROV_SERIAL_VERSION && 
                        i + 6 + data_len < len &&
                        buffer[i + 5 + data_len] == IMPROV_FRAME_END) {
                        
                        // Valid frame found
                        handle_frame(&buffer[i], 6 + data_len);
                        i += 6 + data_len;  // Skip past frame
                    }
                }
            }
        }
        
        // Small delay
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}

static void handle_frame(const uint8_t *data, size_t len) {
    if (len < 6) return;
    
    uint8_t version = data[1];
    uint8_t type = data[2];
    uint8_t data_len = data[3];
    uint8_t checksum = data[4];
    const uint8_t *payload = &data[5];
    
    // Verify checksum (sum of data bytes)
    uint8_t calc_checksum = 0;
    for (int i = 0; i < data_len; i++) {
        calc_checksum += payload[i];
    }
    
    if (calc_checksum != checksum) {
        ESP_LOGW(TAG, "Checksum mismatch: expected %d, got %d", calc_checksum, checksum);
        improv_serial_send_error(IMPROV_ERROR_INVALID_RPC);
        return;
    }
    
    ESP_LOGI(TAG, "Received command: type=%d, len=%d", type, data_len);
    
    switch (type) {
        case IMPROV_CMD_WIFI_SETTINGS:
            handle_wifi_settings(payload, data_len);
            break;
            
        case IMPROV_CMD_IDENTIFY:
            handle_identify();
            break;
            
        case IMPROV_CMD_GET_CURRENT_STATE:
            send_state(current_state);
            break;
            
        case IMPROV_CMD_GET_DEVICE_INFO:
            handle_get_device_info();
            break;
            
        case IMPROV_CMD_GET_WIFI_NETWORKS:
            // Not implemented yet
            improv_serial_send_error(IMPROV_ERROR_UNKNOWN_RPC);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown command: %d", type);
            improv_serial_send_error(IMPROV_ERROR_UNKNOWN_RPC);
            break;
    }
}

static void send_response(uint8_t type, const uint8_t *data, size_t len) {
    if (len > 255) {
        ESP_LOGE(TAG, "Response data too large: %d", len);
        return;
    }
    
    // Calculate checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    
    // Build frame: START | VERSION | TYPE | LEN | CHECKSUM | DATA... | END
    uint8_t frame[258];
    frame[0] = IMPROV_FRAME_START;
    frame[1] = IMPROV_SERIAL_VERSION;
    frame[2] = type;
    frame[3] = (uint8_t)len;
    frame[4] = checksum;
    memcpy(&frame[5], data, len);
    frame[5 + len] = IMPROV_FRAME_END;
    
    // Send to UART
    uart_write_bytes(IMPROV_UART_NUM, frame, 6 + len);
    ESP_LOGD(TAG, "Sent response: type=%d, len=%d", type, len);
}

static void send_state(improv_state_t state) {
    uint8_t data[] = {state};
    send_response(IMPROV_RESP_CURRENT_STATE, data, sizeof(data));
}

static void handle_identify(void) {
    ESP_LOGI(TAG, "Identify request received");
    
    // Blink LED or do something to identify the device
    // For now, just send OK response
    uint8_t data[] = {0};  // Empty OK
    send_response(IMPROV_RESP_RPC_RESULT, data, sizeof(data));
}

static void handle_get_device_info(void) {
    ESP_LOGI(TAG, "Device info request received");
    
    // Build device info response
    // Format: <firmware_name>\0<version>\0<chip>\0<device_name>\0
    char info[128];
    int offset = 0;
    
    // Firmware name
    offset += snprintf(info + offset, sizeof(info) - offset, "%s", OTS_FIRMWARE_NAME) + 1;
    
    // Version
    offset += snprintf(info + offset, sizeof(info) - offset, "%s", OTS_FIRMWARE_VERSION) + 1;
    
    // Chip type
    offset += snprintf(info + offset, sizeof(info) - offset, "ESP32-S3") + 1;
    
    // Device name
    offset += snprintf(info + offset, sizeof(info) - offset, "%s", MDNS_HOSTNAME) + 1;
    
    send_response(IMPROV_RESP_RPC_RESULT, (uint8_t *)info, offset);
}

static void handle_wifi_settings(const uint8_t *data, size_t len) {
    ESP_LOGI(TAG, "WiFi settings command received");
    
    // Parse WiFi settings: <ssid_len><ssid><pass_len><pass>
    if (len < 2) {
        improv_serial_send_error(IMPROV_ERROR_INVALID_RPC);
        return;
    }
    
    uint8_t ssid_len = data[0];
    if (ssid_len == 0 || ssid_len >= WIFI_CREDENTIALS_MAX_SSID_LEN || ssid_len + 2 > len) {
        ESP_LOGE(TAG, "Invalid SSID length: %d", ssid_len);
        improv_serial_send_error(IMPROV_ERROR_INVALID_RPC);
        return;
    }
    
    const uint8_t *ssid = &data[1];
    uint8_t pass_len = data[1 + ssid_len];
    
    if (pass_len >= WIFI_CREDENTIALS_MAX_PASSWORD_LEN || ssid_len + pass_len + 2 > len) {
        ESP_LOGE(TAG, "Invalid password length: %d", pass_len);
        improv_serial_send_error(IMPROV_ERROR_INVALID_RPC);
        return;
    }
    
    const uint8_t *password = &data[2 + ssid_len];
    
    // Create credentials
    wifi_credentials_t creds = {0};
    memcpy(creds.ssid, ssid, ssid_len);
    memcpy(creds.password, password, pass_len);
    creds.ssid[ssid_len] = '\0';
    creds.password[pass_len] = '\0';
    
    ESP_LOGI(TAG, "Provisioning WiFi: SSID='%s'", creds.ssid);
    
    // Save to NVS
    esp_err_t ret = wifi_credentials_save(&creds);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save credentials: %s", esp_err_to_name(ret));
        improv_serial_send_error(IMPROV_ERROR_UNKNOWN);
        return;
    }
    
    // Update state
    current_state = IMPROV_STATE_PROVISIONING;
    send_state(current_state);
    
    // Notify callback
    if (provision_callback) {
        provision_callback(true, creds.ssid);
    }
    
    // Send success response with redirect URL
    char url[64];
    snprintf(url, sizeof(url), "http://%s.local/", MDNS_HOSTNAME);
    send_response(IMPROV_RESP_RPC_RESULT, (uint8_t *)url, strlen(url));
    
    // Update state to provisioned
    current_state = IMPROV_STATE_PROVISIONED;
    send_state(current_state);
    
    ESP_LOGI(TAG, "Provisioning complete! Device will reconnect with new credentials.");
    ESP_LOGI(TAG, "You may need to restart the device for changes to take effect.");
}
