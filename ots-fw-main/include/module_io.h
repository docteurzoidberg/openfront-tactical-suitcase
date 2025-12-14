#ifndef MODULE_IO_H
#define MODULE_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "io_expander.h"

// Pin mapping structure
typedef struct {
    uint8_t board;
    uint8_t pin;
} pin_map_t;

// Main Power Module pins
#define MAIN_LED_LINK_BOARD 0
#define MAIN_LED_LINK_PIN   0

// Nuke Module pins
#define NUKE_BTN_ATOM_BOARD  0
#define NUKE_BTN_ATOM_PIN    1
#define NUKE_BTN_HYDRO_BOARD 0
#define NUKE_BTN_HYDRO_PIN   2
#define NUKE_BTN_MIRV_BOARD  0
#define NUKE_BTN_MIRV_PIN    3

#define NUKE_LED_ATOM_BOARD  0
#define NUKE_LED_ATOM_PIN    8
#define NUKE_LED_HYDRO_BOARD 0
#define NUKE_LED_HYDRO_PIN   9
#define NUKE_LED_MIRV_BOARD  0
#define NUKE_LED_MIRV_PIN    10

// Alert Module pins
#define ALERT_LED_WARNING_BOARD 1
#define ALERT_LED_WARNING_PIN   0
#define ALERT_LED_ATOM_BOARD    1
#define ALERT_LED_ATOM_PIN      1
#define ALERT_LED_HYDRO_BOARD   1
#define ALERT_LED_HYDRO_PIN     2
#define ALERT_LED_MIRV_BOARD    1
#define ALERT_LED_MIRV_PIN      3
#define ALERT_LED_LAND_BOARD    1
#define ALERT_LED_LAND_PIN      4
#define ALERT_LED_NAVAL_BOARD   1
#define ALERT_LED_NAVAL_PIN     5

/**
 * @brief Initialize all module I/O pins
 * 
 * @return ESP_OK on success
 */
esp_err_t module_io_init(void);

/**
 * @brief Read nuke button state
 * 
 * @param button 0=atom, 1=hydro, 2=mirv
 * @param pressed Pointer to store button state (true if pressed)
 * @return true on success
 */
bool module_io_read_nuke_button(uint8_t button, bool *pressed);

/**
 * @brief Set nuke LED state
 * 
 * @param led 0=atom, 1=hydro, 2=mirv
 * @param state true=on, false=off
 * @return true on success
 */
bool module_io_set_nuke_led(uint8_t led, bool state);

/**
 * @brief Set alert LED state
 * 
 * @param led 0=warning, 1=atom, 2=hydro, 3=mirv, 4=land, 5=naval
 * @param state true=on, false=off
 * @return true on success
 */
bool module_io_set_alert_led(uint8_t led, bool state);

/**
 * @brief Set main power link LED state
 * 
 * @param state true=on, false=off
 * @return true on success
 */
bool module_io_set_link_led(bool state);

/**
 * @brief Process button debouncing and state changes
 * Should be called periodically from main loop
 */
void module_io_process(void);

#endif // MODULE_IO_H
