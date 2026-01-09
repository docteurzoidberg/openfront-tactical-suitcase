/**
 * CAN Protocol Decoder - Parse and Display Messages
 */

#include <stdio.h>
#include <string.h>
#include "can_test.h"
#include "can_discovery.h"

// CAN IDs from protocol
#define CAN_ID_MODULE_ANNOUNCE 0x410
#define CAN_ID_MODULE_QUERY    0x411
#define CAN_ID_PLAY_SOUND      0x420
#define CAN_ID_STOP_SOUND      0x421
#define CAN_ID_STOP_ALL        0x422
#define CAN_ID_SOUND_ACK       0x423
#define CAN_ID_STOP_ACK        0x424
#define CAN_ID_SOUND_FINISHED  0x425
#define CAN_ID_SOUND_STATUS    0x426

void can_decoder_init(void) {
    // Nothing to initialize yet
}

const char* can_decoder_get_message_name(uint16_t can_id) {
    switch (can_id) {
        case CAN_ID_MODULE_ANNOUNCE: return "MODULE_ANNOUNCE";
        case CAN_ID_MODULE_QUERY:    return "MODULE_QUERY";
        case CAN_ID_PLAY_SOUND:      return "PLAY_SOUND";
        case CAN_ID_STOP_SOUND:      return "STOP_SOUND";
        case CAN_ID_STOP_ALL:        return "STOP_ALL";
        case CAN_ID_SOUND_ACK:       return "SOUND_ACK";
        case CAN_ID_STOP_ACK:        return "STOP_ACK";
        case CAN_ID_SOUND_FINISHED:  return "SOUND_FINISHED";
        case CAN_ID_SOUND_STATUS:    return "SOUND_STATUS";
        default:                     return "UNKNOWN";
    }
}

static void decode_module_announce(const can_frame_t *frame) {
    if (frame->dlc < 6) return;
    
    uint8_t type = frame->data[0];
    uint8_t ver_maj = frame->data[1];
    uint8_t ver_min = frame->data[2];
    uint8_t caps = frame->data[3];
    uint8_t block = frame->data[4];
    
    printf("      Type: %s (0x%02X), Ver: %d.%d, Block: 0x%02X, Caps: 0x%02X",
           can_discovery_get_module_name(type), type, ver_maj, ver_min, block, caps);
}

static void decode_play_sound(const can_frame_t *frame) {
    if (frame->dlc < 4) return;
    
    uint8_t sound_idx = frame->data[0];
    uint8_t flags = frame->data[1];
    uint8_t volume = frame->data[2];
    uint8_t priority = frame->data[3];
    
    printf("      Sound: %d, Vol: %d, Flags: 0x%02X", sound_idx, volume, flags);
    if (flags & 0x01) printf(" [LOOP]");
    if (flags & 0x02) printf(" [INTERRUPT]");
}

static void decode_sound_ack(const can_frame_t *frame) {
    if (frame->dlc < 3) return;
    
    uint8_t sound_idx = frame->data[0];
    uint8_t status = frame->data[1];
    uint8_t queue_id = frame->data[2];
    
    const char *status_str = "UNKNOWN";
    switch (status) {
        case 0x00: status_str = "SUCCESS"; break;
        case 0x01: status_str = "FILE_NOT_FOUND"; break;
        case 0x02: status_str = "MIXER_FULL"; break;
        case 0x03: status_str = "SD_ERROR"; break;
        case 0xFF: status_str = "UNKNOWN_ERROR"; break;
    }
    
    printf("      Sound: %d, Status: %s, Queue ID: %d", sound_idx, status_str, queue_id);
}

static void decode_stop_sound(const can_frame_t *frame) {
    if (frame->dlc < 1) return;
    uint8_t queue_id = frame->data[0];
    printf("      Queue ID: %d", queue_id);
}

static void decode_sound_finished(const can_frame_t *frame) {
    if (frame->dlc < 3) return;
    
    uint8_t queue_id = frame->data[0];
    uint8_t sound_idx = frame->data[1];
    uint8_t reason = frame->data[2];
    
    const char *reason_str = "UNKNOWN";
    switch (reason) {
        case 0x00: reason_str = "COMPLETED"; break;
        case 0x01: reason_str = "STOPPED_BY_USER"; break;
        case 0x02: reason_str = "PLAYBACK_ERROR"; break;
    }
    
    printf("      Queue ID: %d, Sound: %d, Reason: %s", queue_id, sound_idx, reason_str);
}

static void decode_sound_status(const can_frame_t *frame) {
    if (frame->dlc < 7) return;
    
    uint8_t state_bits = frame->data[0];
    uint16_t current_sound = frame->data[1] | (frame->data[2] << 8);
    uint8_t error_code = frame->data[3];
    uint8_t volume = frame->data[4];
    uint16_t uptime = frame->data[5] | (frame->data[6] << 8);
    
    printf("      Status: ");
    if (state_bits & 0x01) printf("READY ");
    if (state_bits & 0x02) printf("SD_MOUNTED ");
    if (state_bits & 0x04) printf("PLAYING ");
    if (state_bits & 0x08) printf("MUTED ");
    if (state_bits & 0x10) printf("ERROR ");
    
    printf("\n      Sound: %s, Vol: %s, Uptime: %us",
           current_sound == 0xFFFF ? "none" : "", 
           volume == 0xFF ? "POT" : "",
           uptime);
    
    if (error_code != 0) {
        printf(", Error: 0x%02X", error_code);
    }
}

void can_decoder_print_frame(const can_frame_t *frame, bool show_raw, bool show_parsed) {
    // Print CAN ID and message name
    printf("0x%03X [%d] %-17s", frame->id, frame->dlc, 
           can_decoder_get_message_name(frame->id));
    
    // Print raw hex data if requested
    if (show_raw) {
        printf(" | ");
        for (int i = 0; i < frame->dlc; i++) {
            printf("%02X ", frame->data[i]);
        }
        // Pad if DLC < 8
        for (int i = frame->dlc; i < 8; i++) {
            printf("   ");
        }
    }
    
    // Print parsed data if requested
    if (show_parsed) {
        printf("\n");
        switch (frame->id) {
            case CAN_ID_MODULE_ANNOUNCE:
                decode_module_announce(frame);
                break;
            case CAN_ID_PLAY_SOUND:
                decode_play_sound(frame);
                break;
            case CAN_ID_SOUND_ACK:
                decode_sound_ack(frame);
                break;
            case CAN_ID_STOP_SOUND:
                decode_stop_sound(frame);
                break;
            case CAN_ID_SOUND_FINISHED:
                decode_sound_finished(frame);
                break;
            case CAN_ID_SOUND_STATUS:
                decode_sound_status(frame);
                break;
            case CAN_ID_MODULE_QUERY:
                printf("      (Broadcast discovery query)");
                break;
            case CAN_ID_STOP_ALL:
                printf("      (Stop all sounds)");
                break;
            default:
                // Unknown message type
                if (show_raw) {
                    // Already printed hex, just add a note
                    printf("      (Unknown protocol message)");
                }
                break;
        }
    }
    
    printf("\n");
}
