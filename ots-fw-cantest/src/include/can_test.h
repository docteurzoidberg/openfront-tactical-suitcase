#ifndef CAN_TEST_H
#define CAN_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "can_driver.h"

// Test firmware version
#ifndef CAN_TEST_VERSION
#define CAN_TEST_VERSION "1.0.0"
#endif

// Operating modes
typedef enum {
    MODE_IDLE,              // No active operation
    MODE_MONITOR,           // Passive bus monitoring
    MODE_AUDIO_MODULE,      // Simulate audio module
    MODE_CONTROLLER,        // Simulate main controller
    MODE_TRAFFIC_GEN        // Traffic generator for stress testing
} test_mode_t;

// Global state
typedef struct {
    test_mode_t mode;
    bool running;
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
    bool show_raw_hex;
    bool show_parsed;
    uint16_t can_filter;    // 0 = show all
} test_state_t;

extern test_state_t g_test_state;

// Component interfaces
// can_monitor.c
void can_monitor_start(void);
void can_monitor_stop(void);
void can_monitor_process_frame(const can_frame_t *frame);

// can_simulator.c
void can_simulator_audio_module_start(void);
void can_simulator_controller_start(void);
void can_simulator_stop(void);
void can_simulator_process_frame(const can_frame_t *frame);
esp_err_t can_simulator_send_custom(uint16_t can_id, const uint8_t *data, uint8_t dlc);

// can_decoder.c
void can_decoder_init(void);
void can_decoder_print_frame(const can_frame_t *frame, bool show_raw, bool show_parsed);
const char* can_decoder_get_message_name(uint16_t can_id);

// cli_handler.c
void cli_handler_init(void);
void cli_handler_run(void);

#endif // CAN_TEST_H
