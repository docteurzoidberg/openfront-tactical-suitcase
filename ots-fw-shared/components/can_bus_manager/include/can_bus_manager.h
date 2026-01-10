#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "can_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Configuration defaults ----

#ifndef CAN_BUS_MANAGER_TX_QUEUE_DEPTH
#define CAN_BUS_MANAGER_TX_QUEUE_DEPTH 32
#endif

#ifndef CAN_BUS_MANAGER_MAX_HANDLERS
#define CAN_BUS_MANAGER_MAX_HANDLERS 32
#endif

#ifndef CAN_BUS_MANAGER_MAX_MODULES
#define CAN_BUS_MANAGER_MAX_MODULES 16
#endif

#ifndef CAN_BUS_MANAGER_RX_TIMEOUT_MS
#define CAN_BUS_MANAGER_RX_TIMEOUT_MS 100
#endif

#ifndef CAN_BUS_MANAGER_RECOVERY_BACKOFF_MS
#define CAN_BUS_MANAGER_RECOVERY_BACKOFF_MS 5000
#endif

// ---- Public API ----

typedef void (*can_bus_rx_handler_t)(const can_frame_t *frame, void *ctx);

typedef struct {
    uint32_t tx_enqueued;
    uint32_t tx_sent_ok;
    uint32_t tx_send_timeouts;
    uint32_t tx_send_errors;
    uint32_t tx_dropped_queue_full;

    uint32_t rx_received;
    uint32_t rx_dispatched;

    uint32_t recovery_attempts;
} can_bus_manager_stats_t;

// ---- Optional discovery registry ----

typedef struct {
    bool valid;
    uint8_t module_type;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t capabilities;
    uint8_t can_block_base;
    uint8_t node_id;
    uint64_t last_seen_ms;
} can_bus_module_info_t;

/**
 * @brief Initialize CAN bus manager.
 *
 * - Initializes the underlying can_driver (idempotent).
 * - Starts a single RX task and TX task.
 *
 * Safe to call multiple times; subsequent calls return ESP_OK.
 */
esp_err_t can_bus_manager_init(const can_config_t *config);

/**
 * @brief Stop tasks/queue (does not currently deinit can_driver).
 */
esp_err_t can_bus_manager_deinit(void);

/**
 * @brief Enqueue a CAN frame for transmission (non-blocking).
 */
esp_err_t can_bus_manager_send(const can_frame_t *frame);

/**
 * @brief Register an RX handler matched by (frame.id & mask) == (id & mask).
 */
esp_err_t can_bus_manager_register_handler(uint16_t id, uint16_t mask, can_bus_rx_handler_t cb, void *ctx);

/**
 * @brief Broadcast a MODULE_QUERY (0x411) to discover modules.
 *
 * Non-blocking: enqueues the query frame into TX queue.
 */
esp_err_t can_bus_manager_discovery_query_all(void);

/**
 * @brief Lookup discovered module info by (module_type, node_id).
 *
 * This is populated opportunistically by parsing MODULE_ANNOUNCE frames.
 */
esp_err_t can_bus_manager_get_module(uint8_t module_type, uint8_t node_id, can_bus_module_info_t *out_info);

/**
 * @brief True if module exists in registry and (optionally) was seen recently.
 *
 * If max_age_ms is 0, only checks existence.
 */
bool can_bus_manager_is_module_present(uint8_t module_type, uint8_t node_id, uint64_t max_age_ms);

/**
 * @brief Copy up to max_items modules into out_items. Returns number copied.
 */
size_t can_bus_manager_list_modules(can_bus_module_info_t *out_items, size_t max_items);

/**
 * @brief Get internal manager statistics.
 */
void can_bus_manager_get_stats(can_bus_manager_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
