/*
 * Output Board Test (MCP23017 @ 0x21)
 * 
 * Tests all 16 output pins with various patterns:
 *   - Walking bit (one pin at a time)
 *   - All on/off
 *   - Alternating pattern
 * 
 * Connect LEDs or use multimeter to verify each output pin.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define MCP23017_ADDR        0x21  // Output board
#define MCP23017_IODIRA      0x00  // I/O direction A
#define MCP23017_IODIRB      0x01  // I/O direction B
#define MCP23017_GPIOA       0x12  // Port A
#define MCP23017_GPIOB       0x13  // Port B

static const char *TAG = "TEST_OUTPUTS";

void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C master initialized");
}

esp_err_t mcp23017_write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void mcp23017_init_outputs(void) {
    ESP_LOGI(TAG, "Configuring MCP23017 @ 0x21 as outputs...");
    
    // Configure all pins as outputs (0 = output)
    esp_err_t ret = mcp23017_write_reg(MCP23017_IODIRA, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Port A direction");
        return;
    }
    
    ret = mcp23017_write_reg(MCP23017_IODIRB, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Port B direction");
        return;
    }
    
    // Clear all outputs
    mcp23017_write_reg(MCP23017_GPIOA, 0x00);
    mcp23017_write_reg(MCP23017_GPIOB, 0x00);
    
    ESP_LOGI(TAG, "MCP23017 configured successfully");
}

void test_walking_bit(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Walking Bit Test ===");
    ESP_LOGI(TAG, "Watch LEDs: one should light at a time");
    
    // Test Port A (pins 0-7)
    ESP_LOGI(TAG, "Testing Port A (pins 0-7)...");
    for (int i = 0; i < 8; i++) {
        uint8_t pattern = (1 << i);
        mcp23017_write_reg(MCP23017_GPIOA, pattern);
        mcp23017_write_reg(MCP23017_GPIOB, 0x00);
        ESP_LOGI(TAG, "  A%d HIGH (0x%02X, 0x00)", i, pattern);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Test Port B (pins 8-15)
    ESP_LOGI(TAG, "Testing Port B (pins 8-15)...");
    for (int i = 0; i < 8; i++) {
        uint8_t pattern = (1 << i);
        mcp23017_write_reg(MCP23017_GPIOA, 0x00);
        mcp23017_write_reg(MCP23017_GPIOB, pattern);
        ESP_LOGI(TAG, "  B%d HIGH (0x00, 0x%02X)", i, pattern);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void test_all_on_off(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== All Pins On/Off Test ===");
    
    // All ON
    ESP_LOGI(TAG, "All outputs HIGH (0xFF, 0xFF)");
    mcp23017_write_reg(MCP23017_GPIOA, 0xFF);
    mcp23017_write_reg(MCP23017_GPIOB, 0xFF);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // All OFF
    ESP_LOGI(TAG, "All outputs LOW (0x00, 0x00)");
    mcp23017_write_reg(MCP23017_GPIOA, 0x00);
    mcp23017_write_reg(MCP23017_GPIOB, 0x00);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void test_alternating_pattern(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Alternating Pattern Test ===");
    ESP_LOGI(TAG, "Even/odd pins alternating 5 times");
    
    for (int i = 0; i < 5; i++) {
        // Even pins
        mcp23017_write_reg(MCP23017_GPIOA, 0xAA);  // 10101010
        mcp23017_write_reg(MCP23017_GPIOB, 0xAA);
        ESP_LOGI(TAG, "  Even pins HIGH (0xAA, 0xAA)");
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Odd pins
        mcp23017_write_reg(MCP23017_GPIOA, 0x55);  // 01010101
        mcp23017_write_reg(MCP23017_GPIOB, 0x55);
        ESP_LOGI(TAG, "  Odd pins HIGH (0x55, 0x55)");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Clear
    mcp23017_write_reg(MCP23017_GPIOA, 0x00);
    mcp23017_write_reg(MCP23017_GPIOB, 0x00);
}

void app_main(void) {
    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    OTS Output Board Test              ║");
    ESP_LOGI(TAG, "║    MCP23017 @ 0x21                    ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    
    i2c_master_init();
    mcp23017_init_outputs();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Connect LEDs to output pins to visualize tests");
    ESP_LOGI(TAG, "Test cycle starts in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    int cycle = 1;
    while (1) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGI(TAG, "║ Test Cycle %d                          ║", cycle++);
        ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
        
        test_walking_bit();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_all_on_off();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_alternating_pattern();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Cycle complete. Next cycle in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
