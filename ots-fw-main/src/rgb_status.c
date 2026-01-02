#include "rgb_status.h"
#include "config.h"
#include "ws2812_rmt.h"
#include "esp_log.h"

static const char *TAG = "OTS_RGB";

static bool initialized = false;
static rgb_status_t current_state = RGB_STATUS_DISCONNECTED;

// Color definitions for each state
static const ws2812_color_t STATE_COLORS[] = {
    [RGB_STATUS_DISCONNECTED]         = {.r = 0,   .g = 0,   .b = 0},   // OFF
    [RGB_STATUS_WIFI_CONNECTING]      = {.r = 0,   .g = 0,   .b = 255}, // Blue
    [RGB_STATUS_WIFI_ONLY]            = {.r = 255, .g = 255, .b = 0},   // Yellow
    [RGB_STATUS_USERSCRIPT_CONNECTED] = {.r = 128, .g = 0,   .b = 255}, // Purple
    [RGB_STATUS_GAME_STARTED]         = {.r = 0,   .g = 255, .b = 0},   // Green
    [RGB_STATUS_ERROR]                = {.r = 255, .g = 0,   .b = 0}    // Red
};

esp_err_t rgb_status_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing RGB status LED on GPIO%d", RGB_LED_GPIO);
    
    ws2812_config_t config = {
        .gpio_num = RGB_LED_GPIO,
        .led_count = 1,
        .resolution_hz = 10000000  // 10MHz
    };
    
    esp_err_t ret = ws2812_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WS2812 driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    initialized = true;
    current_state = RGB_STATUS_DISCONNECTED;
    
    // Set initial state (OFF)
    ws2812_set_pixel(0, STATE_COLORS[RGB_STATUS_DISCONNECTED]);
    ws2812_update();
    
    ESP_LOGI(TAG, "RGB status LED initialized");
    return ESP_OK;
}

void rgb_status_set(rgb_status_t status) {
    if (!initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    if (status < 0 || status >= RGB_STATUS__COUNT) {
        ESP_LOGE(TAG, "Invalid status: %d", status);
        return;
    }
    
    if (status == current_state) {
        return;  // No change needed
    }
    
    current_state = status;
    ws2812_set_pixel(0, STATE_COLORS[status]);
    ws2812_update();
}

rgb_status_t rgb_status_get(void) {
    return current_state;
}
