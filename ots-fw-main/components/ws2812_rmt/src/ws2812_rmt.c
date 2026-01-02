#include "ws2812_rmt.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ws2812_rmt";

// WS2812 timing (in 0.1us units, with 10MHz resolution)
#define WS2812_T0H_TICKS 4  // 0.4us ±150ns
#define WS2812_T0L_TICKS 8  // 0.85us ±150ns
#define WS2812_T1H_TICKS 8  // 0.8us ±150ns
#define WS2812_T1L_TICKS 4  // 0.45us ±150ns

// Driver state
static rmt_channel_handle_t s_led_chan = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;
static bool s_initialized = false;
static uint8_t *s_led_buffer = NULL;
static uint32_t s_led_count = 0;

// WS2812 encoder structure
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_symbol_word_t ws2812_bit0;
    rmt_symbol_word_t ws2812_bit1;
} ws2812_encoder_t;

// Encoder implementation
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

esp_err_t ws2812_init(const ws2812_config_t *config) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    s_led_count = config->led_count > 0 ? config->led_count : 1;
    uint32_t resolution_hz = config->resolution_hz > 0 ? config->resolution_hz : 10000000;
    
    ESP_LOGI(TAG, "Initializing WS2812 on GPIO%d (%lu LEDs)", config->gpio_num, (unsigned long)s_led_count);
    
    // Allocate LED buffer (GRB format, 3 bytes per LED)
    s_led_buffer = calloc(s_led_count * 3, sizeof(uint8_t));
    if (!s_led_buffer) {
        ESP_LOGE(TAG, "Failed to allocate LED buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Create RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = config->gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = resolution_hz,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &s_led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
        free(s_led_buffer);
        s_led_buffer = NULL;
        return ret;
    }
    
    // Create WS2812 encoder
    ret = create_ws2812_encoder(&s_led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WS2812 encoder: %s", esp_err_to_name(ret));
        rmt_del_channel(s_led_chan);
        s_led_chan = NULL;
        free(s_led_buffer);
        s_led_buffer = NULL;
        return ret;
    }
    
    // Enable RMT channel
    ret = rmt_enable(s_led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        rmt_del_encoder(s_led_encoder);
        s_led_encoder = NULL;
        rmt_del_channel(s_led_chan);
        s_led_chan = NULL;
        free(s_led_buffer);
        s_led_buffer = NULL;
        return ret;
    }
    
    s_initialized = true;
    
    // Clear all LEDs
    memset(s_led_buffer, 0, s_led_count * 3);
    ws2812_update();
    
    ESP_LOGI(TAG, "WS2812 initialized successfully");
    return ESP_OK;
}

esp_err_t ws2812_set_pixel(uint32_t index, ws2812_color_t color) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (index >= s_led_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // WS2812 expects GRB format
    uint32_t offset = index * 3;
    s_led_buffer[offset + 0] = color.g;
    s_led_buffer[offset + 1] = color.r;
    s_led_buffer[offset + 2] = color.b;
    
    return ESP_OK;
}

esp_err_t ws2812_set_all(ws2812_color_t color) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    for (uint32_t i = 0; i < s_led_count; i++) {
        ws2812_set_pixel(i, color);
    }
    
    return ESP_OK;
}

esp_err_t ws2812_update(void) {
    if (!s_initialized || !s_led_chan || !s_led_encoder) {
        return ESP_ERR_INVALID_STATE;
    }
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    esp_err_t ret = rmt_transmit(s_led_chan, s_led_encoder, s_led_buffer, s_led_count * 3, &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

bool ws2812_is_initialized(void) {
    return s_initialized;
}

esp_err_t ws2812_deinit(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Turn off all LEDs
    memset(s_led_buffer, 0, s_led_count * 3);
    ws2812_update();
    
    // Cleanup resources
    if (s_led_encoder) {
        rmt_del_encoder(s_led_encoder);
        s_led_encoder = NULL;
    }
    
    if (s_led_chan) {
        rmt_disable(s_led_chan);
        rmt_del_channel(s_led_chan);
        s_led_chan = NULL;
    }
    
    if (s_led_buffer) {
        free(s_led_buffer);
        s_led_buffer = NULL;
    }
    
    s_led_count = 0;
    s_initialized = false;
    
    ESP_LOGI(TAG, "WS2812 deinitialized");
    return ESP_OK;
}
