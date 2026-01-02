#include "adc_handler.h"
#include "adc_driver.h"
#include "i2c_bus.h"
#include "event_dispatcher.h"
#include "protocol.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "OTS_ADC_HDLR";

// ADC channel configuration
typedef struct {
    adc_channel_id_t id;           // Channel identifier
    uint8_t adc_channel;           // Physical ADC channel (0-3)
    uint8_t i2c_addr;              // I2C address of ADC
    uint8_t change_threshold;      // Minimum change to trigger event (percent)
    const char *name;              // Channel name for logging
} adc_channel_config_t;

// ADC channel state tracking
typedef struct {
    uint16_t last_raw_value;       // Last raw ADC reading
    uint8_t last_percent;          // Last percentage value
    uint32_t last_read_time;       // Time of last successful read (ms)
} adc_channel_state_t;

// Channel configuration
static const adc_channel_config_t channel_configs[ADC_CHANNEL_COUNT] = {
    {
        .id = ADC_CHANNEL_TROOPS_SLIDER,
        .adc_channel = 0,  // AIN0
        .i2c_addr = 0x48,  // ADS1015 default address
        .name = "troops_slider"
    }
};

// Channel states
static adc_channel_state_t channel_states[ADC_CHANNEL_COUNT] = {0};
static bool initialized = false;

esp_err_t adc_handler_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "ADC handler already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ADC handler...");
    
    // Initialize ADS1015 ADC
    esp_err_t ret = ads1015_init(ots_i2c_bus_get(), 0x48);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize channel states
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        channel_states[i].last_raw_value = 0;
        channel_states[i].last_percent = 0;
        channel_states[i].last_read_time = 0;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "ADC handler initialized (%d channels)", ADC_CHANNEL_COUNT);
    return ESP_OK;
}

bool adc_handler_is_initialized(void) {
    return initialized;
}

esp_err_t adc_handler_scan(void) {
    if (!initialized) {
        return ESP_FAIL;
    }
    
    uint32_t now = esp_timer_get_time() / 1000;  // Convert to ms
    
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        const adc_channel_config_t *config = &channel_configs[i];
        adc_channel_state_t *state = &channel_states[i];
        
        // Read ADC channel
        int16_t raw_value = ads1015_read_channel(config->adc_channel);
        if (raw_value < 0) {
            ESP_LOGD(TAG, "Failed to read ADC channel %s", config->name);
            continue;
        }
        
        // Convert to percentage (12-bit ADC: 0-4095)
        uint8_t new_percent = (raw_value * 100) / 4095;
        if (new_percent > 100) new_percent = 100;
        
        // Update state unconditionally - modules handle change detection
        state->last_raw_value = raw_value;
        state->last_percent = new_percent;
        state->last_read_time = now;
    }
    
    return ESP_OK;
}

esp_err_t adc_handler_get_value(adc_channel_id_t channel, adc_event_t *value) {
    if (!initialized || channel >= ADC_CHANNEL_COUNT || !value) {
        return ESP_FAIL;
    }
    
    adc_channel_state_t *state = &channel_states[channel];
    const adc_channel_config_t *config = &channel_configs[channel];
    
    value->channel = config->id;
    value->raw_value = state->last_raw_value;
    value->percent = state->last_percent;
    value->timestamp_ms = state->last_read_time / 1000;
    
    return ESP_OK;
}

esp_err_t adc_handler_shutdown(void) {
    if (!initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Shutting down ADC handler");
    initialized = false;
    
    return ESP_OK;
}
