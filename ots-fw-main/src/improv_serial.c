#include "improv_serial.h"
#include "wifi_credentials.h"
#include "config.h"
#include "driver/uart.h"
#if CONFIG_USJ_ENABLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "OTS_IMPROV";

// Improv Serial protocol constants (https://www.improv-wifi.com/serial/)
#define IMPROV_SERIAL_VERSION 1
#define IMPROV_UART_NUM UART_NUM_0
#define IMPROV_UART_BUF_SIZE (1024)
#define IMPROV_TASK_STACK_SIZE (4096)
#define IMPROV_TASK_PRIORITY (5)

#define IMPROV_HEADER_LEN 6
static const uint8_t IMPROV_HEADER[IMPROV_HEADER_LEN] = {'I', 'M', 'P', 'R', 'O', 'V'};

// Packet types
#define IMPROV_MSG_CURRENT_STATE 0x01
#define IMPROV_MSG_ERROR_STATE   0x02
#define IMPROV_MSG_RPC           0x03
#define IMPROV_MSG_RPC_RESULT    0x04

// RPC command IDs
#define IMPROV_RPC_SEND_WIFI_SETTINGS     0x01
#define IMPROV_RPC_REQUEST_CURRENT_STATE  0x02
#define IMPROV_RPC_REQUEST_INFO           0x03
#define IMPROV_RPC_REQUEST_WIFI_NETWORKS  0x04
#define IMPROV_RPC_GET_SET_HOSTNAME       0x05

// State
static improv_state_t current_state = IMPROV_STATE_READY;
static TaskHandle_t improv_task_handle = NULL;
static improv_provision_callback_t provision_callback = NULL;
static bool improv_running = false;

// Forward declarations
static void improv_serial_task(void *pvParameters);
static void handle_packet(const uint8_t *packet, size_t packet_len);
static void send_packet(uint8_t type, const uint8_t *data, size_t len);
static void send_current_state(improv_state_t state);
static void send_error_state(improv_error_t error);
static void handle_rpc_command(const uint8_t *data, size_t len);
static void rpc_send_wifi_settings(const uint8_t *data, size_t len);
static void rpc_send_info(void);
static void rpc_send_hostname(void);

typedef struct {
    uint8_t buf[512];
    size_t len;
} improv_rx_stream_t;

static void process_rx_bytes(improv_rx_stream_t *stream, const uint8_t *data, size_t data_len);

esp_err_t improv_serial_init(void) {
    ESP_LOGI(TAG, "Initializing Improv Serial...");

#if CONFIG_USJ_ENABLE_USB_SERIAL_JTAG
    // Ensure USB-Serial/JTAG driver is available for WebSerial (ttyACM*) provisioning.
    // If it's already installed by console setup, ESP_ERR_INVALID_STATE is expected.
    usb_serial_jtag_driver_config_t usj_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024,
    };
    esp_err_t usj_ret = usb_serial_jtag_driver_install(&usj_cfg);
    if (usj_ret != ESP_OK && usj_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "USB-Serial/JTAG driver install failed: %s", esp_err_to_name(usj_ret));
    }
#endif
    
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

    // IMPORTANT: set the running flag BEFORE starting the task.
    // Otherwise the task can get scheduled immediately after xTaskCreate()
    // and exit because improv_running is still false.
    improv_running = true;
    
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
        improv_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Improv Serial task started");
    
    // Send initial state
    send_current_state(current_state);
    
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
        send_current_state(state);
        ESP_LOGI(TAG, "State changed to: %d", state);
    }
}

void improv_serial_send_error(improv_error_t error) {
    send_error_state(error);
    ESP_LOGW(TAG, "Sent error: %d", error);
}

bool improv_serial_is_provisioned(void) {
    return wifi_credentials_exist();
}

esp_err_t improv_serial_clear_credentials(void) {
    esp_err_t ret = wifi_credentials_clear();
    if (ret == ESP_OK) {
        current_state = IMPROV_STATE_READY;
        send_current_state(current_state);
        ESP_LOGI(TAG, "Credentials cleared, factory reset complete");
    }
    return ret;
}

static void improv_serial_task(void *pvParameters) {
    uint8_t buffer[256];
    int len;

    uint32_t last_rx_log_ticks = 0;
    uint32_t last_state_broadcast_ticks = 0;

    improv_rx_stream_t uart_stream = {0};
#if CONFIG_USJ_ENABLE_USB_SERIAL_JTAG
    improv_rx_stream_t usj_stream = {0};
#endif
    
    ESP_LOGI(TAG, "Improv Serial listening on UART0...");
#if CONFIG_USJ_ENABLE_USB_SERIAL_JTAG
    ESP_LOGI(TAG, "Improv Serial also listening on USB-Serial/JTAG (ttyACM*)...");
#endif
    ESP_LOGI(TAG, "Improv Serial protocol: https://www.improv-wifi.com/serial/");
    
    while (improv_running) {
        // Read from UART0 (works when host is wired to UART0)
        len = uart_read_bytes(IMPROV_UART_NUM, buffer, sizeof(buffer), pdMS_TO_TICKS(20));
        if (len > 0) {
            // Debug: confirm RX path (rate-limited to avoid log spam)
            const uint32_t now = xTaskGetTickCount();
            if ((now - last_rx_log_ticks) > pdMS_TO_TICKS(500)) {
                ESP_LOGI(TAG, "RX UART0: %d bytes (first=%02X %02X %02X)", len,
                         (unsigned)buffer[0], (unsigned)(len > 1 ? buffer[1] : 0), (unsigned)(len > 2 ? buffer[2] : 0));
                last_rx_log_ticks = now;
            }
            process_rx_bytes(&uart_stream, buffer, (size_t)len);
        }

#if CONFIG_USJ_ENABLE_USB_SERIAL_JTAG
        // Read from USB-Serial/JTAG (works with WebSerial / improv-wifi.com)
        len = usb_serial_jtag_read_bytes(buffer, sizeof(buffer), pdMS_TO_TICKS(20));
        if (len > 0) {
            // Keep WebSerial stream clean: avoid logging RX bytes here.
            process_rx_bytes(&usj_stream, buffer, (size_t)len);
        }
#endif

        // Help WebSerial clients detect the device even if they missed the initial packet.
        // Broadcast Current State periodically while not provisioned.
        const uint32_t now = xTaskGetTickCount();
        if (current_state != IMPROV_STATE_PROVISIONED &&
            (now - last_state_broadcast_ticks) > pdMS_TO_TICKS(2000)) {
            send_current_state(current_state);
            last_state_broadcast_ticks = now;
        }
        
        // Small delay
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}

static void handle_frame(const uint8_t *data, size_t len) {
    (void)data;
    (void)len;
}

static uint8_t improv_checksum(const uint8_t *bytes, size_t len_without_checksum) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len_without_checksum; i++) {
        sum += bytes[i];
    }
    return (uint8_t)(sum & 0xFF);
}

static void send_packet(uint8_t type, const uint8_t *data, size_t len) {
    // Spec format (https://www.improv-wifi.com/serial/):
    // IMPROV (6) | VERSION (1) | TYPE (1) | LEN (1) | DATA... | CHECKSUM (1)
    // Note: The official JS SDK also tolerates a trailing '\n' to help ignore log lines.
    // Keep payload sizes small; we cap to the RX stream buffer.
    if (len > 255) {
        ESP_LOGE(TAG, "Improv packet too large for 1-byte LEN: %d", (int)len);
        return;
    }
    if (len > 512) {
        ESP_LOGE(TAG, "Improv packet too large: %d", (int)len);
        return;
    }

    uint8_t pkt[IMPROV_HEADER_LEN + 1 + 1 + 1 + 512 + 1 + 1];
    size_t idx = 0;
    memcpy(&pkt[idx], IMPROV_HEADER, IMPROV_HEADER_LEN);
    idx += IMPROV_HEADER_LEN;
    pkt[idx++] = IMPROV_SERIAL_VERSION;
    pkt[idx++] = type;
    const uint8_t payload_len = (uint8_t)len;
    pkt[idx++] = payload_len;
    if (len > 0 && data) {
        memcpy(&pkt[idx], data, len);
        idx += len;
    }

    // checksum is the sum of all bytes prior to the checksum byte
    uint8_t checksum = improv_checksum(pkt, idx);
    pkt[idx++] = checksum;
    pkt[idx++] = 10; // '\n' - helps the JS SDK discard log lines cleanly

    // Send to UART0
    uart_write_bytes(IMPROV_UART_NUM, (const char *)pkt, idx);

#if CONFIG_USJ_ENABLE_USB_SERIAL_JTAG
    // Also send to USB-Serial/JTAG (WebSerial)
    usb_serial_jtag_write_bytes(pkt, idx, pdMS_TO_TICKS(100));
#endif
}

static void process_rx_bytes(improv_rx_stream_t *stream, const uint8_t *data, size_t data_len) {
    if (data_len == 0) {
        return;
    }

    // If overflow, drop buffered data (simplest recovery)
    if (stream->len + data_len > sizeof(stream->buf)) {
        stream->len = 0;
    }

    memcpy(&stream->buf[stream->len], data, data_len);
    stream->len += data_len;

    // Parse as many packets as possible
    while (stream->len >= (IMPROV_HEADER_LEN + 1 + 1 + 1 + 1)) {
        // Find header "IMPROV"
        size_t start = 0;
        while (start + IMPROV_HEADER_LEN <= stream->len &&
               memcmp(&stream->buf[start], IMPROV_HEADER, IMPROV_HEADER_LEN) != 0) {
            start++;
        }

        if (start > 0) {
            memmove(stream->buf, &stream->buf[start], stream->len - start);
            stream->len -= start;
        }

        if (stream->len < (IMPROV_HEADER_LEN + 1 + 1 + 1 + 1)) {
            return;
        }

        // Layout: IMPROV (6) | VER (1) | TYPE (1) | LEN (1) | DATA... | CHECKSUM (1)
        const uint8_t version = stream->buf[IMPROV_HEADER_LEN + 0];
        const uint8_t payload_len = stream->buf[IMPROV_HEADER_LEN + 2];
        const size_t packet_len = IMPROV_HEADER_LEN + 1 + 1 + 1 + (size_t)payload_len + 1;

        if (packet_len > sizeof(stream->buf)) {
            // Invalid length; drop one byte and resync
            memmove(stream->buf, &stream->buf[1], stream->len - 1);
            stream->len -= 1;
            continue;
        }
        if (stream->len < packet_len) {
            return;
        }

        if (version == IMPROV_SERIAL_VERSION) {
            handle_packet(stream->buf, packet_len);
        }

        // Consume packet
        size_t consume = packet_len;
        // Optional newline terminator (often present)
        if (stream->len > consume && stream->buf[consume] == 10) {
            consume++;
        }
        if (stream->len > consume) {
            memmove(stream->buf, &stream->buf[consume], stream->len - consume);
        }
        stream->len -= consume;
    }
}

static void send_current_state(improv_state_t state) {
    uint8_t data[] = {(uint8_t)state};
    send_packet(IMPROV_MSG_CURRENT_STATE, data, sizeof(data));
}

static void send_error_state(improv_error_t error) {
    uint8_t data[] = {(uint8_t)error};
    send_packet(IMPROV_MSG_ERROR_STATE, data, sizeof(data));
}

static bool hostname_is_valid(const uint8_t *name, size_t len) {
    if (!name || len == 0 || len > 255) {
        return false;
    }
    // RFC1123-ish: letters, digits, hyphen. Cannot start/end with hyphen.
    if (name[0] == '-' || name[len - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        const uint8_t c = name[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || (c == '-');
        if (!ok) {
            return false;
        }
    }
    return true;
}

static void send_rpc_result(uint8_t rpc_command, const char *const *strings, size_t string_count) {
    // Data: [command][totalLength][len][bytes]...
    uint8_t payload[256];
    size_t idx = 0;

    payload[idx++] = rpc_command;
    payload[idx++] = 0; // total length placeholder

    size_t total = 0;
    for (size_t i = 0; i < string_count; i++) {
        const char *s = strings[i] ? strings[i] : "";
        const size_t slen = strlen(s);
        if (slen > 255 || (idx + 1 + slen) > sizeof(payload)) {
            ESP_LOGW(TAG, "RPC result too large; truncating");
            break;
        }
        payload[idx++] = (uint8_t)slen;
        memcpy(&payload[idx], s, slen);
        idx += slen;
        total += 1 + slen;
    }
    payload[1] = (uint8_t)(total & 0xFF);

    send_packet(IMPROV_MSG_RPC_RESULT, payload, idx);
}

static void rpc_send_info(void) {
    const char *strings[] = {OTS_FIRMWARE_NAME, OTS_FIRMWARE_VERSION, "ESP32-S3", MDNS_HOSTNAME};
    send_rpc_result(IMPROV_RPC_REQUEST_INFO, strings, 4);
}

static void rpc_send_hostname(void) {
    const char *strings[] = {MDNS_HOSTNAME};
    send_rpc_result(IMPROV_RPC_GET_SET_HOSTNAME, strings, 1);
}

static void rpc_send_wifi_settings(const uint8_t *data, size_t len) {
    // Data layout: ssid_len | ssid_bytes | password_len | password_bytes
    if (!data || len < 2) {
        send_error_state(IMPROV_ERROR_INVALID_RPC);
        return;
    }

    const uint8_t ssid_len = data[0];
    if (ssid_len == 0 || ssid_len >= WIFI_CREDENTIALS_MAX_SSID_LEN || (size_t)ssid_len + 2 > len) {
        send_error_state(IMPROV_ERROR_INVALID_RPC);
        return;
    }
    const uint8_t *ssid = &data[1];
    const uint8_t pass_len = data[1 + ssid_len];
    if (pass_len >= WIFI_CREDENTIALS_MAX_PASSWORD_LEN || (size_t)ssid_len + (size_t)pass_len + 2 > len) {
        send_error_state(IMPROV_ERROR_INVALID_RPC);
        return;
    }
    const uint8_t *password = &data[2 + ssid_len];

    wifi_credentials_t creds = {0};
    memcpy(creds.ssid, ssid, ssid_len);
    memcpy(creds.password, password, pass_len);
    creds.ssid[ssid_len] = '\0';
    creds.password[pass_len] = '\0';

    ESP_LOGI(TAG, "Improv provisioning WiFi: SSID='%s'", creds.ssid);

    esp_err_t ret = wifi_credentials_save(&creds);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save credentials: %s", esp_err_to_name(ret));
        send_error_state(IMPROV_ERROR_UNKNOWN);
        return;
    }

    // State update for UI feedback
    current_state = IMPROV_STATE_PROVISIONING;
    send_current_state(current_state);

    // Reply with redirect URL (best-effort)
    char url[96];
    snprintf(url, sizeof(url), "http://%s.local/", MDNS_HOSTNAME);
    const char *strings[] = {url};
    send_rpc_result(IMPROV_RPC_SEND_WIFI_SETTINGS, strings, 1);

    // Notify main app (typically triggers reboot to apply creds)
    if (provision_callback) {
        provision_callback(true, creds.ssid);
    }
}

static void handle_rpc_command(const uint8_t *data, size_t len) {
    if (!data || len < 2) {
        send_error_state(IMPROV_ERROR_INVALID_RPC);
        return;
    }

    const uint8_t cmd = data[0];
    const uint8_t data_len = data[1];
    if ((size_t)data_len != (len - 2)) {
        send_error_state(IMPROV_ERROR_INVALID_RPC);
        return;
    }

    // Clear error state on any valid RPC packet
    send_error_state(IMPROV_ERROR_NONE);

    const uint8_t *rpc_data = &data[2];
    const size_t rpc_len = (size_t)data_len;

    switch (cmd) {
        case IMPROV_RPC_REQUEST_CURRENT_STATE: {
            send_current_state(current_state);
            // If already provisioned, also return a URL (mirrors provision result)
            if (current_state == IMPROV_STATE_PROVISIONED) {
                char url[96];
                snprintf(url, sizeof(url), "http://%s.local/", MDNS_HOSTNAME);
                const char *strings[] = {url};
                send_rpc_result(IMPROV_RPC_REQUEST_CURRENT_STATE, strings, 1);
            }
            break;
        }
        case IMPROV_RPC_REQUEST_INFO:
            rpc_send_info();
            break;
        case IMPROV_RPC_SEND_WIFI_SETTINGS:
            rpc_send_wifi_settings(rpc_data, rpc_len);
            break;
        case IMPROV_RPC_REQUEST_WIFI_NETWORKS:
            // Optional; not implemented in this test firmware.
            send_error_state(IMPROV_ERROR_UNKNOWN_RPC);
            break;
        case IMPROV_RPC_GET_SET_HOSTNAME: {
            // Get if no data; set if data present (we don't persist in this firmware).
            if (rpc_len == 0) {
                rpc_send_hostname();
                break;
            }
            if (!hostname_is_valid(rpc_data, rpc_len)) {
                send_error_state(IMPROV_ERROR_BAD_HOSTNAME);
                break;
            }
            // Acknowledge by returning the (unchanged) hostname.
            rpc_send_hostname();
            break;
        }
        default:
            send_error_state(IMPROV_ERROR_UNKNOWN_RPC);
            break;
    }
}

static void handle_packet(const uint8_t *packet, size_t packet_len) {
    // packet: IMPROV (6) | VER (1) | TYPE (1) | LEN (1) | DATA... | CHECKSUM (1)
    if (!packet || packet_len < (IMPROV_HEADER_LEN + 1 + 1 + 1 + 1)) {
        return;
    }
    if (memcmp(packet, IMPROV_HEADER, IMPROV_HEADER_LEN) != 0) {
        return;
    }

    const uint8_t version = packet[IMPROV_HEADER_LEN + 0];
    const uint8_t type = packet[IMPROV_HEADER_LEN + 1];
    const uint8_t len = packet[IMPROV_HEADER_LEN + 2];

    const size_t expected = IMPROV_HEADER_LEN + 1 + 1 + 1 + (size_t)len + 1;
    if (packet_len != expected) {
        return;
    }
    if (version != IMPROV_SERIAL_VERSION) {
        return;
    }

    const uint8_t *data = &packet[IMPROV_HEADER_LEN + 3];
    const uint8_t checksum = packet[packet_len - 1];
    const uint8_t calc = improv_checksum(packet, packet_len - 1);
    if (calc != checksum) {
        ESP_LOGW(TAG, "Improv checksum mismatch (got=%u expected=%u)", (unsigned)checksum, (unsigned)calc);
        send_error_state(IMPROV_ERROR_INVALID_RPC);
        return;
    }

    if (type == IMPROV_MSG_RPC) {
        handle_rpc_command(data, (size_t)len);
    } else {
        // We only process RPC commands from the client.
        // Other packet types are device-to-client.
    }
}
