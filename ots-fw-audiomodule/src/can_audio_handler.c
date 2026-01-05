/**
 * @file can_audio_handler.c
 * @brief Audio-specific CAN Bus Message Handler Implementation
 */

#include "can_audio_handler.h"
#include "can_driver.h"
#include "can_audio_protocol.h"
#include "can_discovery.h"
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

/**
 * @brief CAN RX task - receives and processes CAN messages
 */
static void can_rx_task(void *arg)
{
    can_frame_t frame;
    uint32_t uptime_sec = 0;
    
    ESP_LOGI(TAG, "CAN RX task started");
    
    while (1) {
        // Try to receive CAN frame (100ms timeout)
        if (can_driver_receive(&frame, pdMS_TO_TICKS(100)) == ESP_OK) {
            ESP_LOGI(TAG, "CAN RX: ID=0x%03lX DLC=%d", (unsigned long)frame.id, frame.dlc);
            
            // Handle MODULE_QUERY (discovery protocol)
            if (frame.id == CAN_ID_MODULE_QUERY) {
                ESP_LOGI(TAG, "Received MODULE_QUERY, announcing...");
                can_discovery_handle_query(&frame, 
                    MODULE_TYPE_AUDIO,      // Module type
                    1,                       // Version 1.0
                    0,
                    MODULE_CAP_STATUS,       // Has status messages
                    0x42,                    // CAN block: 0x420-0x42F
                    0                        // Node ID (single module)
                );
            }
            // Handle PLAY_SOUND command (0x420)
            else if (frame.id == CAN_ID_PLAY_SOUND) {
                uint16_t sound_index;
                uint8_t flags, volume;
                uint16_t request_id;
                
                if (can_audio_parse_play_sound(&frame, &sound_index, &flags, &volume, &request_id)) {
                    ESP_LOGI(TAG, "PLAY_SOUND: index=%d flags=0x%02X vol=%d req_id=%d",
                             sound_index, flags, volume, request_id);
                    
                    int active_count = audio_mixer_get_active_count();
                    bool should_play = (active_count < MAX_AUDIO_SOURCES) || (flags & CAN_AUDIO_FLAG_INTERRUPT);
                    
                    if (should_play) {
                        bool loop = (flags & CAN_AUDIO_FLAG_LOOP) != 0;
                        bool interrupt = (flags & CAN_AUDIO_FLAG_INTERRUPT) != 0;
                        
                        g_last_sound_index = sound_index;
                        audio_source_handle_t handle;
                        esp_err_t ret = audio_player_play_sound_by_index(sound_index, volume, loop, interrupt, &handle);
                        
                        uint8_t queue_id = 0;
                        if (ret == ESP_OK) {
                            // Allocate queue ID and associate with source
                            queue_id = can_audio_allocate_queue_id(&g_next_queue_id);
                            audio_mixer_set_queue_id(handle, queue_id, sound_index);
                            ESP_LOGI(TAG, "Assigned queue_id=%d to source handle=%d", queue_id, handle);
                            g_last_error = 0;
                        } else {
                            g_last_error = CAN_AUDIO_ERR_FILE_NOT_FOUND;
                        }
                        
                        // Send ACK with queue_id (new 5-parameter signature)
                        can_frame_t ack_frame;
                        can_audio_build_sound_ack(
                            ret == ESP_OK ? 1 : 0,  // ok
                            sound_index,            // sound_index
                            queue_id,               // queue_id (0 if error)
                            ret == ESP_OK ? CAN_AUDIO_ERR_OK : CAN_AUDIO_ERR_FILE_NOT_FOUND,  // error_code
                            request_id,             // request_id
                            &ack_frame
                        );
                        can_driver_send(&ack_frame);
                        ESP_LOGI(TAG, "Sent ACK: ok=%d queue_id=%d handle=%d active=%d", 
                                ret == ESP_OK, queue_id, handle, audio_mixer_get_active_count());
                    } else {
                        // Mixer full - send error ACK
                        can_frame_t ack_frame;
                        can_audio_build_sound_ack(
                            0,                      // ok=false
                            sound_index,
                            0,                      // queue_id=0 (error)
                            CAN_AUDIO_ERR_MIXER_FULL,     // error_code
                            request_id,
                            &ack_frame
                        );
                        can_driver_send(&ack_frame);
                        ESP_LOGW(TAG, "Sent NACK: mixer full (max sources=%d)", MAX_AUDIO_SOURCES);
                    }
                }
            }
            // Handle STOP_SOUND command (0x421)
            else if (frame.id == CAN_ID_STOP_SOUND) {
                uint8_t queue_id;
                uint8_t flags;
                uint16_t request_id;
                
                if (can_audio_parse_stop_sound(&frame, &queue_id, &flags, &request_id)) {
                    ESP_LOGI(TAG, "STOP_SOUND: queue_id=%d flags=0x%02X", queue_id, flags);
                    
                    esp_err_t ret = audio_mixer_stop_by_queue_id(queue_id);
                    
                    // Send ACK (reusing SOUND_ACK format)
                    can_frame_t ack_frame;
                    can_audio_build_sound_ack(
                        ret == ESP_OK ? 1 : 0,  // ok
                        0,                       // sound_index (not applicable for STOP)
                        queue_id,                // queue_id (echoed)
                        ret == ESP_OK ? CAN_AUDIO_ERR_OK : CAN_AUDIO_ERR_INVALID_QUEUE_ID,  // error_code
                        request_id,              // request_id
                        &ack_frame
                    );
                    can_driver_send(&ack_frame);
                    
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "Stopped queue_id=%d", queue_id);
                    } else {
                        ESP_LOGW(TAG, "Failed to stop queue_id=%d (not found)", queue_id);
                    }
                }
            }
            // Handle STOP_ALL command (0x424)
            else if (frame.id == CAN_ID_STOP_ALL) {
                ESP_LOGI(TAG, "STOP_ALL received");
                audio_mixer_stop_all();
                ESP_LOGI(TAG, "Stopped all sources");
            }
        }
        
        // Send periodic STATUS message every 5 seconds
        uptime_sec = esp_log_timestamp() / 1000;
        if (uptime_sec % (CAN_AUDIO_STATUS_INTERVAL_MS / 1000) == 0) {
            int active_sources = audio_mixer_get_active_count();
            
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
                CAN_AUDIO_VOLUME_USE_POT,  // Volume from pot
                uptime_sec,
                &status_frame
            );
            can_driver_send(&status_frame);
            
            ESP_LOGI(TAG, "STATUS: bits=0x%02X active=%d uptime=%lus",
                     state_bits, active_sources, (unsigned long)uptime_sec);
            
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
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
    BaseType_t ret = xTaskCreatePinnedToCore(
        can_rx_task,
        "can_rx",
        4096,
        NULL,
        6,  // Higher priority than serial
        NULL,
        tskNO_AFFINITY
    );
    
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
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
    can_driver_send(&finished_frame);
}
