#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "can_driver.h"

// Discovery protocol constants (CANBUS_MESSAGE_SPEC v1.0)

#define CAN_ID_MODULE_ANNOUNCE  0x410  // Module → Main (response)
#define CAN_ID_MODULE_QUERY     0x411  // Main → All modules (broadcast)

// Module types
#define MODULE_TYPE_NONE        0x00
#define MODULE_TYPE_AUDIO       0x01

// Capability flags
#define MODULE_CAP_STATUS       (1 << 0)
#define MODULE_CAP_OTA          (1 << 1)
#define MODULE_CAP_BATTERY      (1 << 2)

typedef struct {
    uint8_t module_type;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t capabilities;
    uint8_t can_block_base;
    uint8_t node_id;
} can_discovery_announce_t;

static inline const char *can_discovery_get_module_name(uint8_t module_type) {
    switch (module_type) {
        case MODULE_TYPE_AUDIO:
            return "Audio Module";
        case MODULE_TYPE_NONE:
            return "None";
        default:
            return "Unknown Module";
    }
}

static inline esp_err_t can_discovery_build_query_all(can_frame_t *out_frame) {
    if (!out_frame) return ESP_ERR_INVALID_ARG;

    can_frame_t frame = {0};
    frame.id = CAN_ID_MODULE_QUERY;
    frame.extended = false;
    frame.rtr = false;
    frame.dlc = 8;
    frame.data[0] = 0xFF;

    *out_frame = frame;
    return ESP_OK;
}

static inline esp_err_t can_discovery_build_announce(can_frame_t *out_frame,
                                                    uint8_t module_type,
                                                    uint8_t version_major,
                                                    uint8_t version_minor,
                                                    uint8_t capabilities,
                                                    uint8_t can_block_base,
                                                    uint8_t node_id) {
    if (!out_frame) return ESP_ERR_INVALID_ARG;

    can_frame_t frame = {0};
    frame.id = CAN_ID_MODULE_ANNOUNCE;
    frame.extended = false;
    frame.rtr = false;
    frame.dlc = 8;

    frame.data[0] = module_type;
    frame.data[1] = version_major;
    frame.data[2] = version_minor;
    frame.data[3] = capabilities;
    frame.data[4] = can_block_base;
    frame.data[5] = node_id;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    *out_frame = frame;
    return ESP_OK;
}

static inline bool can_discovery_parse_announce(const can_frame_t *frame, can_discovery_announce_t *out_info) {
    if (!frame || !out_info) return false;
    if ((uint16_t)frame->id != CAN_ID_MODULE_ANNOUNCE) return false;
    if (frame->dlc < 8) return false;

    out_info->module_type = frame->data[0];
    out_info->version_major = frame->data[1];
    out_info->version_minor = frame->data[2];
    out_info->capabilities = frame->data[3];
    out_info->can_block_base = frame->data[4];
    out_info->node_id = frame->data[5];
    return true;
}
