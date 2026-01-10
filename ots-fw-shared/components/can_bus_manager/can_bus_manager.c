#include "can_bus_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "CAN_BUS_MGR";

typedef struct {
    uint16_t id;
    uint16_t mask;
    can_bus_rx_handler_t cb;
    void *ctx;
    bool used;
} handler_entry_t;

typedef struct {
    bool initialized;

    TaskHandle_t rx_task;
    TaskHandle_t tx_task;
    QueueHandle_t tx_queue;

    handler_entry_t handlers[CAN_BUS_MANAGER_MAX_HANDLERS];

    uint64_t last_recovery_attempt_ms;

    can_bus_manager_stats_t stats;
} can_bus_manager_state_t;

static can_bus_manager_state_t s_mgr = {0};

static uint64_t uptime_ms(void) {
    // FreeRTOS tick-based uptime; sufficient for rate-limiting recovery attempts.
    return (uint64_t)xTaskGetTickCount() * (uint64_t)portTICK_PERIOD_MS;
}

typedef struct {
    can_frame_t frame;
} tx_item_t;

static void rx_task(void *arg) {
    (void)arg;

    can_frame_t frame;
    ESP_LOGI(TAG, "RX task started");

    while (1) {
        esp_err_t ret = can_driver_receive(&frame, CAN_BUS_MANAGER_RX_TIMEOUT_MS);
        if (ret == ESP_OK) {
            s_mgr.stats.rx_received++;

            // Dispatch to matching handlers
            for (size_t i = 0; i < CAN_BUS_MANAGER_MAX_HANDLERS; i++) {
                handler_entry_t *h = &s_mgr.handlers[i];
                if (!h->used || !h->cb) {
                    continue;
                }
                if (((uint16_t)frame.id & h->mask) == (h->id & h->mask)) {
                    s_mgr.stats.rx_dispatched++;
                    h->cb(&frame, h->ctx);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

static void tx_task(void *arg) {
    (void)arg;

    ESP_LOGI(TAG, "TX task started");

    tx_item_t item;
    while (1) {
        if (xQueueReceive(s_mgr.tx_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        esp_err_t ret = can_driver_send(&item.frame);
        if (ret == ESP_OK) {
            s_mgr.stats.tx_sent_ok++;
        } else if (ret == ESP_ERR_TIMEOUT) {
            s_mgr.stats.tx_send_timeouts++;
            // No ACKs present; do not block callers. Recovery is not always needed.
        } else {
            s_mgr.stats.tx_send_errors++;

            const uint64_t now_ms = uptime_ms();
            if (now_ms - s_mgr.last_recovery_attempt_ms >= CAN_BUS_MANAGER_RECOVERY_BACKOFF_MS) {
                s_mgr.last_recovery_attempt_ms = now_ms;
                s_mgr.stats.recovery_attempts++;

                ESP_LOGW(TAG, "TX error (%s) - attempting recovery/start", esp_err_to_name(ret));
                can_driver_log_twai_status();
                (void)can_driver_recover();
                vTaskDelay(pdMS_TO_TICKS(50));
                (void)can_driver_start();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t can_bus_manager_init(const can_config_t *config) {
    if (s_mgr.initialized) {
        return ESP_OK;
    }

    memset(&s_mgr, 0, sizeof(s_mgr));

    esp_err_t ret = can_driver_init(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "can_driver_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_mgr.tx_queue = xQueueCreate(CAN_BUS_MANAGER_TX_QUEUE_DEPTH, sizeof(tx_item_t));
    if (!s_mgr.tx_queue) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(rx_task, "can_rx", 4096, NULL, 6, &s_mgr.rx_task, tskNO_AFFINITY);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    ok = xTaskCreatePinnedToCore(tx_task, "can_tx", 4096, NULL, 5, &s_mgr.tx_task, tskNO_AFFINITY);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    s_mgr.initialized = true;
    ESP_LOGI(TAG, "CAN bus manager initialized");
    return ESP_OK;
}

esp_err_t can_bus_manager_deinit(void) {
    if (!s_mgr.initialized) {
        return ESP_OK;
    }

    if (s_mgr.rx_task) {
        vTaskDelete(s_mgr.rx_task);
        s_mgr.rx_task = NULL;
    }
    if (s_mgr.tx_task) {
        vTaskDelete(s_mgr.tx_task);
        s_mgr.tx_task = NULL;
    }
    if (s_mgr.tx_queue) {
        vQueueDelete(s_mgr.tx_queue);
        s_mgr.tx_queue = NULL;
    }

    s_mgr.initialized = false;
    return ESP_OK;
}

esp_err_t can_bus_manager_send(const can_frame_t *frame) {
    if (!s_mgr.initialized || !s_mgr.tx_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!frame) {
        return ESP_ERR_INVALID_ARG;
    }

    tx_item_t item = {0};
    item.frame = *frame;

    if (xQueueSend(s_mgr.tx_queue, &item, 0) != pdTRUE) {
        s_mgr.stats.tx_dropped_queue_full++;
        return ESP_ERR_TIMEOUT;
    }

    s_mgr.stats.tx_enqueued++;
    return ESP_OK;
}

esp_err_t can_bus_manager_register_handler(uint16_t id, uint16_t mask, can_bus_rx_handler_t cb, void *ctx) {
    if (!cb) return ESP_ERR_INVALID_ARG;

    for (size_t i = 0; i < CAN_BUS_MANAGER_MAX_HANDLERS; i++) {
        handler_entry_t *h = &s_mgr.handlers[i];
        if (!h->used) {
            h->used = true;
            h->id = id;
            h->mask = mask;
            h->cb = cb;
            h->ctx = ctx;
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

esp_err_t can_bus_manager_discovery_query_all(void) {
    can_frame_t frame = {0};
    frame.id = CAN_ID_MODULE_QUERY;
    frame.extended = false;
    frame.rtr = false;
    frame.dlc = 8;
    frame.data[0] = 0xFF;

    return can_bus_manager_send(&frame);
}

void can_bus_manager_get_stats(can_bus_manager_stats_t *out_stats) {
    if (!out_stats) return;
    *out_stats = s_mgr.stats;
}
