/**
 * CAN Simulator - Emulate Controller or Modules
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "can_test.h"
#include "can_driver.h"
#include "can_discovery.h"

static const char *TAG = "simulator";

// Audio module state
static struct {
    bool active;
    uint8_t queue_id_counter;
} audio_module_state = {
    .active = false,
    .queue_id_counter = 1
};

// Controller state
static struct {
    bool active;
} controller_state = {
    .active = false
};

// Audio Module CAN IDs (from /prompts/CANBUS_MESSAGE_SPEC.md)
#define CAN_ID_PLAY_SOUND      0x420
#define CAN_ID_STOP_SOUND      0x421
#define CAN_ID_STOP_ALL        0x422
#define CAN_ID_SOUND_ACK       0x423
#define CAN_ID_STOP_ACK        0x424
#define CAN_ID_SOUND_FINISHED  0x425

void can_simulator_audio_module_start(void) {
    audio_module_state.active = true;
    audio_module_state.queue_id_counter = 1;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              AUDIO MODULE SIMULATOR - ACTIVE                   ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Responding to:                                                ║\n");
    printf("║    - Discovery queries (MODULE_TYPE_AUDIO v1.0)                ║\n");
    printf("║    - PLAY_SOUND (0x420) → auto-send ACK (0x423)                ║\n");
    printf("║    - STOP_SOUND (0x421) → auto-send ACK (0x424)                ║\n");
    printf("║    - STOP_ALL (0x422)                                          ║\n");
    printf("║                                                                ║\n");
    printf("║  Manual commands:                                              ║\n");
    printf("║    f <queue_id> - Send SOUND_FINISHED (0x425)                  ║\n");
    printf("║    q            - Stop simulator                               ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    ESP_LOGI(TAG, "Audio module simulator started");
}

void can_simulator_controller_start(void) {
    controller_state.active = true;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              CONTROLLER SIMULATOR - ACTIVE                     ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Manual commands:                                              ║\n");
    printf("║    d           - Send MODULE_QUERY (discovery)                 ║\n");
    printf("║    p <idx>     - Send PLAY_SOUND                               ║\n");
    printf("║    s <qid>     - Send STOP_SOUND                               ║\n");
    printf("║    x           - Send STOP_ALL                                 ║\n");
    printf("║    q           - Stop simulator                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    ESP_LOGI(TAG, "Controller simulator started");
}

void can_simulator_stop(void) {
    audio_module_state.active = false;
    controller_state.active = false;
    ESP_LOGI(TAG, "Simulator stopped");
}

void can_simulator_process_frame(const can_frame_t *frame) {
    // Audio module simulator
    if (audio_module_state.active) {
        // Handle discovery query
        if (frame->id == CAN_ID_MODULE_QUERY) {
            printf("← RX: ");
            can_decoder_print_frame(frame, false, true);
            
            printf("[DEBUG] Calling can_discovery_handle_query()...\n");
            
            // Auto-respond with MODULE_ANNOUNCE
            esp_err_t ret = can_discovery_handle_query(frame, MODULE_TYPE_AUDIO, 1, 0, 
                                                        MODULE_CAP_STATUS, 0x42, 0);
            
            if (ret == ESP_OK) {
                printf("→ TX: MODULE_ANNOUNCE (AUDIO v1.0, block 0x42) - SUCCESS\n");
                g_test_state.tx_count++;
            } else {
                printf("✗ TX: MODULE_ANNOUNCE FAILED: %s\n", esp_err_to_name(ret));
            }
            return;
        }
        
        // Handle PLAY_SOUND
        if (frame->id == CAN_ID_PLAY_SOUND) {
            printf("← RX: ");
            can_decoder_print_frame(frame, true, true);
            
            // Extract parameters
            uint8_t sound_idx = frame->data[0];
            uint8_t flags = frame->data[1];
            uint8_t volume = frame->data[2];
            
            // Send ACK
            can_frame_t ack;
            ack.id = CAN_ID_SOUND_ACK;
            ack.dlc = 8;
            ack.extended = false;
            ack.rtr = false;
            ack.data[0] = sound_idx;  // Echo sound index
            ack.data[1] = 0x00;       // Status: SUCCESS
            ack.data[2] = audio_module_state.queue_id_counter++;  // Queue ID
            ack.data[3] = 0x00;
            memset(&ack.data[4], 0, 4);
            
            can_driver_send(&ack);
            printf("→ TX: SOUND_ACK (queue_id=%d)\n", ack.data[2]);
            g_test_state.tx_count++;
            return;
        }
        
        // Handle STOP_SOUND
        if (frame->id == CAN_ID_STOP_SOUND) {
            printf("← RX: ");
            can_decoder_print_frame(frame, true, true);
            
            uint8_t queue_id = frame->data[0];
            
            // Send ACK
            can_frame_t ack;
            ack.id = CAN_ID_STOP_ACK;
            ack.dlc = 8;
            ack.extended = false;
            ack.rtr = false;
            ack.data[0] = queue_id;   // Echo queue ID
            ack.data[1] = 0x00;       // Status: SUCCESS
            memset(&ack.data[2], 0, 6);
            
            can_driver_send(&ack);
            printf("→ TX: STOP_ACK (queue_id=%d)\n", queue_id);
            g_test_state.tx_count++;
            return;
        }
        
        // Handle STOP_ALL
        if (frame->id == CAN_ID_STOP_ALL) {
            printf("← RX: ");
            can_decoder_print_frame(frame, false, true);
            printf("   (All sounds stopped)\n");
            return;
        }
    }
    
    // Controller simulator - just show received messages
    if (controller_state.active) {
        printf("← RX: ");
        can_decoder_print_frame(frame, true, true);
    }
}

esp_err_t can_simulator_send_custom(uint16_t can_id, const uint8_t *data, uint8_t dlc) {
    can_frame_t frame;
    frame.id = can_id;
    frame.dlc = dlc;
    frame.extended = false;
    frame.rtr = false;
    memcpy(frame.data, data, dlc);
    
    esp_err_t ret = can_driver_send(&frame);
    if (ret == ESP_OK) {
        printf("→ TX: ");
        can_decoder_print_frame(&frame, true, true);
        g_test_state.tx_count++;
    }
    return ret;
}
