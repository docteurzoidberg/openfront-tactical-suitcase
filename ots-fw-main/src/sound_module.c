#include "sound_module.h"
#include "can_audio_protocol.h"
#include "can_discovery.h"
#include "protocol.h"
#include "event_dispatcher.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "SOUND_MODULE";

// Module state
typedef struct {
    bool initialized;
    bool can_driver_ready;
    bool can_ready;  // audio module online/usable (not just CAN driver)
    uint32_t sounds_played;
    uint32_t sounds_failed;
    uint16_t last_sound_index;
    uint64_t last_play_time;
    
    // Discovery state
    bool audio_module_discovered;
    uint8_t audio_module_version_major;
    uint8_t audio_module_version_minor;

    // Liveness/status tracking
    uint64_t audio_discovered_time_ms;
    uint64_t last_status_time_ms;
    uint8_t last_status_state_bits;
    uint16_t last_status_current_sound;
    uint8_t last_status_error_code;
    uint8_t last_status_volume;
    uint16_t last_status_uptime;
    uint32_t tx_timeouts;
    uint64_t last_tx_timeout_time_ms;
    uint64_t last_discovery_query_time_ms;
} sound_module_state_t;

static sound_module_state_t s_state = {0};
static uint16_t s_request_counter = 0;
static TaskHandle_t s_can_rx_task = NULL;
static TaskHandle_t s_can_tx_task = NULL;
static QueueHandle_t s_can_tx_queue = NULL;

#define SOUND_CAN_RX_TIMEOUT_MS 100
#define SOUND_CAN_TX_QUEUE_DEPTH 16

// Consider audio module offline if no status received for this long.
// Use several intervals to tolerate jitter and startup.
#define SOUND_AUDIO_OFFLINE_TIMEOUT_MS (CAN_AUDIO_STATUS_INTERVAL_MS * 3)
#define SOUND_DISCOVERY_RETRY_INTERVAL_MS 2000

typedef enum {
    SOUND_TX_KIND_PLAY = 1,
    SOUND_TX_KIND_STOP_ALL = 2,
    SOUND_TX_KIND_DISCOVERY_QUERY = 3,
} sound_tx_kind_t;

typedef struct {
    sound_tx_kind_t kind;
    uint16_t sound_index;
    uint16_t request_id;
    can_frame_t frame;
} sound_tx_item_t;

// Forward declarations
static esp_err_t sound_init(void);
static esp_err_t sound_update(void);
static bool sound_handle_event(const internal_event_t *event);
static void sound_get_status(module_status_t *status);
static esp_err_t sound_shutdown(void);

// Helper functions
static uint16_t map_event_to_sound_index(game_event_type_t event_type);
static esp_err_t parse_sound_play_data(const char *json_data, uint16_t *sound_index, 
                                       bool *interrupt, bool *high_priority);

static void can_tx_task(void *arg);
static esp_err_t enqueue_can_tx(const sound_tx_item_t *item);
static esp_err_t enqueue_discovery_query(uint64_t now_ms);

/**
 * @brief CAN RX task - receives discovery announcements and sound status
 */
static void can_rx_task(void *arg) {
    can_frame_t frame;
    ESP_LOGI(TAG, "CAN RX task started");
    
    while (1) {
        // Receive CAN frame with 100ms timeout (matches audiomodule/cantest pattern)
        esp_err_t ret = can_driver_receive(&frame, SOUND_CAN_RX_TIMEOUT_MS);
        
        if (ret == ESP_OK) {
            const uint64_t now_ms = esp_timer_get_time() / 1000;
            // Handle MODULE_ANNOUNCE (discovery)
            if (frame.id == CAN_ID_MODULE_ANNOUNCE) {
                module_info_t info;
                if (can_discovery_parse_announce(&frame, &info) == ESP_OK) {
                    if (info.module_type == MODULE_TYPE_AUDIO) {
                        s_state.audio_module_discovered = true;
                        s_state.audio_module_version_major = info.version_major;
                        s_state.audio_module_version_minor = info.version_minor;
                        s_state.audio_discovered_time_ms = now_ms;
                        // Treat as usable immediately; status will refine liveness.
                        s_state.can_ready = true;
                        s_state.tx_timeouts = 0;
                        ESP_LOGI(TAG, "Audio module v%d.%d discovered on CAN block 0x%02X",
                                 info.version_major, info.version_minor, info.can_block_base);
                    }
                }
            }
            // Handle SOUND_STATUS (0x426)
            else if (frame.id == CAN_ID_SOUND_STATUS && frame.dlc >= 8) {
                // Layout from can_audiomodule component:
                // data[0]=state_bits, [1-2]=current_sound (LE), [3]=error_code,
                // [4]=volume, [5-6]=uptime_sec (LE), [7]=reserved
                s_state.last_status_time_ms = now_ms;
                s_state.last_status_state_bits = frame.data[0];
                s_state.last_status_current_sound = frame.data[1] | (frame.data[2] << 8);
                s_state.last_status_error_code = frame.data[3];
                s_state.last_status_volume = frame.data[4];
                s_state.last_status_uptime = frame.data[5] | (frame.data[6] << 8);

                // If we are receiving status, the module is definitely online.
                s_state.can_ready = true;
                s_state.audio_module_discovered = true;
                s_state.tx_timeouts = 0;
            }
            // Handle SOUND_ACK (0x423) - future implementation
            else if (frame.id == CAN_ID_SOUND_ACK) {
                ESP_LOGI(TAG, "Received SOUND_ACK (parsing not yet implemented)");
            }
            // Handle SOUND_FINISHED (0x425) - future implementation
            else if (frame.id == CAN_ID_SOUND_FINISHED) {
                ESP_LOGI(TAG, "Received SOUND_FINISHED (parsing not yet implemented)");
            }
            
            // Yield after processing to prevent watchdog (matches cantest pattern)
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        // If timeout (no CAN traffic), just continue and check again
    }
}

static esp_err_t enqueue_can_tx(const sound_tx_item_t *item) {
    if (!item || !s_can_tx_queue) return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_can_tx_queue, item, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t enqueue_discovery_query(uint64_t now_ms) {
    if (!s_state.can_driver_ready) return ESP_ERR_INVALID_STATE;
    if (now_ms - s_state.last_discovery_query_time_ms < SOUND_DISCOVERY_RETRY_INTERVAL_MS) {
        return ESP_OK;
    }

    sound_tx_item_t item = {0};
    item.kind = SOUND_TX_KIND_DISCOVERY_QUERY;
    item.request_id = 0;
    item.sound_index = 0;

    // Build MODULE_QUERY (0x411): [FF 00 00 00 00 00 00 00]
    memset(&item.frame, 0, sizeof(item.frame));
    item.frame.id = CAN_ID_MODULE_QUERY;
    item.frame.extended = false;
    item.frame.rtr = false;
    item.frame.dlc = 8;
    item.frame.data[0] = 0xFF;

    esp_err_t ret = enqueue_can_tx(&item);
    if (ret == ESP_OK) {
        s_state.last_discovery_query_time_ms = now_ms;
    }
    return ret;
}

static void can_tx_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "CAN TX task started");

    sound_tx_item_t item;
    while (1) {
        if (xQueueReceive(s_can_tx_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        esp_err_t ret = can_driver_send(&item.frame);
        if (ret == ESP_OK) {
            // Success implies at least one node ACKed the frame.
            // (Doesn't guarantee audio module accepted payload, but status/ACK will.)
        } else if (ret == ESP_ERR_TIMEOUT) {
            const uint64_t now_ms = esp_timer_get_time() / 1000;
            s_state.tx_timeouts++;
            s_state.last_tx_timeout_time_ms = now_ms;

            // Fast-fail: if we can't get CAN ACKs, assume the audio ESP is offline.
            // Stop accepting new sound requests until status resumes.
            s_state.can_ready = false;

            // Drop any queued sound commands to avoid repeated 100ms blocks.
            xQueueReset(s_can_tx_queue);
        }

        // Yield after processing to prevent watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Module interface
static hardware_module_t s_sound_module = {
    .name = "Sound Module",
    .enabled = true,
    .init = sound_init,
    .update = sound_update,
    .handle_event = sound_handle_event,
    .get_status = sound_get_status,
    .shutdown = sound_shutdown
};

hardware_module_t* sound_module_get(void) {
    return &s_sound_module;
}

/**
 * @brief Initialize sound module
 */
static esp_err_t sound_init(void) {
    ESP_LOGI(TAG, "Initializing sound module...");
    
    // Initialize CAN driver with validated Phase 2.5/3.2 configuration
    can_config_t config = {
        .tx_gpio = 5,        // GPIO5 TX (matches Phase 2.5 ESP32-S3 config)
        .rx_gpio = 4,        // GPIO4 RX (matches Phase 2.5 ESP32-S3 config)
        .bitrate = 125000,   // 125 kbps (validated in Phase 2.5/3.2)
        .loopback = false,   // Physical CAN bus (not loopback)
        .mock_mode = false   // Auto-detect (falls back to mock if hardware missing)
    };
    esp_err_t ret = can_driver_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CAN driver: %s", esp_err_to_name(ret));
        s_state.can_ready = false;
        return ret;
    }
    
    s_state.can_driver_ready = true;
    s_state.can_ready = false;
    s_state.initialized = true;
    s_state.sounds_played = 0;
    s_state.sounds_failed = 0;
    s_state.last_sound_index = 0;
    s_state.last_play_time = 0;
    s_state.audio_module_discovered = false;
    s_state.audio_discovered_time_ms = 0;
    s_state.last_status_time_ms = 0;
    s_state.last_status_state_bits = 0;
    s_state.last_status_current_sound = 0xFFFF;
    s_state.last_status_error_code = 0;
    s_state.last_status_volume = 0;
    s_state.last_status_uptime = 0;
    s_state.tx_timeouts = 0;
    s_state.last_tx_timeout_time_ms = 0;
    s_state.last_discovery_query_time_ms = 0;
    
    // Start CAN RX task to receive discovery announcements and responses
    // Use PinnedToCore with tskNO_AFFINITY and priority 6 (matches audiomodule pattern)
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        can_rx_task, 
        "can_rx", 
        4096, 
        NULL, 
        6,  // Higher priority than serial
        &s_can_rx_task,
        tskNO_AFFINITY
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN RX task");
        return ESP_FAIL;
    }

    // Create CAN TX queue + task (non-blocking SOUND_PLAY handling)
    s_can_tx_queue = xQueueCreate(SOUND_CAN_TX_QUEUE_DEPTH, sizeof(sound_tx_item_t));
    if (!s_can_tx_queue) {
        ESP_LOGE(TAG, "Failed to create CAN TX queue");
        return ESP_FAIL;
    }

    task_ret = xTaskCreatePinnedToCore(
        can_tx_task,
        "can_tx",
        4096,
        NULL,
        5,
        &s_can_tx_task,
        tskNO_AFFINITY
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN TX task");
        return ESP_FAIL;
    }
    
    // Send module discovery query
    ESP_LOGI(TAG, "Discovering CAN modules...");
    ret = can_discovery_query_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send discovery query: %s", esp_err_to_name(ret));
    }
    
    // Wait for discovery responses
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (s_state.audio_module_discovered) {
        ESP_LOGI(TAG, "✓ Audio module v%d.%d detected",
                 s_state.audio_module_version_major,
                 s_state.audio_module_version_minor);
        s_state.can_ready = true;
    } else {
        ESP_LOGW(TAG, "✗ No audio module detected - sound features disabled");
        s_state.can_ready = false;
    }
    
    ESP_LOGI(TAG, "Sound module initialized successfully");
    ESP_LOGI(TAG, "Sound module ready: %s", s_state.can_ready ? "YES" : "NO");
    
    return ESP_OK;
}

/**
 * @brief Update sound module (periodic)
 * 
 * In real implementation, this would:
 * - Check for incoming CAN STATUS messages from audio controller
 * - Update connection/playback status
 * - Handle timeouts
 */
static esp_err_t sound_update(void) {
    if (!s_state.initialized) return ESP_FAIL;

    const uint64_t now_ms = esp_timer_get_time() / 1000;

    // Liveness: if we had status and it went stale, mark offline.
    if (s_state.audio_module_discovered) {
        if (s_state.last_status_time_ms != 0 &&
            (now_ms - s_state.last_status_time_ms) > SOUND_AUDIO_OFFLINE_TIMEOUT_MS) {
            s_state.can_ready = false;
        } else if (s_state.last_status_time_ms == 0 && s_state.audio_discovered_time_ms != 0 &&
                   (now_ms - s_state.audio_discovered_time_ms) > SOUND_AUDIO_OFFLINE_TIMEOUT_MS) {
            // Discovered but never saw status (unexpected) -> treat offline.
            s_state.can_ready = false;
        }
    } else {
        s_state.can_ready = false;
    }

    // If offline, periodically broadcast a discovery query (via TX task)
    if (!s_state.can_ready) {
        (void)enqueue_discovery_query(now_ms);
    }

    return ESP_OK;
}

/**
 * @brief Handle incoming events
 */
static bool sound_handle_event(const internal_event_t *event) {
    if (!s_state.initialized || !event) return false;
    
    // Only handle SOUND_PLAY events
    if (event->type != GAME_EVENT_SOUND_PLAY) {
        return false;
    }
    
    ESP_LOGI(TAG, "Received SOUND_PLAY event");

    // Non-blocking behavior: if audio module is offline, drop immediately.
    if (!s_state.can_ready) {
        s_state.sounds_failed++;
        ESP_LOGW(TAG, "Skipping sound (audio module offline)");
        return true;
    }
    
    // Parse event data
    uint16_t sound_index = 0;
    bool interrupt = false;
    bool high_priority = false;
    
    esp_err_t ret = parse_sound_play_data(event->data, 
                                          &sound_index, &interrupt, &high_priority);
    
    if (ret != ESP_OK) {
        // If no explicit sound index, try to map from event message or type
        sound_index = map_event_to_sound_index(event->type);
        if (sound_index == 0) {
            ESP_LOGW(TAG, "Failed to parse sound data and no fallback mapping");
            s_state.sounds_failed++;
            return false;
        }
        ESP_LOGI(TAG, "Using fallback sound index: %u", sound_index);
    }
    
    // Play the sound
    ret = sound_module_play(sound_index, interrupt, high_priority);
    if (ret == ESP_OK) {
        s_state.sounds_played++;
        s_state.last_sound_index = sound_index;
        s_state.last_play_time = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, "Sound played successfully (total: %lu)", (unsigned long)s_state.sounds_played);
    } else {
        s_state.sounds_failed++;
        ESP_LOGE(TAG, "Failed to play sound (total failed: %lu)", (unsigned long)s_state.sounds_failed);
    }
    
    return true;
}

/**
 * @brief Get module status
 */
static void sound_get_status(module_status_t *status) {
    if (!status) return;
    
    status->initialized = s_state.initialized;
    status->operational = s_state.can_ready;
    status->error_count = s_state.sounds_failed;
    
    if (!s_state.can_driver_ready) {
        snprintf(status->last_error, sizeof(status->last_error), "CAN driver not ready");
    } else if (!s_state.can_ready) {
        // Prefer differentiating "never discovered" vs "went offline"
        if (!s_state.audio_module_discovered) {
            snprintf(status->last_error, sizeof(status->last_error), "Audio module not discovered");
        } else {
            snprintf(status->last_error, sizeof(status->last_error), "Audio module offline");
        }
    } else if (s_state.sounds_failed > 0) {
        snprintf(status->last_error, sizeof(status->last_error), 
                "%lu sounds failed to play", (unsigned long)s_state.sounds_failed);
    } else {
        snprintf(status->last_error, sizeof(status->last_error), "OK");
    }
}

/**
 * @brief Shutdown sound module
 */
static esp_err_t sound_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down sound module...");
    
    // Stop all sounds before shutdown (best-effort)
    (void)sound_module_stop(0, true);
    
    // Stop CAN RX task
    if (s_can_rx_task) {
        vTaskDelete(s_can_rx_task);
        s_can_rx_task = NULL;
    }

    if (s_can_tx_task) {
        vTaskDelete(s_can_tx_task);
        s_can_tx_task = NULL;
    }

    if (s_can_tx_queue) {
        vQueueDelete(s_can_tx_queue);
        s_can_tx_queue = NULL;
    }
    
    s_state.initialized = false;
    s_state.can_ready = false;
    s_state.can_driver_ready = false;
    
    ESP_LOGI(TAG, "Sound module shutdown complete");
    return ESP_OK;
}

/**
 * @brief Play a sound via CAN bus
 */
esp_err_t sound_module_play(uint16_t sound_index, bool interrupt, bool high_priority) {
    if (!s_state.initialized) {
        ESP_LOGE(TAG, "Cannot play sound: module not initialized");
        return ESP_FAIL;
    }
    
    if (!s_state.can_ready) {
        ESP_LOGW(TAG, "Cannot play sound: audio module offline");
        return ESP_FAIL;
    }
    
    // Build CAN frame
    uint8_t flags = 0;
    if (interrupt) flags |= CAN_AUDIO_FLAG_INTERRUPT;
    if (high_priority) flags |= CAN_AUDIO_FLAG_HIGH_PRIORITY;

    const uint16_t request_id = can_audio_allocate_request_id(&s_request_counter);

    sound_tx_item_t item = {0};
    item.kind = SOUND_TX_KIND_PLAY;
    item.sound_index = sound_index;
    item.request_id = request_id;
    can_audio_build_play_sound(sound_index, flags, CAN_AUDIO_VOLUME_USE_POT, request_id, &item.frame);

    esp_err_t ret = enqueue_can_tx(&item);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue PLAY_SOUND (queue full/offline)");
    } else {
        ESP_LOGI(TAG, "Queued PLAY_SOUND: index=%u interrupt=%d priority=%d reqID=%u",
                 sound_index, interrupt, high_priority, request_id);
    }

    return ret;
}

/**
 * @brief Stop currently playing sound
 */
esp_err_t sound_module_stop(uint16_t sound_index, bool stop_all) {
    if (!s_state.initialized) {
        return ESP_FAIL;
    }
    
    if (!s_state.can_ready) {
        return ESP_FAIL;
    }

    if (!stop_all) {
        (void)sound_index;
        ESP_LOGW(TAG, "STOP_SOUND by sound_index is not supported (protocol uses queue_id)");
        return ESP_ERR_NOT_SUPPORTED;
    }

    sound_tx_item_t item = {0};
    item.kind = SOUND_TX_KIND_STOP_ALL;
    item.sound_index = 0;
    item.request_id = 0;
    can_audio_build_stop_all(&item.frame);

    esp_err_t ret = enqueue_can_tx(&item);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue STOP_ALL");
        return ret;
    }

    ESP_LOGI(TAG, "Queued STOP_ALL");
    return ESP_OK;
}

/**
 * @brief Map soundId string to soundIndex number
 * 
 * Canonical mapping from /prompts/WEBSOCKET_MESSAGE_SPEC.md:
 * - game_start → 1
 * - game_player_death → 2
 * - game_victory → 3
 * - game_defeat → 4
 * - audio-ready → 100
 */
static uint16_t map_sound_id_to_index(const char *sound_id) {
    if (!sound_id) return 0;
    
    if (strcmp(sound_id, "game_start") == 0) return 1;
    if (strcmp(sound_id, "game_player_death") == 0) return 2;
    if (strcmp(sound_id, "game_victory") == 0) return 3;
    if (strcmp(sound_id, "game_defeat") == 0) return 4;
    if (strcmp(sound_id, "audio-ready") == 0) return 100;
    
    ESP_LOGW(TAG, "Unknown soundId: %s", sound_id);
    return 0;
}

/**
 * @brief Parse SOUND_PLAY event data JSON
 * 
 * Expected format:
 * {
 *   "soundId": "game_start",
 *   "soundIndex": 1,
 *   "interrupt": true,
 *   "priority": "high"
 * }
 * 
 * Prefers soundIndex if present, falls back to mapping soundId string.
 */
static esp_err_t parse_sound_play_data(const char *json_data, uint16_t *sound_index,
                                      bool *interrupt, bool *high_priority) {
    if (!json_data || strlen(json_data) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse sound event JSON");
        return ESP_FAIL;
    }
    
    bool found_index = false;
    
    // Try to get soundIndex directly (numeric)
    cJSON *index_item = cJSON_GetObjectItem(root, "soundIndex");
    if (index_item && cJSON_IsNumber(index_item)) {
        *sound_index = (uint16_t)index_item->valueint;
        found_index = true;
    } else {
        // Fall back to mapping soundId string
        cJSON *id_item = cJSON_GetObjectItem(root, "soundId");
        if (id_item && cJSON_IsString(id_item)) {
            *sound_index = map_sound_id_to_index(id_item->valuestring);
            if (*sound_index > 0) {
                found_index = true;
            }
        }
    }
    
    if (!found_index) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get optional flags
    cJSON *interrupt_item = cJSON_GetObjectItem(root, "interrupt");
    if (interrupt_item && cJSON_IsBool(interrupt_item)) {
        *interrupt = cJSON_IsTrue(interrupt_item);
    }
    
    cJSON *priority_item = cJSON_GetObjectItem(root, "priority");
    if (priority_item && cJSON_IsString(priority_item)) {
        const char *priority_str = priority_item->valuestring;
        *high_priority = (strcmp(priority_str, "high") == 0);
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief Map game event type to sound index (fallback)
 * 
 * This is a simple fallback mapping when soundIndex is not provided in event data.
 * Real implementation should always include soundIndex in event data.
 */
static uint16_t map_event_to_sound_index(game_event_type_t event_type) {
    switch (event_type) {
        case GAME_EVENT_GAME_START:
            return SOUND_INDEX_GAME_START;
        case GAME_EVENT_ALERT_ATOM:
            return SOUND_INDEX_ALERT_ATOM;
        case GAME_EVENT_ALERT_HYDRO:
            return SOUND_INDEX_ALERT_HYDRO;
        case GAME_EVENT_ALERT_MIRV:
            return SOUND_INDEX_ALERT_MIRV;
        case GAME_EVENT_ALERT_LAND:
            return SOUND_INDEX_ALERT_LAND;
        case GAME_EVENT_ALERT_NAVAL:
            return SOUND_INDEX_ALERT_NAVAL;
        case GAME_EVENT_NUKE_LAUNCHED:
            return SOUND_INDEX_NUKE_LAUNCH;
        case GAME_EVENT_NUKE_EXPLODED:
            return SOUND_INDEX_NUKE_EXPLODE;
        case GAME_EVENT_NUKE_INTERCEPTED:
            return SOUND_INDEX_NUKE_INTERCEPT;
        case GAME_EVENT_HARDWARE_TEST:
            return SOUND_INDEX_TEST_BEEP;
        default:
            return 0; // Unknown
    }
}
