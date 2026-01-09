#include "can_discovery.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "can_discovery";

// ============================================================================
// MODULE SIDE IMPLEMENTATION
// ============================================================================

esp_err_t can_discovery_announce(uint8_t module_type,
                                  uint8_t version_major,
                                  uint8_t version_minor,
                                  uint8_t capabilities,
                                  uint8_t can_block_base,
                                  uint8_t node_id)
{
    can_frame_t msg = {
        .id = CAN_ID_MODULE_ANNOUNCE,
        .dlc = 8,
        .data = {
            module_type,
            version_major,
            version_minor,
            capabilities,
            can_block_base,
            node_id,
            0x00,  // Reserved
            0x00   // Reserved
        }
    };
    
    ESP_LOGI(TAG, "Attempting to send MODULE_ANNOUNCE (type=0x%02X ver=%d.%d block=0x%02X)...",
             module_type, version_major, version_minor, can_block_base);
    
    esp_err_t ret = can_driver_send(&msg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ MODULE_ANNOUNCE sent successfully");
    } else {
        ESP_LOGE(TAG, "✗ Failed to send MODULE_ANNOUNCE: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t can_discovery_handle_query(can_frame_t *msg,
                                      uint8_t module_type,
                                      uint8_t version_major,
                                      uint8_t version_minor,
                                      uint8_t capabilities,
                                      uint8_t can_block_base,
                                      uint8_t node_id)
{
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if this is a query message
    if (msg->id != CAN_ID_MODULE_QUERY) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check magic byte (0xFF = enumerate all)
    if (msg->dlc > 0 && msg->data[0] == 0xFF) {
        ESP_LOGI(TAG, "Received MODULE_QUERY, announcing...");
        return can_discovery_announce(module_type, version_major, version_minor,
                                       capabilities, can_block_base, node_id);
    }
    
    return ESP_ERR_NOT_FOUND;
}

// ============================================================================
// MAIN CONTROLLER IMPLEMENTATION
// ============================================================================

esp_err_t can_discovery_query_all(void)
{
    can_frame_t msg = {
        .id = CAN_ID_MODULE_QUERY,
        .dlc = 8,
        .data = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    };
    
    esp_err_t ret = can_driver_send(&msg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sent MODULE_QUERY (enumerate all)");
    } else {
        ESP_LOGE(TAG, "Failed to send MODULE_QUERY: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t can_discovery_parse_announce(can_frame_t *msg, module_info_t *info)
{
    if (!msg || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (msg->id != CAN_ID_MODULE_ANNOUNCE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (msg->dlc < 6) {
        ESP_LOGW(TAG, "ANNOUNCE message too short (%d bytes)", msg->dlc);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Parse message
    info->module_type = msg->data[0];
    info->version_major = msg->data[1];
    info->version_minor = msg->data[2];
    info->capabilities = msg->data[3];
    info->can_block_base = msg->data[4];
    info->node_id = msg->data[5];
    info->discovered = true;
    
    ESP_LOGI(TAG, "Discovered %s v%d.%d (CAN block 0x%02X0-0x%02XF, node %d)",
             can_discovery_get_module_name(info->module_type),
             info->version_major,
             info->version_minor,
             info->can_block_base,
             info->can_block_base,
             info->node_id);
    
    return ESP_OK;
}

const char* can_discovery_get_module_name(uint8_t module_type)
{
    switch (module_type) {
        case MODULE_TYPE_AUDIO:   return "Audio Module";
        default:                  return "Unknown Module";
    }
}
