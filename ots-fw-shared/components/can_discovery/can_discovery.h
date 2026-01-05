#ifndef CAN_DISCOVERY_H
#define CAN_DISCOVERY_H

#include "can_driver.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @file can_discovery.h
 * @brief CAN module discovery protocol (boot-time only)
 * 
 * Simple discovery protocol:
 * 1. Main controller sends MODULE_QUERY on boot
 * 2. Modules respond with MODULE_ANNOUNCE
 * 3. Main controller waits 500ms for responses
 * 4. Done - no heartbeat tracking
 */

// ============================================================================
// CAN IDs
// ============================================================================

#define CAN_ID_MODULE_ANNOUNCE  0x410  // Module → Main (response)
#define CAN_ID_MODULE_QUERY     0x411  // Main → All modules (broadcast)

// ============================================================================
// MODULE TYPES
// ============================================================================

#define MODULE_TYPE_NONE        0x00
#define MODULE_TYPE_AUDIO       0x01
// 0x02-0xFF: Reserved for future modules

// ============================================================================
// CAPABILITY FLAGS
// ============================================================================

#define MODULE_CAP_STATUS       (1 << 0)  // Sends periodic status messages
#define MODULE_CAP_OTA          (1 << 1)  // Supports firmware updates (future)
#define MODULE_CAP_BATTERY      (1 << 2)  // Battery powered (future)

// ============================================================================
// MODULE INFO STRUCTURE
// ============================================================================

typedef struct {
    uint8_t module_type;        // MODULE_TYPE_* constant
    uint8_t version_major;      // Firmware version major
    uint8_t version_minor;      // Firmware version minor
    uint8_t capabilities;       // Capability flags (bitfield)
    uint8_t can_block_base;     // CAN ID block base (0x42 for audio = 0x420-0x42F)
    uint8_t node_id;            // Node ID (0 for single module)
    bool discovered;            // true if module responded to query
} module_info_t;

// ============================================================================
// MODULE SIDE FUNCTIONS (Audio module, light module, etc.)
// ============================================================================

/**
 * @brief Send MODULE_ANNOUNCE message (response to query)
 * @param module_type Module type (MODULE_TYPE_AUDIO, etc.)
 * @param version_major Firmware version major
 * @param version_minor Firmware version minor
 * @param capabilities Capability flags (bitfield)
 * @param can_block_base CAN ID block base (e.g., 0x42 for 0x420-0x42F)
 * @param node_id Node ID (0 for single module)
 * @return ESP_OK on success
 */
esp_err_t can_discovery_announce(uint8_t module_type,
                                  uint8_t version_major,
                                  uint8_t version_minor,
                                  uint8_t capabilities,
                                  uint8_t can_block_base,
                                  uint8_t node_id);

/**
 * @brief Handle MODULE_QUERY from main controller
 * @param msg Received CAN message
 * @param module_type This module's type
 * @param version_major Firmware version major
 * @param version_minor Firmware version minor
 * @param capabilities Capability flags
 * @param can_block_base CAN ID block base
 * @param node_id Node ID
 * @return ESP_OK if query was for us and we responded
 * 
 * Usage in module's CAN RX handler:
 *   if (msg.identifier == CAN_ID_MODULE_QUERY) {
 *       can_discovery_handle_query(&msg, MODULE_TYPE_AUDIO, 1, 0, 
 *                                   MODULE_CAP_STATUS, 0x42, 0);
 *   }
 */
esp_err_t can_discovery_handle_query(can_frame_t *msg,
                                      uint8_t module_type,
                                      uint8_t version_major,
                                      uint8_t version_minor,
                                      uint8_t capabilities,
                                      uint8_t can_block_base,
                                      uint8_t node_id);

// ============================================================================
// MAIN CONTROLLER FUNCTIONS (ots-fw-main)
// ============================================================================

/**
 * @brief Send MODULE_QUERY to discover all modules
 * @return ESP_OK on success
 * 
 * Usage:
 *   can_discovery_query_all();
 *   vTaskDelay(pdMS_TO_TICKS(500));  // Wait for responses
 */
esp_err_t can_discovery_query_all(void);

/**
 * @brief Handle MODULE_ANNOUNCE from a module
 * @param msg Received CAN message
 * @param info Output parameter - filled with module info
 * @return ESP_OK if announce was valid and parsed
 * 
 * Usage in main controller's CAN RX handler:
 *   if (msg.identifier == CAN_ID_MODULE_ANNOUNCE) {
 *       module_info_t info;
 *       if (can_discovery_parse_announce(&msg, &info) == ESP_OK) {
 *           // Register module...
 *       }
 *   }
 */
esp_err_t can_discovery_parse_announce(can_frame_t *msg, module_info_t *info);

/**
 * @brief Get module type name (for logging)
 * @param module_type Module type constant
 * @return String name ("Audio Module", "Light Module", etc.)
 */
const char* can_discovery_get_module_name(uint8_t module_type);

#endif // CAN_DISCOVERY_H
