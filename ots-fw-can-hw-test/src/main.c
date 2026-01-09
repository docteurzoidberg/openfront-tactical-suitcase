/**
 * Minimal CAN Hardware Test
 * 
 * Validates TWAI peripheral at lowest level:
 * 1. Init test (does TWAI start?)
 * 2. Loopback test (can we TX and RX to ourselves?)
 * 3. TX voltage test (does TX pin toggle?)
 * 4. Two-device test (can we communicate?)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/twai.h"

static const char *TAG = "CAN_TEST";

// Test stages
typedef enum {
    STAGE_INIT,
    STAGE_LOOPBACK,
    STAGE_TX_VOLTAGE,
    STAGE_TWO_DEVICE,
    STAGE_DONE
} test_stage_t;

static test_stage_t current_stage = STAGE_INIT;

// Stats
static uint32_t tx_success = 0;
static uint32_t tx_failed = 0;
static uint32_t rx_success = 0;

/**
 * Stage 1: Initialize TWAI driver
 */
static bool test_init(void) {
    printf("\n=== STAGE 1: TWAI INITIALIZATION ===\n");
    printf("Board: %s\n", BOARD_NAME);
    printf("TX Pin: GPIO%d\n", CAN_TX_GPIO);
    printf("RX Pin: GPIO%d\n", CAN_RX_GPIO);
    
    // Configure TWAI
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_GPIO, 
        (gpio_num_t)CAN_RX_GPIO, 
        TWAI_MODE_NORMAL
    );
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    // Install driver
    printf("Installing TWAI driver...\n");
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        printf("✗ FAILED: twai_driver_install() = %s\n", esp_err_to_name(ret));
        return false;
    }
    printf("✓ Driver installed\n");
    
    // Start driver
    printf("Starting TWAI driver...\n");
    ret = twai_start();
    if (ret != ESP_OK) {
        printf("✗ FAILED: twai_start() = %s\n", esp_err_to_name(ret));
        return false;
    }
    printf("✓ Driver started\n");
    
    // Check status
    twai_status_info_t status;
    ret = twai_get_status_info(&status);
    if (ret == ESP_OK) {
        printf("✓ TWAI Status: %s\n", 
               status.state == TWAI_STATE_RUNNING ? "RUNNING" :
               status.state == TWAI_STATE_BUS_OFF ? "BUS_OFF" :
               status.state == TWAI_STATE_STOPPED ? "STOPPED" : "UNKNOWN");
        printf("  TX Error Counter: %d\n", status.tx_error_counter);
        printf("  RX Error Counter: %d\n", status.rx_error_counter);
    }
    
    printf("=== STAGE 1: PASSED ===\n");
    return true;
}

/**
 * Stage 2: Loopback test (TX and RX to self)
 */
static bool test_loopback(void) {
    printf("\n=== STAGE 2: LOOPBACK TEST ===\n");
    printf("Reconfiguring for loopback mode...\n");
    
    // Stop and uninstall
    twai_stop();
    twai_driver_uninstall();
    
    // Reinstall in loopback mode
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_GPIO, 
        (gpio_num_t)CAN_RX_GPIO, 
        TWAI_MODE_NO_ACK  // Loopback-like (self-test)
    );
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        printf("✗ FAILED: Reinstall failed\n");
        return false;
    }
    
    ret = twai_start();
    if (ret != ESP_OK) {
        printf("✗ FAILED: Start failed\n");
        return false;
    }
    
    printf("✓ Loopback mode active\n");
    
    // Send test frame
    twai_message_t tx_msg = {
        .identifier = 0x123,
        .data_length_code = 8,
        .data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}
    };
    
    printf("Sending test frame...\n");
    ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(1000));
    if (ret == ESP_OK) {
        printf("✓ Frame transmitted\n");
        tx_success++;
    } else {
        printf("✗ TX FAILED: %s\n", esp_err_to_name(ret));
        tx_failed++;
    }
    
    // Try to receive (should work in NO_ACK mode)
    printf("Waiting for RX...\n");
    twai_message_t rx_msg;
    ret = twai_receive(&rx_msg, pdMS_TO_TICKS(2000));
    if (ret == ESP_OK) {
        printf("✓ Frame received!\n");
        printf("  ID: 0x%03lX, DLC: %d\n", rx_msg.identifier, rx_msg.data_length_code);
        printf("  Data: ");
        for (int i = 0; i < rx_msg.data_length_code; i++) {
            printf("%02X ", rx_msg.data[i]);
        }
        printf("\n");
        rx_success++;
        printf("=== STAGE 2: PASSED ===\n");
        return true;
    } else {
        printf("✗ RX FAILED: %s\n", esp_err_to_name(ret));
        printf("=== STAGE 2: FAILED ===\n");
        return false;
    }
}

/**
 * Stage 3: TX voltage test
 */
static bool test_tx_voltage(void) {
    printf("\n=== STAGE 3: TX VOLTAGE TEST ===\n");
    printf("Reconfiguring for normal mode...\n");
    
    // Stop and uninstall
    twai_stop();
    twai_driver_uninstall();
    
    // Reinstall in normal mode
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_GPIO, 
        (gpio_num_t)CAN_RX_GPIO, 
        TWAI_MODE_NORMAL
    );
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        printf("✗ FAILED: Reinstall failed\n");
        return false;
    }
    
    ret = twai_start();
    if (ret != ESP_OK) {
        printf("✗ FAILED: Start failed\n");
        return false;
    }
    
    printf("✓ Normal mode active\n");
    printf("\n*** MEASURE GPIO%d WITH MULTIMETER/SCOPE ***\n", CAN_TX_GPIO);
    printf("You should see voltage toggling during transmission attempts.\n");
    printf("Sending 10 frames (will fail without ACK, but TX pin should toggle)...\n\n");
    
    for (int i = 0; i < 10; i++) {
        twai_message_t tx_msg = {
            .identifier = 0x100 + i,
            .data_length_code = 8,
            .data = {i, i+1, i+2, i+3, i+4, i+5, i+6, i+7}
        };
        
        printf("TX #%d (ID 0x%03lX)... ", i+1, tx_msg.identifier);
        ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(500));
        if (ret == ESP_OK) {
            printf("OK\n");
            tx_success++;
        } else {
            printf("FAIL (%s)\n", esp_err_to_name(ret));
            tx_failed++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Check error counters
    twai_status_info_t status;
    twai_get_status_info(&status);
    printf("\nTWAI Status after TX test:\n");
    printf("  State: %s\n", 
           status.state == TWAI_STATE_RUNNING ? "RUNNING" :
           status.state == TWAI_STATE_BUS_OFF ? "BUS_OFF" : "OTHER");
    printf("  TX Error Counter: %d\n", status.tx_error_counter);
    printf("  RX Error Counter: %d\n", status.rx_error_counter);
    
    printf("\nDid you measure voltage toggling on GPIO%d? (y/n): ", CAN_TX_GPIO);
    printf("=== STAGE 3: MANUAL VERIFICATION REQUIRED ===\n");
    return true;
}

/**
 * Stage 4: Two-device test
 */
static bool test_two_device(void) {
    printf("\n=== STAGE 4: TWO-DEVICE TEST ===\n");
    printf("Connect this device to another ESP32 with CAN transceiver.\n");
    printf("Ensure CANH-CANH, CANL-CANL, common GND, 120Ω termination on each.\n");
    printf("\nSending 5 test frames every 2 seconds...\n");
    printf("Listening for frames from other device...\n\n");
    
    // Already in normal mode from stage 3
    
    uint32_t loop_count = 0;
    while (loop_count < 5) {
        loop_count++;
        
        // Send frame
        twai_message_t tx_msg = {
            .identifier = 0x200 + loop_count,
            .data_length_code = 8,
            .data = {0xAA, 0xBB, loop_count, 0x00, 0x00, 0x00, 0x00, 0x00}
        };
        
        printf("[%lu] Sending ID 0x%03lX... ", loop_count, tx_msg.identifier);
        esp_err_t ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK) {
            printf("✓ TX OK\n");
            tx_success++;
        } else {
            printf("✗ TX FAIL (%s)\n", esp_err_to_name(ret));
            tx_failed++;
        }
        
        // Check for RX
        twai_message_t rx_msg;
        ret = twai_receive(&rx_msg, pdMS_TO_TICKS(500));
        if (ret == ESP_OK) {
            printf("[%lu] ✓ Received ID 0x%03lX, DLC %d: ", 
                   loop_count, rx_msg.identifier, rx_msg.data_length_code);
            for (int i = 0; i < rx_msg.data_length_code; i++) {
                printf("%02X ", rx_msg.data[i]);
            }
            printf("\n");
            rx_success++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // Final stats
    twai_status_info_t status;
    twai_get_status_info(&status);
    
    printf("\n=== FINAL STATISTICS ===\n");
    printf("TX Success: %lu\n", tx_success);
    printf("TX Failed: %lu\n", tx_failed);
    printf("RX Success: %lu\n", rx_success);
    printf("TWAI State: %s\n", 
           status.state == TWAI_STATE_RUNNING ? "RUNNING" :
           status.state == TWAI_STATE_BUS_OFF ? "BUS_OFF" : "OTHER");
    printf("TX Error Counter: %d\n", status.tx_error_counter);
    printf("RX Error Counter: %d\n", status.rx_error_counter);
    
    if (tx_success > 0 && rx_success > 0 && status.state == TWAI_STATE_RUNNING) {
        printf("=== STAGE 4: PASSED ===\n");
        return true;
    } else {
        printf("=== STAGE 4: FAILED ===\n");
        return false;
    }
}

void app_main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║  CAN Hardware Validation Test       ║\n");
    printf("║  Bare-metal TWAI driver testing     ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Stage 1: Init
    if (!test_init()) {
        printf("\n✗✗✗ INIT FAILED - STOPPING ✗✗✗\n");
        return;
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Stage 2: Loopback
    if (!test_loopback()) {
        printf("\n⚠ LOOPBACK FAILED - CONTINUING ANYWAY\n");
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Stage 3: TX voltage test
    test_tx_voltage();
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Stage 4: Two-device test
    test_two_device();
    
    printf("\n=== ALL TESTS COMPLETE ===\n");
    printf("Review results above.\n");
}
