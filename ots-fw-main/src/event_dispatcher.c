#include "event_dispatcher.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "EVENT_DISP";

#define EVENT_QUEUE_SIZE 32
#define EVENT_TASK_STACK_SIZE 4096
#define EVENT_TASK_PRIORITY 5
#define MAX_HANDLERS_PER_TYPE 8
#define MAX_EVENT_TYPES 32

typedef struct {
    game_event_type_t event_type;
    event_handler_t handlers[MAX_HANDLERS_PER_TYPE];
    uint8_t handler_count;
} event_handler_list_t;

static QueueHandle_t event_queue = NULL;
static TaskHandle_t event_task_handle = NULL;
static bool is_running = false;

// Handler registry
static event_handler_list_t handler_registry[MAX_EVENT_TYPES];
static uint8_t registry_count = 0;

// Wildcard handlers (for all events)
static event_handler_t wildcard_handlers[MAX_HANDLERS_PER_TYPE];
static uint8_t wildcard_count = 0;

// Forward declarations
static void event_dispatcher_task(void *pvParameters);
static void dispatch_event_to_handlers(const internal_event_t *event);

esp_err_t event_dispatcher_init(void) {
    ESP_LOGI(TAG, "Initializing event dispatcher...");
    
    if (event_queue != NULL) {
        ESP_LOGW(TAG, "Event dispatcher already initialized");
        return ESP_OK;
    }
    
    // Create event queue
    event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(internal_event_t));
    if (event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_FAIL;
    }
    
    // Initialize registry
    memset(handler_registry, 0, sizeof(handler_registry));
    memset(wildcard_handlers, 0, sizeof(wildcard_handlers));
    registry_count = 0;
    wildcard_count = 0;
    
    // Create dispatcher task
    BaseType_t result = xTaskCreate(
        event_dispatcher_task,
        "evt_disp",
        EVENT_TASK_STACK_SIZE,
        NULL,
        EVENT_TASK_PRIORITY,
        &event_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event dispatcher task");
        vQueueDelete(event_queue);
        event_queue = NULL;
        return ESP_FAIL;
    }
    
    is_running = true;
    ESP_LOGI(TAG, "Event dispatcher initialized");
    return ESP_OK;
}

esp_err_t event_dispatcher_register(game_event_type_t event_type, event_handler_t handler) {
    if (!handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Handle wildcard registration (all events)
    if (event_type == GAME_EVENT_INVALID) {
        if (wildcard_count >= MAX_HANDLERS_PER_TYPE) {
            ESP_LOGE(TAG, "Too many wildcard handlers");
            return ESP_ERR_NO_MEM;
        }
        wildcard_handlers[wildcard_count++] = handler;
        ESP_LOGI(TAG, "Registered wildcard handler");
        return ESP_OK;
    }
    
    // Find existing handler list or create new one
    event_handler_list_t *list = NULL;
    for (int i = 0; i < registry_count; i++) {
        if (handler_registry[i].event_type == event_type) {
            list = &handler_registry[i];
            break;
        }
    }
    
    // Create new handler list if needed
    if (!list) {
        if (registry_count >= MAX_EVENT_TYPES) {
            ESP_LOGE(TAG, "Handler registry full");
            return ESP_ERR_NO_MEM;
        }
        list = &handler_registry[registry_count++];
        list->event_type = event_type;
        list->handler_count = 0;
    }
    
    // Add handler to list
    if (list->handler_count >= MAX_HANDLERS_PER_TYPE) {
        ESP_LOGE(TAG, "Too many handlers for event type %d", event_type);
        return ESP_ERR_NO_MEM;
    }
    
    list->handlers[list->handler_count++] = handler;
    ESP_LOGD(TAG, "Registered handler for event type %d", event_type);
    return ESP_OK;
}

esp_err_t event_dispatcher_unregister(game_event_type_t event_type, event_handler_t handler) {
    if (!handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Handle wildcard unregistration
    if (event_type == GAME_EVENT_INVALID) {
        for (int i = 0; i < wildcard_count; i++) {
            if (wildcard_handlers[i] == handler) {
                // Shift remaining handlers
                for (int j = i; j < wildcard_count - 1; j++) {
                    wildcard_handlers[j] = wildcard_handlers[j + 1];
                }
                wildcard_count--;
                return ESP_OK;
            }
        }
        return ESP_ERR_NOT_FOUND;
    }
    
    // Find handler list
    for (int i = 0; i < registry_count; i++) {
        if (handler_registry[i].event_type == event_type) {
            event_handler_list_t *list = &handler_registry[i];
            
            // Find and remove handler
            for (int j = 0; j < list->handler_count; j++) {
                if (list->handlers[j] == handler) {
                    // Shift remaining handlers
                    for (int k = j; k < list->handler_count - 1; k++) {
                        list->handlers[k] = list->handlers[k + 1];
                    }
                    list->handler_count--;
                    return ESP_OK;
                }
            }
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t event_dispatcher_post(const internal_event_t *event) {
    if (!event || !event_queue) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xQueueSend(event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event type %d", event->type);
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t event_dispatcher_post_simple(game_event_type_t type, event_source_t source) {
    internal_event_t event = {
        .type = type,
        .source = source,
        .timestamp = esp_timer_get_time() / 1000,
        .data = {0},
        .message = {0}
    };
    
    return event_dispatcher_post(&event);
}

esp_err_t event_dispatcher_post_game_event(const game_event_t *game_event, event_source_t source) {
    if (!game_event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    internal_event_t event = {
        .type = game_event->type,
        .source = source,
        .timestamp = game_event->timestamp
    };
    
    strncpy(event.message, game_event->message, sizeof(event.message) - 1);
    strncpy(event.data, game_event->data, sizeof(event.data) - 1);
    
    return event_dispatcher_post(&event);
}

void* event_dispatcher_get_queue(void) {
    return event_queue;
}

bool event_dispatcher_is_running(void) {
    return is_running;
}

static void dispatch_event_to_handlers(const internal_event_t *event) {
    bool handled = false;
    
    // Call wildcard handlers first
    for (int i = 0; i < wildcard_count; i++) {
        if (wildcard_handlers[i](event)) {
            handled = true;
        }
    }
    
    // Call specific handlers
    for (int i = 0; i < registry_count; i++) {
        if (handler_registry[i].event_type == event->type) {
            event_handler_list_t *list = &handler_registry[i];
            for (int j = 0; j < list->handler_count; j++) {
                if (list->handlers[j](event)) {
                    handled = true;
                }
            }
            break;
        }
    }
    
    if (!handled) {
        ESP_LOGD(TAG, "Event type %d not handled by any registered handlers", event->type);
    }
}

static void event_dispatcher_task(void *pvParameters) {
    ESP_LOGI(TAG, "Event dispatcher task started");
    
    internal_event_t event;
    
    while (is_running) {
        if (xQueueReceive(event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGD(TAG, "Dispatching event: type=%d, source=%d", event.type, event.source);
            dispatch_event_to_handlers(&event);
        }
    }
    
    ESP_LOGI(TAG, "Event dispatcher task ended");
    vTaskDelete(NULL);
}
