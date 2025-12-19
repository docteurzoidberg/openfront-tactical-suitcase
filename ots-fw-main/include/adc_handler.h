#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief ADC channel identifiers
 */
typedef enum {
    ADC_CHANNEL_TROOPS_SLIDER = 0,  // ADS1015 AIN0 - Troops deployment slider
    ADC_CHANNEL_COUNT
} adc_channel_id_t;

/**
 * @brief ADC event structure
 */
typedef struct {
    adc_channel_id_t channel;   // Which ADC channel changed
    uint16_t raw_value;         // Raw ADC value (0-4095 for 12-bit)
    uint8_t percent;            // Converted percentage (0-100)
    uint32_t timestamp_ms;      // Timestamp of the event
} adc_event_t;

/**
 * @brief Initialize ADC handler
 * 
 * Initializes ADC hardware and internal state tracking
 * 
 * @return ESP_OK on success
 */
esp_err_t adc_handler_init(void);

/**
 * @brief Scan all ADC channels and update state
 * 
 * Should be called periodically (e.g., every 100ms) from I/O task
 * Updates internal state that modules can query via adc_handler_get_value()
 * 
 * @return ESP_OK on success
 */
esp_err_t adc_handler_scan(void);

/**
 * @brief Get current ADC value
 * 
 * Modules can call this to query current ADC state (updated by io_task)
 * 
 * @param channel ADC channel to read
 * @param value Output: ADC event structure with raw value, percent, timestamp
 * @return ESP_OK on success, ESP_FAIL if invalid channel or not initialized
 */
esp_err_t adc_handler_get_value(adc_channel_id_t channel, adc_event_t *value);

/**
 * @brief Shutdown ADC handler
 * 
 * @return ESP_OK on success
 */
esp_err_t adc_handler_shutdown(void);

#endif // ADC_HANDLER_H
