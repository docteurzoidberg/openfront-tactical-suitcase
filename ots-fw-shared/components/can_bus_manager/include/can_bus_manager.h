#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "can_driver.h"
#include "can_discovery.h"

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
 * @brief Get internal manager statistics.
 */
void can_bus_manager_get_stats(can_bus_manager_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
