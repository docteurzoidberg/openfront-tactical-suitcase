#include "module_io.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "MODULE_IO";

static bool button_states[3] = {false, false, false};  // Debounced button states
static uint32_t button_press_times[3] = {0, 0, 0};

esp_err_t module_io_init(void) {
    ESP_LOGI(TAG, "Initializing module I/O...");
    
    // Configure entire boards (global setup)
    // Board 0 (0x20): ALL pins = INPUT with pullup (buttons, sensors)
    // Board 1 (0x21): ALL pins = OUTPUT (LEDs, relays)
    
    ESP_LOGI(TAG, "Configuring Board %d (0x20) - ALL INPUT with pullup", IO_BOARD_INPUT);
    for (int pin = 0; pin < 16; pin++) {
        if (!io_expander_set_pin_mode(IO_BOARD_INPUT, pin, IO_MODE_INPUT_PULLUP)) {
            ESP_LOGE(TAG, "Failed to configure Board %d pin %d as input", IO_BOARD_INPUT, pin);
            return ESP_FAIL;
        }
    }
    
    ESP_LOGI(TAG, "Configuring Board %d (0x21) - ALL OUTPUT", IO_BOARD_OUTPUT);
    for (int pin = 0; pin < 16; pin++) {
        if (!io_expander_set_pin_mode(IO_BOARD_OUTPUT, pin, IO_MODE_OUTPUT)) {
            ESP_LOGE(TAG, "Failed to configure Board %d pin %d as output", IO_BOARD_OUTPUT, pin);
            return ESP_FAIL;
        }
    }
    
    // Turn off all LEDs initially (entire OUTPUT board)
    for (int pin = 0; pin < 16; pin++) {
        io_expander_digital_write(IO_BOARD_OUTPUT, pin, 0);
    }
    
    ESP_LOGI(TAG, "Module I/O initialized (Board 0=INPUT, Board 1=OUTPUT)");
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
