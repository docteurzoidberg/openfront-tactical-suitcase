/**
 * @file can_handler.c
 * @brief CAN Handler Module Implementation
 * 
 * Handles CAN bus communication using can_bus_manager and can_protocol_keypad.
 */

#include "can_handler.h"
#include "can_bus_manager.h"
#include "can_protocol_keypad.h"
#include "can_protocol_discovery.h"
#include "led_controller.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "CAN";

// Private context
typedef struct {
    bool initialized;
    bool running;
} can_handler_ctx_t;

static can_handler_ctx_t s_ctx = {0};

/**
 * @brief Get current time in milliseconds
 */
static inline uint32_t get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/**
 * @brief RX handler for LED_SET messages (CAN ID 0x430)
 */
static void on_led_set_message(const can_frame_t *frame, void *ctx) {
    uint8_t key_id, state, r, g, b;
    
    if (!can_keypad_parse_led_set(frame, &key_id, &state, &r, &g, &b)) {
        ESP_LOGW(TAG, "Failed to parse LED_SET message");
        return;
    }
    
    ESP_LOGD(TAG, "LED_SET: K%d %s (R=%d G=%d B=%d)", 
             key_id, state ? "ON" : "OFF", r, g, b);
    
    led_rgb_t color = {r, g, b};
    
    // Set LED(s) and update immediately
    if (key_id == 0xFF) {
        // All LEDs
        led_controller_set_all(state != 0, color);
    } else {
        // Single LED
        led_controller_set_key(key_id, state != 0, color);
    }
    
    led_controller_update();
}

/**
 * @brief RX handler for LED_BULK messages (CAN ID 0x440)
 */
static void on_led_bulk_message(const can_frame_t *frame, void *ctx) {
    uint16_t mask;
    uint8_t state, r, g, b;
    
    if (!can_keypad_parse_led_bulk(frame, &mask, &r, &g, &b, &state)) {
        ESP_LOGW(TAG, "Failed to parse LED_BULK message");
        return;
    }
    
    ESP_LOGD(TAG, "LED_BULK: mask=0x%04X %s (R=%d G=%d B=%d)", 
             mask, state ? "ON" : "OFF", r, g, b);
    
    led_rgb_t color = {r, g, b};
    led_controller_set_bulk(mask, state != 0, color);
    led_controller_update();
}

/**
 * @brief RX handler for MODULE_QUERY messages (CAN ID 0x7FE)
 */
static void on_module_query(const can_frame_t *frame, void *ctx) {
   ESP_LOGD(TAG, "Received MODULE_QUERY, sending announce");
    
    // Build and send MODULE_ANNOUNCE
    can_frame_t announce;
    can_discovery_build_announce(
        &announce,              // out_frame
        CAN_MODULE_TYPE_KEYPAD, // module_type
        0,                      // version_major
        1,                      // version_minor
        0x0F,                   // capabilities: 15 keys + 15 LEDs
        0x43,                   // can_block_base (0x430)
        1                       // node_id
    );
    
    esp_err_t ret = can_bus_manager_send(&announce);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send MODULE_ANNOUNCE: %s", esp_err_to_name(ret));
    }
}

// Public API implementation

esp_err_t can_handler_init(void) {
    if (s_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    // Initialize can_bus_manager
    can_config_t can_config = {
        .tx_gpio = GPIO_CAN_TX,
        .rx_gpio = GPIO_CAN_RX,
        .bitrate = CAN_BITRATE,
        .loopback = false,
        .mock_mode = false
    };
    
    esp_err_t ret = can_bus_manager_init(&can_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "can_bus_manager_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register RX handlers for LED commands
    ret = can_bus_manager_register_handler(CAN_ID_KEYPAD_LED_SET, 0x7FF, 
                                           on_led_set_message, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register LED_SET handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = can_bus_manager_register_handler(CAN_ID_KEYPAD_LED_BULK, 0x7FF,
                                           on_led_bulk_message, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register LED_BULK handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register handler for MODULE_QUERY
    ret = can_bus_manager_register_handler(CAN_ID_MODULE_QUERY, 0x7FF,
                                           on_module_query, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MODULE_QUERY handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_ctx.initialized = true;
    ESP_LOGI(TAG, "Initialized (TX=%d RX=%d, %d kbit/s)", 
             GPIO_CAN_TX, GPIO_CAN_RX, CAN_BITRATE / 1000);
    return ESP_OK;
}

esp_err_t can_handler_start(void) {
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_ctx.running) {
        return ESP_OK;
    }
    
    // Send initial MODULE_ANNOUNCE
    can_frame_t announce;
    can_discovery_build_announce(
        &announce,              // out_frame
        CAN_MODULE_TYPE_KEYPAD, // module_type
        0,                      // version_major
        1,                      // version_minor
        0x0F,                   // capabilities: 15 keys + 15 LEDs
        0x43,                   // can_block_base (0x430)
        1                       // node_id
    );
    
    esp_err_t ret = can_bus_manager_send(&announce);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send initial MODULE_ANNOUNCE: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Sent MODULE_ANNOUNCE");
    }
    
    s_ctx.running = true;
    ESP_LOGI(TAG, "Started");
    return ESP_OK;
}

esp_err_t can_handler_stop(void) {
    s_ctx.running = false;
    return ESP_OK;
}

esp_err_t can_handler_send_key_event(uint8_t key_id, key_state_t state) {
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build CAN frame using can_protocol_keypad
    can_frame_t frame;
    uint16_t timestamp = (uint16_t)(get_time_ms() & 0xFFFF);
    
    can_keypad_build_key_event(key_id, state, timestamp, &frame);
    
    // Send via can_bus_manager
    esp_err_t ret = can_bus_manager_send(&frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send key event: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Sent KEY_EVENT: K%d %s", 
             key_id, state == KEY_STATE_PRESSED ? "PRESSED" : "RELEASED");
    
    return ESP_OK;
}
