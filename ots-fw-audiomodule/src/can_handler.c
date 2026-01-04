/**
 * @file can_handler.c
 * @brief CAN Bus Message Handler Implementation
 */

#include "can_handler.h"
#include "can_driver.h"
#include "can_protocol.h"
#include "audio_mixer.h"
#include "audio_player.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "CAN_HANDLER";

// Module state
static bool *g_sd_mounted = NULL;
static uint16_t g_last_sound_index = 0;
static uint8_t g_last_error = CAN_ERR_OK;

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
            
            // Handle PLAY_SOUND command (0x420)
            if (frame.id == CAN_ID_PLAY_SOUND) {
                uint16_t sound_index;
                uint8_t flags, volume;
                uint16_t request_id;
                
                if (can_parse_play_sound(&frame, &sound_index, &flags, &volume, &request_id)) {
                    ESP_LOGI(TAG, "PLAY_SOUND: index=%d flags=0x%02X vol=%d req_id=%d",
                             sound_index, flags, volume, request_id);
                    
                    int active_count = audio_mixer_get_active_count();
                    bool should_play = (active_count < MAX_AUDIO_SOURCES) || (flags & CAN_FLAG_INTERRUPT);
                    
                    if (should_play) {
                        bool loop = (flags & CAN_FLAG_LOOP) != 0;
                        bool interrupt = (flags & CAN_FLAG_INTERRUPT) != 0;
                        
                        g_last_sound_index = sound_index;
                        audio_source_handle_t handle;
                        esp_err_t ret = audio_player_play_sound_by_index(sound_index, volume, loop, interrupt, &handle);
                        g_last_error = (ret == ESP_OK) ? 0 : 1;
                        
                        can_frame_t ack_frame;
                        can_build_sound_ack(
                            ret == ESP_OK ? 1 : 0,
                            sound_index,
                            ret == ESP_OK ? 0 : 1,
                            request_id,
                            &ack_frame
                        );
                        can_driver_send(&ack_frame);
                        ESP_LOGI(TAG, "Sent ACK: ok=%d handle=%d active=%d", 
                                ret == ESP_OK, handle, audio_mixer_get_active_count());
                    } else {
                        can_frame_t ack_frame;
                        can_build_sound_ack(0, sound_index, 3, request_id, &ack_frame);
                        can_driver_send(&ack_frame);
                        ESP_LOGW(TAG, "Sent NACK: max sources=%d", MAX_AUDIO_SOURCES);
                    }
                }
            }
            // Handle STOP_SOUND command (0x421)
            else if (frame.id == CAN_ID_STOP_SOUND) {
                uint16_t sound_index;
                uint8_t flags;
                uint16_t request_id;
                
                if (can_parse_stop_sound(&frame, &sound_index, &flags, &request_id)) {
                    ESP_LOGI(TAG, "STOP_SOUND: index=%d flags=0x%02X", sound_index, flags);
                    
                    if (flags & CAN_FLAG_STOP_ALL) {
                        audio_mixer_stop_all();
                        ESP_LOGI(TAG, "Stopped all sources");
                    } else {
                        audio_mixer_stop_all();
                        ESP_LOGI(TAG, "Stopped sound %d (all sources)", sound_index);
                    }
                    
                    can_frame_t ack_frame;
                    can_build_sound_ack(1, sound_index, 0, request_id, &ack_frame);
                    can_driver_send(&ack_frame);
                }
            }
        }
        
        // Send periodic STATUS message every 5 seconds
        uptime_sec = esp_log_timestamp() / 1000;
        if (uptime_sec % 5 == 0) {
            int active_sources = audio_mixer_get_active_count();
            
            uint8_t state_bits = 0;
            if (g_sd_mounted && *g_sd_mounted) state_bits |= CAN_STATUS_SD_MOUNTED;
            if (active_sources > 0) state_bits |= CAN_STATUS_PLAYING;
            if (g_last_error == 0) state_bits |= CAN_STATUS_READY;
            if (g_last_error != 0) state_bits |= CAN_STATUS_ERROR;
            
            can_frame_t status_frame;
            can_build_sound_status(
                state_bits,
                active_sources > 0 ? g_last_sound_index : 0xFFFF,
                g_last_error,
                CAN_VOLUME_USE_POT,  // Volume from pot
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

esp_err_t can_handler_init(bool *sd_mounted)
{
    if (sd_mounted == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_sd_mounted = sd_mounted;
    g_last_sound_index = 0;
    g_last_error = CAN_ERR_OK;
    
    return ESP_OK;
}

esp_err_t can_handler_start_task(void)
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
