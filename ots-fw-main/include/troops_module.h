#ifndef TROOPS_MODULE_H
#define TROOPS_MODULE_H

#include "hardware_module.h"
#include <stdint.h>
#include <stdbool.h>

// I2C addresses
#define TROOPS_ADS1015_ADDR     0x48    // ADS1015 ADC
#define TROOPS_LCD_ADDR         0x27    // LCD I2C backpack (PCF8574)

// ADS1015 configuration
#define ADS1015_CHANNEL_AIN0    0       // Slider on AIN0

// Polling intervals
#define TROOPS_SLIDER_POLL_MS   100     // Poll slider every 100ms
#define TROOPS_CHANGE_THRESHOLD 1       // Send command on ≥1% change

// LCD dimensions
#define LCD_COLS                16
#define LCD_ROWS                2

/**
 * @brief Troops module state
 */
typedef struct {
    uint32_t current_troops;        // Current troop count from server
    uint32_t max_troops;            // Maximum troop count from server
    uint8_t slider_percent;         // Current slider position (0-100)
    uint8_t last_sent_percent;      // Last percent value sent to server
    uint64_t last_slider_read;      // Timestamp of last slider read (ms)
    bool display_dirty;             // LCD needs update
    bool initialized;               // Module initialization complete
} troops_module_state_t;

/**
 * @brief Get the troops module instance
 * 
 * @return Pointer to hardware_module_t interface
 */
const hardware_module_t* troops_module_get(void);

/**
 * @brief Format troop count with K/M/B scaling
 * 
 * @param troops Troop count to format
 * @param buffer Output buffer (min 8 bytes)
 * @param buffer_size Size of output buffer
 */
void troops_format_count(uint32_t troops, char* buffer, size_t buffer_size);

#endif // TROOPS_MODULE_H
