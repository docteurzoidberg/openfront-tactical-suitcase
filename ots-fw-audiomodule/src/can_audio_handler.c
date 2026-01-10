/**
 * @file can_audio_handler.c
 * @brief Audio-specific CAN Bus Message Handler Implementation
 */

#include "can_audio_handler.h"
#include "can_audio_protocol.h"
#include "can_discovery.h"
#include "can_bus_manager.h"
#include "sound_config.h"
#include "audio_mixer.h"
#include "audio_player.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "CAN_AUDIO";

// Module state
static bool *g_sd_mounted = NULL;
static uint16_t g_last_sound_index = 0;
static uint8_t g_last_error = CAN_AUDIO_ERR_OK;

// Queue ID allocator state (using shared utility function)
static uint8_t g_next_queue_id = 1;

static TaskHandle_t s_status_task = NULL;
static bool s_handlers_registered = false;

static void send_module_announce(void) {
    can_frame_t announce = {0};
    announce.id = CAN_ID_MODULE_ANNOUNCE;
    announce.extended = false;
    announce.rtr = false;
    announce.dlc = 8;

    announce.data[0] = MODULE_TYPE_AUDIO;
    announce.data[1] = 1; // v1.0
    announce.data[2] = 0;
    announce.data[3] = MODULE_CAP_STATUS;
    announce.data[4] = 0x42; // 0x420-0x42F
    announce.data[5] = 0;    // node_id
    announce.data[6] = 0;
    announce.data[7] = 0;

    (void)can_bus_manager_send(&announce);
}

static void on_can_module_query(const can_frame_t *frame, void *ctx) {
    (void)ctx;
    if (!frame || frame->dlc < 1) {
        return;
    }

    // MODULE_QUERY expects magic byte 0xFF to enumerate all modules.
    if (frame->data[0] != 0xFF) {
        return;
    }

    ESP_LOGI(TAG, "Received MODULE_QUERY, announcing...");
    send_module_announce();
}

static void on_can_play_sound(const can_frame_t *frame, void *ctx) {
    (void)ctx;
    if (!frame) {
        return;
    }

    uint16_t sound_index;
    uint8_t flags, volume;
    uint16_t request_id;

    if (!can_audio_parse_play_sound(frame, &sound_index, &flags, &volume, &request_id)) {
        return;
    }

    ESP_LOGI(TAG, "PLAY_SOUND: index=%d flags=0x%02X vol=%d req_id=%d",
             sound_index, flags, volume, request_id);

    int active_count = audio_mixer_get_active_count();
    bool should_play = (active_count < MAX_AUDIO_SOURCES) || (flags & CAN_AUDIO_FLAG_INTERRUPT);

    if (should_play) {
        bool loop = (flags & CAN_AUDIO_FLAG_LOOP) != 0;
        bool interrupt = (flags & CAN_AUDIO_FLAG_INTERRUPT) != 0;

        g_last_sound_index = sound_index;
        audio_source_handle_t handle;
        esp_err_t ret = audio_player_play_sound(sound_index, volume, loop, interrupt, &handle);

        uint8_t queue_id = 0;
        uint8_t error_code = CAN_AUDIO_ERR_OK;

        if (ret == ESP_OK) {
            queue_id = can_audio_allocate_queue_id(&g_next_queue_id);
            audio_mixer_set_queue_id(handle, queue_id, sound_index);
            ESP_LOGI(TAG, "Assigned queue_id=%d to source handle=%d", queue_id, handle);
            error_code = CAN_AUDIO_ERR_OK;
        } else {
            if (ret == ESP_ERR_NOT_FOUND) {
                error_code = CAN_AUDIO_ERR_FILE_NOT_FOUND;
                ESP_LOGE(TAG, "Sound %d: File not found or SD card not mounted", sound_index);
            } else if (ret == ESP_ERR_INVALID_ARG) {
                error_code = CAN_AUDIO_ERR_SD_ERROR;
                ESP_LOGE(TAG, "Sound %d: Invalid WAV file format", sound_index);
            } else if (ret == ESP_FAIL) {
                error_code = CAN_AUDIO_ERR_MIXER_FULL;
                ESP_LOGE(TAG, "Sound %d: Mixer error (possibly full)", sound_index);
            } else {
                error_code = CAN_AUDIO_ERR_SD_ERROR;
                ESP_LOGE(TAG, "Sound %d: Playback error: %s", sound_index, esp_err_to_name(ret));
            }
        }

        g_last_error = error_code;

        can_frame_t ack_frame;
        can_audio_build_sound_ack(
            ret == ESP_OK ? 1 : 0,
            sound_index,
            queue_id,
            error_code,
            request_id,
            &ack_frame
        );
        (void)can_bus_manager_send(&ack_frame);
        ESP_LOGI(TAG, "Sent ACK: ok=%d queue_id=%d error=0x%02X active=%d",
                 ret == ESP_OK, queue_id, error_code, audio_mixer_get_active_count());
    } else {
        can_frame_t ack_frame;
        can_audio_build_sound_ack(
            0,
            sound_index,
            0,
            CAN_AUDIO_ERR_MIXER_FULL,
            request_id,
            &ack_frame
        );
        (void)can_bus_manager_send(&ack_frame);
        ESP_LOGW(TAG, "Sent NACK: mixer full (max sources=%d)", MAX_AUDIO_SOURCES);
    }
}

static void on_can_stop_sound(const can_frame_t *frame, void *ctx) {
    (void)ctx;
    if (!frame) {
        return;
    }

    uint8_t queue_id;
    uint8_t flags;
    uint16_t request_id;

    if (!can_audio_parse_stop_sound(frame, &queue_id, &flags, &request_id)) {
        return;
    }

    ESP_LOGI(TAG, "STOP_SOUND: queue_id=%d flags=0x%02X", queue_id, flags);

    esp_err_t ret = audio_mixer_stop_by_queue_id(queue_id);

    can_frame_t ack_frame;
    can_audio_build_sound_ack(
        ret == ESP_OK ? 1 : 0,
        0,
        queue_id,
        ret == ESP_OK ? CAN_AUDIO_ERR_OK : CAN_AUDIO_ERR_INVALID_QUEUE_ID,
        request_id,
        &ack_frame
    );
    (void)can_bus_manager_send(&ack_frame);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Stopped queue_id=%d", queue_id);
    } else {
        ESP_LOGW(TAG, "Failed to stop queue_id=%d (not found)", queue_id);
    }
}

static void on_can_stop_all(const can_frame_t *frame, void *ctx) {
    (void)frame;
    (void)ctx;

    ESP_LOGI(TAG, "STOP_ALL received");
    audio_mixer_stop_all();
    ESP_LOGI(TAG, "Stopped all sources");
}

static void status_task(void *arg) {
    (void)arg;

    ESP_LOGI(TAG, "CAN STATUS task started");

    while (1) {
        const uint32_t uptime_sec = esp_log_timestamp() / 1000;
        const int active_sources = audio_mixer_get_active_count();

        uint8_t state_bits = 0;
        if (g_sd_mounted && *g_sd_mounted) state_bits |= CAN_AUDIO_STATUS_SD_MOUNTED;
        if (active_sources > 0) state_bits |= CAN_AUDIO_STATUS_PLAYING;
        if (g_last_error == 0) state_bits |= CAN_AUDIO_STATUS_READY;
        if (g_last_error != 0) state_bits |= CAN_AUDIO_STATUS_ERROR;

        can_frame_t status_frame;
        can_audio_build_sound_status(
            state_bits,
            active_sources > 0 ? g_last_sound_index : 0xFFFF,
            g_last_error,
            CAN_AUDIO_VOLUME_USE_POT,
            uptime_sec,
            &status_frame
        );
        (void)can_bus_manager_send(&status_frame);

        ESP_LOGI(TAG, "STATUS: bits=0x%02X active=%d uptime=%lus",
                 state_bits, active_sources, (unsigned long)uptime_sec);

        vTaskDelay(pdMS_TO_TICKS(CAN_AUDIO_STATUS_INTERVAL_MS));
    }
}

esp_err_t can_audio_handler_init(bool *sd_mounted)
{
    if (sd_mounted == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_sd_mounted = sd_mounted;
    g_last_sound_index = 0;
    g_last_error = CAN_AUDIO_ERR_OK;
    
    return ESP_OK;
}

esp_err_t can_audio_handler_start_task(void)
{
    if (!s_handlers_registered) {
        (void)can_bus_manager_register_handler(CAN_ID_MODULE_QUERY, 0x7FF, on_can_module_query, NULL);
        (void)can_bus_manager_register_handler(CAN_ID_PLAY_SOUND, 0x7FF, on_can_play_sound, NULL);
        (void)can_bus_manager_register_handler(CAN_ID_STOP_SOUND, 0x7FF, on_can_stop_sound, NULL);
        (void)can_bus_manager_register_handler(CAN_ID_STOP_ALL, 0x7FF, on_can_stop_all, NULL);
        s_handlers_registered = true;
    }

    if (!s_status_task) {
        BaseType_t ret = xTaskCreatePinnedToCore(
            status_task,
            "can_status",
            4096,
            NULL,
            5,
            &s_status_task,
            tskNO_AFFINITY
        );
        if (ret != pdPASS) {
            s_status_task = NULL;
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

void can_audio_handler_sound_finished(uint8_t queue_id, uint16_t sound_index, uint8_t reason)
{
    if (!can_audio_queue_id_is_valid(queue_id)) {
        return;  // Invalid queue ID
    }
    
    ESP_LOGI(TAG, "Sound finished: queue_id=%d index=%d reason=%d", queue_id, sound_index, reason);
    
    // Build and send SOUND_FINISHED message
    can_frame_t finished_frame;
    can_audio_build_sound_finished(queue_id, sound_index, reason, &finished_frame);
    (void)can_bus_manager_send(&finished_frame);
}
