#include "module_io.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "MODULE_IO";

static bool button_states[3] = {false, false, false};  // Debounced button states
static uint32_t button_press_times[3] = {0, 0, 0};

esp_err_t module_io_init(void) {
    ESP_LOGI(TAG, "Initializing module I/O...");
    
    // Configure nuke button inputs
    if (!io_expander_set_pin_mode(NUKE_BTN_ATOM_BOARD, NUKE_BTN_ATOM_PIN, IO_MODE_INPUT_PULLUP)) {
        ESP_LOGE(TAG, "Failed to configure atom button");
        return ESP_FAIL;
    }
    if (!io_expander_set_pin_mode(NUKE_BTN_HYDRO_BOARD, NUKE_BTN_HYDRO_PIN, IO_MODE_INPUT_PULLUP)) {
        ESP_LOGE(TAG, "Failed to configure hydro button");
        return ESP_FAIL;
    }
    if (!io_expander_set_pin_mode(NUKE_BTN_MIRV_BOARD, NUKE_BTN_MIRV_PIN, IO_MODE_INPUT_PULLUP)) {
        ESP_LOGE(TAG, "Failed to configure mirv button");
        return ESP_FAIL;
    }
    
    // Configure nuke LED outputs
    io_expander_set_pin_mode(NUKE_LED_ATOM_BOARD, NUKE_LED_ATOM_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(NUKE_LED_HYDRO_BOARD, NUKE_LED_HYDRO_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(NUKE_LED_MIRV_BOARD, NUKE_LED_MIRV_PIN, IO_MODE_OUTPUT);
    
    // Configure alert LED outputs
    io_expander_set_pin_mode(ALERT_LED_WARNING_BOARD, ALERT_LED_WARNING_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(ALERT_LED_ATOM_BOARD, ALERT_LED_ATOM_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(ALERT_LED_HYDRO_BOARD, ALERT_LED_HYDRO_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(ALERT_LED_MIRV_BOARD, ALERT_LED_MIRV_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(ALERT_LED_LAND_BOARD, ALERT_LED_LAND_PIN, IO_MODE_OUTPUT);
    io_expander_set_pin_mode(ALERT_LED_NAVAL_BOARD, ALERT_LED_NAVAL_PIN, IO_MODE_OUTPUT);
    
    // Configure main power link LED
    io_expander_set_pin_mode(MAIN_LED_LINK_BOARD, MAIN_LED_LINK_PIN, IO_MODE_OUTPUT);
    
    // Turn off all LEDs initially
    module_io_set_nuke_led(0, false);
    module_io_set_nuke_led(1, false);
    module_io_set_nuke_led(2, false);
    module_io_set_alert_led(0, false);
    module_io_set_alert_led(1, false);
    module_io_set_alert_led(2, false);
    module_io_set_alert_led(3, false);
    module_io_set_alert_led(4, false);
    module_io_set_alert_led(5, false);
    module_io_set_link_led(false);
    
    ESP_LOGI(TAG, "Module I/O initialized");
    return ESP_OK;
}

bool module_io_read_nuke_button(uint8_t button, bool *pressed) {
    if (button > 2 || !pressed) {
        return false;
    }
    
    uint8_t board, pin;
    
    switch (button) {
        case 0:  // atom
            board = NUKE_BTN_ATOM_BOARD;
            pin = NUKE_BTN_ATOM_PIN;
            break;
        case 1:  // hydro
            board = NUKE_BTN_HYDRO_BOARD;
            pin = NUKE_BTN_HYDRO_PIN;
            break;
        case 2:  // mirv
            board = NUKE_BTN_MIRV_BOARD;
            pin = NUKE_BTN_MIRV_PIN;
            break;
        default:
            return false;
    }
    
    uint8_t value;
    if (!io_expander_digital_read(board, pin, &value)) {
        return false;
    }
    
    // Button is active low (pressed = 0)
    *pressed = (value == 0);
    return true;
}

bool module_io_set_nuke_led(uint8_t led, bool state) {
    if (led > 2) {
        return false;
    }
    
    uint8_t board, pin;
    
    switch (led) {
        case 0:  // atom
            board = NUKE_LED_ATOM_BOARD;
            pin = NUKE_LED_ATOM_PIN;
            break;
        case 1:  // hydro
            board = NUKE_LED_HYDRO_BOARD;
            pin = NUKE_LED_HYDRO_PIN;
            break;
        case 2:  // mirv
            board = NUKE_LED_MIRV_BOARD;
            pin = NUKE_LED_MIRV_PIN;
            break;
        default:
            return false;
    }
    
    return io_expander_digital_write(board, pin, state ? 1 : 0);
}

bool module_io_set_alert_led(uint8_t led, bool state) {
    if (led > 5) {
        return false;
    }
    
    uint8_t board, pin;
    
    switch (led) {
        case 0:  // warning
            board = ALERT_LED_WARNING_BOARD;
            pin = ALERT_LED_WARNING_PIN;
            break;
        case 1:  // atom
            board = ALERT_LED_ATOM_BOARD;
            pin = ALERT_LED_ATOM_PIN;
            break;
        case 2:  // hydro
            board = ALERT_LED_HYDRO_BOARD;
            pin = ALERT_LED_HYDRO_PIN;
            break;
        case 3:  // mirv
            board = ALERT_LED_MIRV_BOARD;
            pin = ALERT_LED_MIRV_PIN;
            break;
        case 4:  // land
            board = ALERT_LED_LAND_BOARD;
            pin = ALERT_LED_LAND_PIN;
            break;
        case 5:  // naval
            board = ALERT_LED_NAVAL_BOARD;
            pin = ALERT_LED_NAVAL_PIN;
            break;
        default:
            return false;
    }
    
    return io_expander_digital_write(board, pin, state ? 1 : 0);
}

bool module_io_set_link_led(bool state) {
    return io_expander_digital_write(MAIN_LED_LINK_BOARD, MAIN_LED_LINK_PIN, state ? 1 : 0);
}

void module_io_process(void) {
    // This function can be used for button debouncing or periodic tasks
    // Currently not needed as we handle buttons on-demand
}
