#include "sound_module.h"
#include "can_protocol.h"
#include "can_discovery.h"
#include "protocol.h"
#include "event_dispatcher.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "SOUND_MODULE";

// Module state
typedef struct {
    bool initialized;
    bool can_ready;
    uint32_t sounds_played;
    uint32_t sounds_failed;
    uint16_t last_sound_index;
    uint64_t last_play_time;
    
    // Discovery state
    bool audio_module_discovered;
    uint8_t audio_module_version_major;
    uint8_t audio_module_version_minor;
} sound_module_state_t;

static sound_module_state_t s_state = {0};
static uint16_t s_request_counter = 0;
static TaskHandle_t s_can_rx_task = NULL;

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

/**
 * @brief CAN RX task - receives discovery announcements and sound status
 */
static void can_rx_task(void *arg) {
    can_frame_t frame;
    ESP_LOGI(TAG, "CAN RX task started");
    
    while (1) {
        // Receive CAN frame with 100ms timeout (matches audiomodule/cantest pattern)
        esp_err_t ret = can_driver_receive(&frame, pdMS_TO_TICKS(100));
        
        if (ret == ESP_OK) {
            // Handle MODULE_ANNOUNCE (discovery)
            if (frame.id == CAN_ID_MODULE_ANNOUNCE) {
                module_info_t info;
                if (can_discovery_parse_announce(&frame, &info) == ESP_OK) {
                    if (info.module_type == MODULE_TYPE_AUDIO) {
                        s_state.audio_module_discovered = true;
                        s_state.audio_module_version_major = info.version_major;
                        s_state.audio_module_version_minor = info.version_minor;
                        ESP_LOGI(TAG, "Audio module v%d.%d discovered on CAN block 0x%02X",
                                 info.version_major, info.version_minor, info.can_block_base);
                    }
                }
            }
            // Handle SOUND_ACK (0x423) - future implementation
            else if (frame.id == 0x423) {
                ESP_LOGI(TAG, "Received SOUND_ACK (parsing not yet implemented)");
            }
            // Handle SOUND_FINISHED (0x425) - future implementation
            else if (frame.id == 0x425) {
                ESP_LOGI(TAG, "Received SOUND_FINISHED (parsing not yet implemented)");
            }
            
            // Yield after processing to prevent watchdog (matches cantest pattern)
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        // If timeout (no CAN traffic), just continue and check again
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
    
    s_state.can_ready = true;
    s_state.initialized = true;
    s_state.sounds_played = 0;
    s_state.sounds_failed = 0;
    s_state.last_sound_index = 0;
    s_state.last_play_time = 0;
    s_state.audio_module_discovered = false;
    
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
    } else {
        ESP_LOGW(TAG, "✗ No audio module detected - sound features disabled");
        s_state.can_ready = false;  // Disable sound features
    }
    
    ESP_LOGI(TAG, "Sound module initialized successfully");
    ESP_LOGI(TAG, "Mode: MOCK (CAN messages logged only)");
    
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
    
    // Mock: No periodic processing needed
    // Real implementation would poll CAN RX queue for STATUS/ACK messages
    
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
    
    if (!s_state.can_ready) {
        snprintf(status->last_error, sizeof(status->last_error), "CAN driver not ready");
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
    
    // Stop all sounds before shutdown
    sound_module_stop(CAN_SOUND_INDEX_ANY, true);
    
    // Stop CAN RX task
    if (s_can_rx_task) {
        vTaskDelete(s_can_rx_task);
        s_can_rx_task = NULL;
    }
    
    s_state.initialized = false;
    s_state.can_ready = false;
    
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
        ESP_LOGW(TAG, "Cannot play sound: CAN driver not ready");
        return ESP_FAIL;
    }
    
    // Build CAN frame
    can_frame_t frame;
    uint8_t flags = 0;
    if (interrupt) flags |= CAN_FLAG_INTERRUPT;
    if (high_priority) flags |= CAN_FLAG_HIGH_PRIORITY;
    
    uint16_t request_id = s_request_counter++;
    
    can_build_play_sound(sound_index, flags, CAN_VOLUME_USE_POT, request_id, &frame);
    
    // Send CAN message
    esp_err_t ret = can_driver_send(&frame);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send PLAY_SOUND CAN message");
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent PLAY_SOUND: index=%u interrupt=%d priority=%d reqID=%u",
             sound_index, interrupt, high_priority, request_id);
    
    return ESP_OK;
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
    
    // Build CAN frame
    can_frame_t frame;
    uint8_t flags = stop_all ? CAN_FLAG_STOP_ALL : 0;
    uint16_t request_id = s_request_counter++;
    
    can_build_stop_sound(sound_index, flags, request_id, &frame);
    
    // Send CAN message
    esp_err_t ret = can_driver_send(&frame);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send STOP_SOUND CAN message");
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent STOP_SOUND: index=%u stop_all=%d reqID=%u",
             sound_index, stop_all, request_id);
    
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
 */
static uint16_t map_sound_id_to_index(const char *sound_id) {
    if (!sound_id) return 0;
    
    if (strcmp(sound_id, "game_start") == 0) return 1;
    if (strcmp(sound_id, "game_player_death") == 0) return 2;
    if (strcmp(sound_id, "game_victory") == 0) return 3;
    if (strcmp(sound_id, "game_defeat") == 0) return 4;
    
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
