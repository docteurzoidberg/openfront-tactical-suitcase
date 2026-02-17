/**
 * @file keypad_module.c
 * @brief Keypad Module Implementation
 * 
 * Handles communication with external 15-key keyboard module via CAN bus.
 * Receives key press/release events and sends LED commands.
 */

#include "keypad_module.h"
#include "can_bus_manager.h"
#include "can_protocol_keypad.h"
#include "ws_handlers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "OTS_KEYPAD";

static module_status_t status = {0};
static bool keypad_connected = false;

// CAN RX handlers

/**
 * @brief Handle KEY_EVENT messages from keypad (0x431-0x43F)
 */
static void on_keypad_key_event(const can_frame_t *frame, void *ctx) {
    if (frame->dlc < 4) {
        ESP_LOGW(TAG, "Invalid KEY_EVENT DLC: %d", frame->dlc);
        return;
    }
    
    // Parse KEY_EVENT message
    can_keypad_event_t event;
    memcpy(&event, frame->data, sizeof(event));
    
    // Validate key ID
    if (event.key_id < CAN_KEYPAD_KEY_MIN || event.key_id > CAN_KEYPAD_KEY_MAX) {
        ESP_LOGW(TAG, "Invalid key_id: %d", event.key_id);
        return;
    }
    
    // Log key event
    ESP_LOGI(TAG, "Key K%d %s (timestamp=%u)", 
             event.key_id, 
             event.state == CAN_KEYPAD_STATE_PRESSED ? "PRESSED" : "RELEASED",
             event.timestamp);
    
    // Create game event for WebSocket
    game_event_t game_event = {0};
    game_event.timestamp = esp_timer_get_time() / 1000;
    
    if (event.state == CAN_KEYPAD_STATE_PRESSED) {
        game_event.type = GAME_EVENT_KEYPAD_KEY_PRESSED;
        snprintf(game_event.message, sizeof(game_event.message), 
                 "Key K%d pressed", event.key_id);
    } else {
        game_event.type = GAME_EVENT_KEYPAD_KEY_RELEASED;
        snprintf(game_event.message, sizeof(game_event.message), 
                 "Key K%d released", event.key_id);
    }
    
    // Add key_id to data payload
    snprintf(game_event.data, sizeof(game_event.data), 
             "{\"keyId\":%d}", event.key_id);
    
    // Send to WebSocket
    ws_handlers_send_event(&game_event);
    
    // Post to event dispatcher for local handling
    internal_event_t internal_event = {
        .type = event.state == CAN_KEYPAD_STATE_PRESSED ? 
                GAME_EVENT_KEYPAD_KEY_PRESSED : GAME_EVENT_KEYPAD_KEY_RELEASED,
        .data = {event.key_id, 0, 0, 0}
    };
    event_dispatcher_post(&internal_event);
}

/**
 * @brief Helper to send LED_SET command to keypad
 */
static esp_err_t send_led_command(uint8_t key_id, bool on, uint8_t r, uint8_t g, uint8_t b) {
    can_frame_t frame;
    can_keypad_build_led_set(
        key_id,
        on ? CAN_KEYPAD_LED_ON : CAN_KEYPAD_LED_OFF,
        r,
        g,
        b,
        &frame
    );
    
    esp_err_t ret = can_bus_manager_send(&frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send LED_SET: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Sent LED_SET: K%d %s (R=%d G=%d B=%d)", 
             key_id, on ? "ON" : "OFF", r, g, b);
    
    return ESP_OK;
}

// Module interface implementations

static esp_err_t keypad_module_init(void) {
    ESP_LOGI(TAG, "Initializing keypad module...");
    
    // Register CAN handlers for all 15 key events (0x431-0x43F)
    for (uint8_t key_id = CAN_KEYPAD_KEY_MIN; key_id <= CAN_KEYPAD_KEY_MAX; key_id++) {
        uint32_t can_id = CAN_ID_KEYPAD_KEY_EVENT(key_id);
        esp_err_t ret = can_bus_manager_register_handler(can_id, 0x7FF, 
                                                         on_keypad_key_event, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register CAN handler for K%d: %s", 
                     key_id, esp_err_to_name(ret));
            status.error_count++;
            snprintf(status.last_error, sizeof(status.last_error), 
                     "CAN register failed");
            return ret;
        }
    }
    
    status.initialized = true;
    status.operational = true;
    status.error_count = 0;
    
    ESP_LOGI(TAG, "Keypad module initialized (15 keys, CAN-connected)");
    return ESP_OK;
}

static esp_err_t keypad_module_update(void) {
    // Nothing to do here - CAN events are async
    return ESP_OK;
}

static bool keypad_module_handle_event(const internal_event_t *event) {
    // Handle LED command events from dashboard/userscript
    // Example: INTERNAL_EVENT_KEYPAD_LED_SET
    // For now, only handle keypad connection status
    
    if (event->type == GAME_EVENT_KEYPAD_CONNECTED) {
        ESP_LOGI(TAG, "Keypad connected");
        keypad_connected = true;
        
        // Send connection event to WebSocket
        game_event_t game_event = {0};
        game_event.timestamp = esp_timer_get_time() / 1000;
        game_event.type = GAME_EVENT_KEYPAD_CONNECTED;
        strncpy(game_event.message, "Keypad connected", sizeof(game_event.message) - 1);
        ws_handlers_send_event(&game_event);
        
        return true;
    }
    
    if (event->type == GAME_EVENT_KEYPAD_DISCONNECTED) {
        ESP_LOGI(TAG, "Keypad disconnected");
        keypad_connected = false;
        
        // Send disconnection event to WebSocket
        game_event_t game_event = {0};
        game_event.timestamp = esp_timer_get_time() / 1000;
        game_event.type = GAME_EVENT_KEYPAD_DISCONNECTED;
        strncpy(game_event.message, "Keypad disconnected", sizeof(game_event.message) - 1);
        ws_handlers_send_event(&game_event);
        
        return true;
    }
    
    // Future: Handle LED command events from dashboard
    // if (event->type == INTERNAL_EVENT_KEYPAD_LED_COMMAND) { ... }
    
    return false;
}

static void keypad_module_get_status(module_status_t *out_status) {
    if (out_status) {
        memcpy(out_status, &status, sizeof(module_status_t));
    }
}

static esp_err_t keypad_module_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down keypad module...");
    
    // Turn off all LEDs
    for (uint8_t key_id = CAN_KEYPAD_KEY_MIN; key_id <= CAN_KEYPAD_KEY_MAX; key_id++) {
        send_led_command(key_id, false, 0, 0, 0);
    }
    
    status.operational = false;
    keypad_connected = false;
    return ESP_OK;
}

// Module definition
hardware_module_t keypad_module = {
    .name = "Keypad Module",
    .enabled = true,
    .init = keypad_module_init,
    .update = keypad_module_update,
    .handle_event = keypad_module_handle_event,
    .get_status = keypad_module_get_status,
    .shutdown = keypad_module_shutdown
};
