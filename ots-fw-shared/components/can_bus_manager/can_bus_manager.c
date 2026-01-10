#include "can_bus_manager.h"

#include "can_discovery.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
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

    SemaphoreHandle_t lock;

    handler_entry_t handlers[CAN_BUS_MANAGER_MAX_HANDLERS];

    can_bus_module_info_t modules[CAN_BUS_MANAGER_MAX_MODULES];

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

static void registry_update_from_announce(const can_frame_t *frame) {
    if (!frame) return;
    if ((uint16_t)frame->id != CAN_ID_MODULE_ANNOUNCE) return;

    can_frame_t tmp = *frame;
    module_info_t info;
    if (can_discovery_parse_announce(&tmp, &info) != ESP_OK) {
        return;
    }

    const uint64_t now_ms = uptime_ms();

    if (s_mgr.lock) {
        (void)xSemaphoreTake(s_mgr.lock, portMAX_DELAY);
    }

    // Update existing entry
    for (size_t i = 0; i < CAN_BUS_MANAGER_MAX_MODULES; i++) {
        can_bus_module_info_t *m = &s_mgr.modules[i];
        if (!m->valid) continue;
        if (m->module_type == info.module_type && m->node_id == info.node_id) {
            m->version_major = info.version_major;
            m->version_minor = info.version_minor;
            m->capabilities = info.capabilities;
            m->can_block_base = info.can_block_base;
            m->last_seen_ms = now_ms;
            if (s_mgr.lock) {
                (void)xSemaphoreGive(s_mgr.lock);
            }
            return;
        }
    }

    // Insert into first free slot
    for (size_t i = 0; i < CAN_BUS_MANAGER_MAX_MODULES; i++) {
        can_bus_module_info_t *m = &s_mgr.modules[i];
        if (m->valid) continue;
        m->valid = true;
        m->module_type = info.module_type;
        m->version_major = info.version_major;
        m->version_minor = info.version_minor;
        m->capabilities = info.capabilities;
        m->can_block_base = info.can_block_base;
        m->node_id = info.node_id;
        m->last_seen_ms = now_ms;
        break;
    }

    if (s_mgr.lock) {
        (void)xSemaphoreGive(s_mgr.lock);
    }
}

static void rx_task(void *arg) {
    (void)arg;

    can_frame_t frame;
    ESP_LOGI(TAG, "RX task started");

    while (1) {
        esp_err_t ret = can_driver_receive(&frame, CAN_BUS_MANAGER_RX_TIMEOUT_MS);
        if (ret == ESP_OK) {
            s_mgr.stats.rx_received++;

            // Opportunistically update discovery registry
            if ((uint16_t)frame.id == CAN_ID_MODULE_ANNOUNCE) {
                registry_update_from_announce(&frame);
            }

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

    s_mgr.lock = xSemaphoreCreateMutex();
    if (!s_mgr.lock) {
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
    if (s_mgr.lock) {
        vSemaphoreDelete(s_mgr.lock);
        s_mgr.lock = NULL;
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

esp_err_t can_bus_manager_get_module(uint8_t module_type, uint8_t node_id, can_bus_module_info_t *out_info) {
    if (!out_info) return ESP_ERR_INVALID_ARG;

    if (s_mgr.lock) {
        (void)xSemaphoreTake(s_mgr.lock, portMAX_DELAY);
    }

    for (size_t i = 0; i < CAN_BUS_MANAGER_MAX_MODULES; i++) {
        const can_bus_module_info_t *m = &s_mgr.modules[i];
        if (!m->valid) continue;
        if (m->module_type == module_type && m->node_id == node_id) {
            *out_info = *m;
            if (s_mgr.lock) {
                (void)xSemaphoreGive(s_mgr.lock);
            }
            return ESP_OK;
        }
    }

    if (s_mgr.lock) {
        (void)xSemaphoreGive(s_mgr.lock);
    }
    memset(out_info, 0, sizeof(*out_info));
    return ESP_ERR_NOT_FOUND;
}

bool can_bus_manager_is_module_present(uint8_t module_type, uint8_t node_id, uint64_t max_age_ms) {
    can_bus_module_info_t info;
    if (can_bus_manager_get_module(module_type, node_id, &info) != ESP_OK) {
        return false;
    }
    if (max_age_ms == 0) {
        return true;
    }
    const uint64_t now_ms = uptime_ms();
    return (now_ms >= info.last_seen_ms) && ((now_ms - info.last_seen_ms) <= max_age_ms);
}

size_t can_bus_manager_list_modules(can_bus_module_info_t *out_items, size_t max_items) {
    if (!out_items || max_items == 0) return 0;

    size_t count = 0;

    if (s_mgr.lock) {
        (void)xSemaphoreTake(s_mgr.lock, portMAX_DELAY);
    }

    for (size_t i = 0; i < CAN_BUS_MANAGER_MAX_MODULES && count < max_items; i++) {
        const can_bus_module_info_t *m = &s_mgr.modules[i];
        if (!m->valid) continue;
        out_items[count++] = *m;
    }

    if (s_mgr.lock) {
        (void)xSemaphoreGive(s_mgr.lock);
    }

    return count;
}

void can_bus_manager_get_stats(can_bus_manager_stats_t *out_stats) {
    if (!out_stats) return;
    *out_stats = s_mgr.stats;
}
