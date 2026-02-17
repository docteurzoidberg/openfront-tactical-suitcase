/**
 * @file led_controller.c
 * @brief LED Controller Module Implementation
 * 
 * Controls 15 SK6812-MINI-E RGB LEDs via RMT peripheral.
 */

#include "led_controller.h"
#include "config.h"
#include "ws2812_rmt.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "LED";

// Private data structures
typedef struct {
    bool on;              // LED on/off
    led_rgb_t color;      // RGB color (0-255 each)
} led_state_t;

typedef struct {
    led_state_t leds[NUM_LEDS];        // State for all 15 LEDs
    uint8_t brightness_global;         // Global brightness (0-255)
    bool initialized;                  // Init complete flag
} led_controller_ctx_t;

// Global context
static led_controller_ctx_t s_ctx = {0};

/**
 * @brief Configure WS2812 for SK6812 LEDs
 */
static esp_err_t configure_ws2812(void) {
    ws2812_config_t ws_config = {
        .gpio_num = GPIO_RGB_DATA,
        .led_count = NUM_LEDS,
        .resolution_hz = 10000000  // 10MHz
    };
    
    esp_err_t ret = ws2812_init(&ws_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WS2812: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WS2812 configured (GPIO %d, %d LEDs)", GPIO_RGB_DATA, NUM_LEDS);
    return ESP_OK;
}

/**
 * @brief Write LED state buffer to hardware
 */
static esp_err_t write_leds_to_hardware(void) {
    if (!s_ctx.initialized || !ws2812_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Set each LED according to state
    for (int i = 0; i < NUM_LEDS; i++) {
        ws2812_color_t color;
        if (s_ctx.leds[i].on) {
            // Apply global brightness
            color.r = (s_ctx.leds[i].color.r * s_ctx.brightness_global) / 255;
            color.g = (s_ctx.leds[i].color.g * s_ctx.brightness_global) / 255;
            color.b = (s_ctx.leds[i].color.b * s_ctx.brightness_global) / 255;
        } else {
            // LED off
            color.r = 0;
            color.g = 0;
            color.b = 0;
        }
        
        ws2812_set_pixel(i, color);
    }
    
    // Update hardware
    esp_err_t ret = ws2812_update();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDs: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

// Public API implementation

esp_err_t led_controller_init(void) {
    if (s_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    // Initialize context
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.brightness_global = LED_BRIGHTNESS_DEFAULT;
    
    // Initialize all LEDs to OFF with white color
    led_rgb_t default_color = {255, 255, 255};
    for (int i = 0; i < NUM_LEDS; i++) {
        s_ctx.leds[i].on = false;
        s_ctx.leds[i].color = default_color;
    }
    
    // Configure WS2812
    esp_err_t ret = configure_ws2812();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Clear all LEDs
    ws2812_color_t off = {0, 0, 0};
    ws2812_set_all(off);
    ws2812_update();
    
    s_ctx.initialized = true;
    ESP_LOGI(TAG, "Initialized (%d LEDs, brightness=%d/255)", 
             NUM_LEDS, s_ctx.brightness_global);
    return ESP_OK;
}

esp_err_t led_controller_set_key(uint8_t key_id, bool on, led_rgb_t color) {
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key_id == 0 || key_id > NUM_LEDS) {
        ESP_LOGE(TAG, "Invalid key_id: %d", key_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    int index = key_id - 1;
    s_ctx.leds[index].on = on;
    if (on) {
        s_ctx.leds[index].color = color;
    }
    
    ESP_LOGD(TAG, "Set K%d: %s (R=%d G=%d B=%d)", 
             key_id, on ? "ON" : "OFF", color.r, color.g, color.b);
    
    return ESP_OK;
}

esp_err_t led_controller_set_bulk(uint16_t key_bitmask, bool on, led_rgb_t color) {
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Apply to each key in bitmask
    for (int i = 0; i < NUM_LEDS; i++) {
        if (key_bitmask & (1 << i)) {
            s_ctx.leds[i].on = on;
            if (on) {
                s_ctx.leds[i].color = color;
            }
        }
    }
    
    ESP_LOGD(TAG, "Set bulk: mask=0x%04X %s", key_bitmask, on ? "ON" : "OFF");
    
    return ESP_OK;
}

esp_err_t led_controller_set_all(bool on, led_rgb_t color) {
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    for (int i = 0; i < NUM_LEDS; i++) {
        s_ctx.leds[i].on = on;
        if (on) {
            s_ctx.leds[i].color = color;
        }
    }
    
    ESP_LOGD(TAG, "Set all: %s", on ? "ON" : "OFF");
    
    return ESP_OK;
}

esp_err_t led_controller_update(void) {
    return write_leds_to_hardware();
}

esp_err_t led_controller_get_key(uint8_t key_id, bool *on, led_rgb_t *color) {
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key_id == 0 || key_id > NUM_LEDS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int index = key_id - 1;
    if (on) {
        *on = s_ctx.leds[index].on;
    }
    if (color) {
        *color = s_ctx.leds[index].color;
    }
    
    return ESP_OK;
}
