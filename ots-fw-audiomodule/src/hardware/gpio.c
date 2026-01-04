/**
 * @file gpio.c
 * @brief ESP32-A1S Board GPIO Hardware Abstraction Layer Implementation
 */

#include "gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "gpio";

static bool s_headphones_plugged = false;

esp_err_t gpio_init(void)
{
    ESP_LOGI(TAG, "Initializing board GPIO...");
    
    // Configure PA enable GPIO (power amplifier)
    // Note: Per ESP-ADF best practice, PA is enabled ONCE during init and stays on.
    // Audio on/off is controlled via ES8388 DAC mute (DACCONTROL3), not PA toggling.
    // Rapid PA toggling causes clicks.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PA_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PA_ENABLE_GPIO, 1);  // Enable power amplifier (keep ON)
    ESP_LOGI(TAG, "Power amplifier enabled (GPIO%d) - keeping ON per ESP-ADF", PA_ENABLE_GPIO);
    
    // Configure status LED
    io_conf.pin_bit_mask = (1ULL << GREEN_LED_GPIO);
    gpio_config(&io_conf);
    gpio_set_level(GREEN_LED_GPIO, 0);  // Start OFF, turn on when ready
    ESP_LOGI(TAG, "Status LED initialized (GPIO%d)", GREEN_LED_GPIO);
    
    // Configure headphone detection as input
    io_conf.pin_bit_mask = (1ULL << HEADPHONE_DETECT_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    // Read initial headphone state
    s_headphones_plugged = (gpio_get_level(HEADPHONE_DETECT_GPIO) == 0);
    ESP_LOGI(TAG, "Headphone detection initialized (GPIO%d): %s",
             HEADPHONE_DETECT_GPIO,
             s_headphones_plugged ? "PLUGGED" : "UNPLUGGED");
    
    return ESP_OK;
}

void gpio_set_pa_enable(bool enable)
{
    gpio_set_level(PA_ENABLE_GPIO, enable ? 1 : 0);
}

void gpio_set_led(bool on)
{
    gpio_set_level(GREEN_LED_GPIO, on ? 1 : 0);
}

bool gpio_get_headphone_state(void)
{
    return s_headphones_plugged;
}

bool gpio_update_headphone_detection(void)
{
    bool current_state = (gpio_get_level(HEADPHONE_DETECT_GPIO) == 0);
    
    if (current_state != s_headphones_plugged) {
        s_headphones_plugged = current_state;
        ESP_LOGI(TAG, "Headphones %s",
                 s_headphones_plugged ? "PLUGGED" : "UNPLUGGED");
        return true;  // State changed
    }
    
    return false;  // No change
}

// Button GPIO getters (for future use)
int8_t gpio_get_button_mode(void) { return BUTTON_MODE_GPIO; }
int8_t gpio_get_button_rec(void) { return BUTTON_REC_GPIO; }
int8_t gpio_get_button_play(void) { return BUTTON_PLAY_GPIO; }
int8_t gpio_get_button_set(void) { return BUTTON_SET_GPIO; }
int8_t gpio_get_button_voldown(void) { return BUTTON_VOLDOWN_GPIO; }
int8_t gpio_get_button_volup(void) { return BUTTON_VOLUP_GPIO; }
