/**
 * @file can_handler.h
 * @brief CAN Handler Module - CAN Bus Communication
 * 
 * Handles CAN bus communication using can_bus_manager.
 * Sends key events and receives LED commands.
 */

#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <stdint.h>
#include "esp_err.h"
#include "matrix_scanner.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize CAN handler
 * 
 * Initializes can_bus_manager and registers RX handlers for LED commands.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t can_handler_init(void);

/**
 * @brief Start CAN handler
 * 
 * Sends MODULE_ANNOUNCE to discover main controller.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t can_handler_start(void);

/**
 * @brief Stop CAN handler
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t can_handler_stop(void);

/**
 * @brief Send key event to CAN bus
 * 
 * Builds and sends a KEY_EVENT CAN message to the main controller.
 * 
 * @param key_id Key identifier (1-15)
 * @param state Key state (pressed or released)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t can_handler_send_key_event(uint8_t key_id, key_state_t state);

#ifdef __cplusplus
}
#endif

#endif // CAN_HANDLER_H
