#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @file can_driver.h
 * @brief Generic CAN bus driver for ESP32 TWAI interface
 * 
 * Hardware-agnostic CAN driver that can be used by both ots-fw-main and
 * ots-fw-audiomodule. This driver provides low-level CAN frame TX/RX
 * without any application-specific protocol logic.
 * 
 * Current Mode: MOCK (logs frames without physical transmission)
 * Future: Will use ESP-IDF TWAI (Two-Wire Automotive Interface) driver
 * 
 * Hardware Requirements (for physical CAN):
 * - External CAN transceiver (SN65HVD230, MCP2551, TJA1050, etc.)
 * - 120Ω termination resistors at bus ends
 * - Proper ground and power connections
 */

// CAN frame structure (Standard 11-bit ID, 8-byte payload)
typedef struct {
    uint16_t id;          // 11-bit CAN identifier (0x000-0x7FF)
    uint8_t dlc;          // Data length code (0-8 bytes)
    uint8_t data[8];      // Payload data
    bool extended;        // Extended 29-bit ID (not commonly used)
    bool rtr;             // Remote transmission request
} can_frame_t;

// CAN driver configuration
typedef struct {
    int tx_gpio;          // TX pin (e.g., GPIO 21)
    int rx_gpio;          // RX pin (e.g., GPIO 22)
    uint32_t bitrate;     // Bitrate in bps (500000 for 500kbps)
    bool loopback;        // Loopback mode for testing
    bool mock_mode;       // Mock mode (log only, no physical bus)
} can_config_t;

// Default configuration for automatic hardware detection
#define CAN_CONFIG_DEFAULT() { \
    .tx_gpio = 21, \
    .rx_gpio = 22, \
    .bitrate = 500000, \
    .loopback = false, \
    .mock_mode = false \
}

/**
 * @brief Initialize CAN driver
 * 
 * Attempts to initialize physical TWAI hardware. If hardware is not detected
 * or initialization fails, automatically falls back to mock mode.
 * 
 * Mock mode behavior:
 * - Logs all TX frames to serial
 * - No physical bus transmission
 * - Perfect for development without CAN hardware
 * 
 * Physical mode behavior:
 * - Uses ESP32 TWAI controller
 * - Requires external CAN transceiver (SN65HVD230, MCP2551, etc.)
 * - Requires proper bus termination (120Ω at ends)
 * 
 * @param config Driver configuration (NULL for defaults)
 * @return ESP_OK on success (always succeeds with fallback)
 */
esp_err_t can_driver_init(const can_config_t *config);

/**
 * @brief Deinitialize CAN driver
 * 
 * @return ESP_OK on success
 */
esp_err_t can_driver_deinit(void);

/**
 * @brief Check if CAN driver is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool can_driver_is_initialized(void);

/**
 * @brief Send a CAN frame
 * 
 * Non-blocking in mock mode. In physical mode, may block briefly
 * if TX queue is full.
 * 
 * @param frame CAN frame to send
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if TX queue full, error otherwise
 */
esp_err_t can_driver_send(const can_frame_t *frame);

/**
 * @brief Receive a CAN frame
 * 
 * Non-blocking poll. Returns immediately with ESP_ERR_TIMEOUT if no frame available.
 * 
 * @param frame Pointer to store received frame
 * @param timeout_ms Timeout in milliseconds (0 for immediate return)
 * @return ESP_OK if frame received, ESP_ERR_TIMEOUT if no frame, error otherwise
 */
esp_err_t can_driver_receive(can_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief Get number of frames available in RX queue
 * 
 * @return Number of frames in RX queue
 */
uint32_t can_driver_rx_available(void);

/**
 * @brief Get CAN bus statistics
 * 
 * @param tx_count Pointer to store TX frame count (NULL to ignore)
 * @param rx_count Pointer to store RX frame count (NULL to ignore)
 * @param tx_errors Pointer to store TX error count (NULL to ignore)
 * @param rx_errors Pointer to store RX error count (NULL to ignore)
 * @return ESP_OK on success
 */
esp_err_t can_driver_get_stats(uint32_t *tx_count, uint32_t *rx_count, 
                               uint32_t *tx_errors, uint32_t *rx_errors);

/**
 * @brief Reset CAN bus statistics
 */
void can_driver_reset_stats(void);

/**
 * @brief Set CAN RX filter
 * 
 * In physical mode, configures hardware acceptance filter.
 * In mock mode, has no effect.
 * 
 * @param filter_id Filter ID (CAN identifier to accept)
 * @param filter_mask Filter mask (1 = must match, 0 = don't care)
 * @return ESP_OK on success
 */
esp_err_t can_driver_set_filter(uint16_t filter_id, uint16_t filter_mask);

#endif // CAN_DRIVER_H
