#include "rgb_status.h"
#include "config.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "rgb_status";

// WS2812 timing (in 0.1us units, with 10MHz resolution)
#define WS2812_T0H_TICKS 4  // 0.4us ±150ns
#define WS2812_T0L_TICKS 8  // 0.85us ±150ns
#define WS2812_T1H_TICKS 8  // 0.8us ±150ns
#define WS2812_T1L_TICKS 4  // 0.45us ±150ns

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static bool initialized = false;
static rgb_status_t current_state = RGB_STATUS_DISCONNECTED;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// RGB color definitions for each state
static const rgb_color_t STATE_COLORS[] = {
    [RGB_STATUS_DISCONNECTED]         = {.r = 0,   .g = 0,   .b = 0},   // OFF
    [RGB_STATUS_WIFI_CONNECTING]      = {.r = 0,   .g = 0,   .b = 255}, // Blue
    [RGB_STATUS_WIFI_ONLY]            = {.r = 255, .g = 255, .b = 0},   // Yellow
    [RGB_STATUS_USERSCRIPT_CONNECTED] = {.r = 128, .g = 0,   .b = 255}, // Purple
    [RGB_STATUS_GAME_STARTED]         = {.r = 0,   .g = 255, .b = 0},   // Green
    [RGB_STATUS_ERROR]                = {.r = 255, .g = 0,   .b = 0}    // Red
};

// Simple bytes encoder for WS2812
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_symbol_word_t ws2812_bit0;
    rmt_symbol_word_t ws2812_bit1;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                           const void *primary_data, size_t data_size,
                           rmt_encode_state_t *ret_state) {
    ws2812_encoder_t *ws_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws_encoder->bytes_encoder;
    return bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, ret_state);
}

static esp_err_t ws2812_reset(rmt_encoder_t *encoder) {
    ws2812_encoder_t *ws_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws_encoder->bytes_encoder;
    return bytes_encoder->reset(bytes_encoder);
}

static esp_err_t ws2812_del(rmt_encoder_t *encoder) {
    ws2812_encoder_t *ws_encoder = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws_encoder->bytes_encoder;
    esp_err_t ret = bytes_encoder->del(bytes_encoder);
    free(ws_encoder);
    return ret;
}

static esp_err_t create_ws2812_encoder(rmt_encoder_handle_t *ret_encoder) {
    ws2812_encoder_t *encoder = calloc(1, sizeof(ws2812_encoder_t));
    if (!encoder) {
        return ESP_ERR_NO_MEM;
    }
    
    encoder->base.encode = ws2812_encode;
    encoder->base.del = ws2812_del;
    encoder->base.reset = ws2812_reset;
    
    // WS2812 bit representations
    encoder->ws2812_bit0.level0 = 1;
    encoder->ws2812_bit0.duration0 = WS2812_T0H_TICKS;
    encoder->ws2812_bit0.level1 = 0;
    encoder->ws2812_bit0.duration1 = WS2812_T0L_TICKS;
    
    encoder->ws2812_bit1.level0 = 1;
    encoder->ws2812_bit1.duration0 = WS2812_T1H_TICKS;
    encoder->ws2812_bit1.level1 = 0;
    encoder->ws2812_bit1.duration1 = WS2812_T1L_TICKS;
    
    // Create bytes encoder
    rmt_bytes_encoder_config_t bytes_config = {
        .bit0 = encoder->ws2812_bit0,
        .bit1 = encoder->ws2812_bit1,
        .flags.msb_first = 1
    };
    
    esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &encoder->bytes_encoder);
    if (ret != ESP_OK) {
        free(encoder);
        return ret;
    }
    
    *ret_encoder = &encoder->base;
    return ESP_OK;
}

static void set_color(rgb_color_t color) {
    if (!initialized || !led_chan || !led_encoder) return;
    
    // WS2812 expects GRB format
    uint8_t led_data[3] = {color.g, color.r, color.b};
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    rmt_transmit(led_chan, led_encoder, led_data, sizeof(led_data), &tx_config);
}

esp_err_t rgb_status_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing RGB status LED on GPIO%d", RGB_LED_GPIO);
    
    // Create RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RGB_LED_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = 10000000, // 10MHz, 0.1us tick
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create WS2812 encoder
    ret = create_ws2812_encoder(&led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WS2812 encoder: %s", esp_err_to_name(ret));
        rmt_del_channel(led_chan);
        return ret;
    }
    
    // Enable RMT channel
    ret = rmt_enable(led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        rmt_del_encoder(led_encoder);
        rmt_del_channel(led_chan);
        return ret;
    }
    
    initialized = true;
    current_state = RGB_STATUS_DISCONNECTED;
    
    // Set initial state (OFF)
    set_color(STATE_COLORS[RGB_STATUS_DISCONNECTED]);
    
    ESP_LOGI(TAG, "RGB status LED initialized with RMT driver");
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
    
    const char *status_names[] = {
        "DISCONNECTED",
        "WIFI_CONNECTING",
        "WIFI_ONLY",
        "USERSCRIPT_CONNECTED",
        "GAME_STARTED",
        "ERROR",
    };
    ESP_LOGI(TAG, "Status: %s -> %s", 
             status_names[current_state], 
             status_names[status]);
    
    current_state = status;
    set_color(STATE_COLORS[status]);
}

rgb_status_t rgb_status_get(void) {
    return current_state;
}
