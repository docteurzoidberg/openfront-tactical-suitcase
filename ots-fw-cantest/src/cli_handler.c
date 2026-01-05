/**
 * CLI Handler - Interactive Command Line Interface
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "can_test.h"
#include "can_discovery.h"

static const char *TAG = "cli";

// Helper to read a line from stdin (non-blocking)
static int read_line(char *buffer, int max_len, TickType_t timeout) {
    int pos = 0;
    TickType_t start = xTaskGetTickCount();
    
    while (pos < max_len - 1) {
        int c = fgetc(stdin);
        
        if (c == EOF || c == 0xFF) {
            // No data available, check timeout
            if (xTaskGetTickCount() - start >= timeout) {
                return 0;  // Timeout
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            return pos;
        }
        
        buffer[pos++] = c;
    }
    
    buffer[pos] = '\0';
    return pos;
}

static void print_help(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                         COMMANDS                               ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ MODE CONTROL:                                                  ║\n");
    printf("║   m          Monitor mode (passive bus sniffer)                ║\n");
    printf("║   a          Audio module simulator                            ║\n");
    printf("║   c          Controller simulator                              ║\n");
    printf("║   i          Idle mode (stop current mode)                     ║\n");
    printf("║                                                                ║\n");
    printf("║ CONTROLLER COMMANDS (in controller mode):                      ║\n");
    printf("║   d          Send MODULE_QUERY (discovery)                     ║\n");
    printf("║   p <idx>    Send PLAY_SOUND (idx=sound index 0-255)           ║\n");
    printf("║   s <qid>    Send STOP_SOUND (qid=queue ID)                    ║\n");
    printf("║   x          Send STOP_ALL                                     ║\n");
    printf("║                                                                ║\n");
    printf("║ AUDIO MODULE COMMANDS (in audio module mode):                  ║\n");
    printf("║   f <qid>    Send SOUND_FINISHED (qid=queue ID)                ║\n");
    printf("║                                                                ║\n");
    printf("║ DISPLAY OPTIONS:                                               ║\n");
    printf("║   r          Toggle raw hex display                            ║\n");
    printf("║   v          Toggle parsed message display                     ║\n");
    printf("║   t          Show statistics                                   ║\n");
    printf("║                                                                ║\n");
    printf("║ GENERAL:                                                       ║\n");
    printf("║   h or ?     Show this help                                    ║\n");
    printf("║   q          Quit/stop current mode                            ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_stats(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                         STATISTICS                             ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Mode:            %-44s ║\n", 
           g_test_state.mode == MODE_IDLE ? "IDLE" :
           g_test_state.mode == MODE_MONITOR ? "MONITOR" :
           g_test_state.mode == MODE_AUDIO_MODULE ? "AUDIO MODULE" :
           g_test_state.mode == MODE_CONTROLLER ? "CONTROLLER" : "UNKNOWN");
    printf("║  Messages RX:     %-44lu║\n", g_test_state.rx_count);
    printf("║  Messages TX:     %-44lu║\n", g_test_state.tx_count);
    printf("║  Errors:          %-44lu║\n", g_test_state.error_count);
    printf("║  Show Raw Hex:    %-44s║\n", g_test_state.show_raw_hex ? "YES" : "NO");
    printf("║  Show Parsed:     %-44s║\n", g_test_state.show_parsed ? "YES" : "NO");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void handle_command(const char *cmd) {
    if (strlen(cmd) == 0) return;
    
    char command = cmd[0];
    const char *args = cmd + 1;
    while (*args == ' ') args++;  // Skip spaces
    
    switch (command) {
        case 'h':
        case '?':
            print_help();
            break;
        
        case 'm':
            g_test_state.mode = MODE_MONITOR;
            can_monitor_start();
            break;
        
        case 'a':
            g_test_state.mode = MODE_AUDIO_MODULE;
            can_simulator_audio_module_start();
            break;
        
        case 'c':
            g_test_state.mode = MODE_CONTROLLER;
            can_simulator_controller_start();
            break;
        
        case 'i':
        case 'q':
            if (g_test_state.mode == MODE_MONITOR) {
                can_monitor_stop();
            } else if (g_test_state.mode != MODE_IDLE) {
                can_simulator_stop();
            }
            g_test_state.mode = MODE_IDLE;
            printf("Mode: IDLE\n");
            break;
        
        case 'r':
            g_test_state.show_raw_hex = !g_test_state.show_raw_hex;
            printf("Raw hex display: %s\n", g_test_state.show_raw_hex ? "ON" : "OFF");
            break;
        
        case 'v':
            g_test_state.show_parsed = !g_test_state.show_parsed;
            printf("Parsed display: %s\n", g_test_state.show_parsed ? "ON" : "OFF");
            break;
        
        case 't':
            print_stats();
            break;
        
        // Controller mode commands
        case 'd':
            if (g_test_state.mode == MODE_CONTROLLER) {
                can_discovery_query_all();
                printf("→ TX: MODULE_QUERY (0x411)\n");
                g_test_state.tx_count++;
            } else {
                printf("Error: Not in controller mode. Use 'c' first.\n");
            }
            break;
        
        case 'p': {
            if (g_test_state.mode == MODE_CONTROLLER) {
                int sound_idx = atoi(args);
                if (sound_idx < 0 || sound_idx > 255) {
                    printf("Error: Sound index must be 0-255\n");
                    break;
                }
                
                uint8_t data[8] = {sound_idx, 0x00, 100, 0, 0, 0, 0, 0};  // No loop, 100% vol
                can_simulator_send_custom(0x420, data, 8);
            } else {
                printf("Error: Not in controller mode. Use 'c' first.\n");
            }
            break;
        }
        
        case 's': {
            if (g_test_state.mode == MODE_CONTROLLER) {
                int queue_id = atoi(args);
                if (queue_id < 0 || queue_id > 255) {
                    printf("Error: Queue ID must be 0-255\n");
                    break;
                }
                
                uint8_t data[8] = {queue_id, 0, 0, 0, 0, 0, 0, 0};
                can_simulator_send_custom(0x421, data, 8);
            } else {
                printf("Error: Not in controller mode. Use 'c' first.\n");
            }
            break;
        }
        
        case 'x':
            if (g_test_state.mode == MODE_CONTROLLER) {
                uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
                can_simulator_send_custom(0x422, data, 8);
            } else {
                printf("Error: Not in controller mode. Use 'c' first.\n");
            }
            break;
        
        // Audio module commands
        case 'f': {
            if (g_test_state.mode == MODE_AUDIO_MODULE) {
                int queue_id = atoi(args);
                if (queue_id < 0 || queue_id > 255) {
                    printf("Error: Queue ID must be 0-255\n");
                    break;
                }
                
                uint8_t data[8] = {queue_id, 1, 0x00, 0, 0, 0, 0, 0};  // sound_idx=1, reason=COMPLETED
                can_simulator_send_custom(0x425, data, 8);
            } else {
                printf("Error: Not in audio module mode. Use 'a' first.\n");
            }
            break;
        }
        
        default:
            printf("Unknown command: '%c'. Type 'h' for help.\n", command);
            break;
    }
}

void cli_handler_init(void) {
    // Nothing to init
}

void cli_handler_run(void) {
    char line[128];
    
    while (g_test_state.running) {
        // Non-blocking read with timeout
        int len = read_line(line, sizeof(line), pdMS_TO_TICKS(100));
        
        if (len > 0) {
            handle_command(line);
            
            // Print prompt if in idle mode
            if (g_test_state.mode == MODE_IDLE) {
                printf("> ");
                fflush(stdout);
            }
        }
        
        // Let other tasks run
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
