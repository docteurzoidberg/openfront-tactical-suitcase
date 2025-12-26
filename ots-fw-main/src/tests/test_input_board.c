/*
 * Input Board Test (MCP23017 @ 0x20)
 * 
 * Continuously reads all 16 input pins and displays their state.
 * Pins are configured with internal pullups (HIGH when not pressed).
 * Connect pins to GND to test (pin goes LOW when pressed).
 * 
 * Expected behavior:
 *   - All pins read 1 (HIGH) when nothing connected
 *   - Pin reads 0 (LOW) when connected to GND
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "ots_logging.h"

#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_NUM       I2C_NUM_0

#define MCP23017_ADDR        0x20  // Input board
#define MCP23017_IODIRA      0x00
#define MCP23017_IODIRB      0x01
#define MCP23017_GPPUA       0x0C  // Pullup A
#define MCP23017_GPPUB       0x0D  // Pullup B
#define MCP23017_GPIOA       0x12
#define MCP23017_GPIOB       0x13

static const char *TAG = "TEST_INPUTS";

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

esp_err_t mcp23017_read_reg(uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  // Repeated start
    i2c_master_write_byte(cmd, (MCP23017_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void mcp23017_init_inputs(void) {
    ESP_LOGI(TAG, "Configuring MCP23017 @ 0x20 as inputs...");
    
    // Configure all pins as inputs (1 = input)
    esp_err_t ret = mcp23017_write_reg(MCP23017_IODIRA, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Port A direction");
        return;
    }
    
    ret = mcp23017_write_reg(MCP23017_IODIRB, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Port B direction");
        return;
    }
    
    // Enable pullups on all pins
    ret = mcp23017_write_reg(MCP23017_GPPUA, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable Port A pullups");
        return;
    }
    
    ret = mcp23017_write_reg(MCP23017_GPPUB, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable Port B pullups");
        return;
    }
    
    ESP_LOGI(TAG, "MCP23017 configured successfully");
    ESP_LOGI(TAG, "All pins INPUT with pullups enabled");
}

void print_input_state(void) {
    uint8_t porta = 0, portb = 0;
    
    esp_err_t ret_a = mcp23017_read_reg(MCP23017_GPIOA, &porta);
    esp_err_t ret_b = mcp23017_read_reg(MCP23017_GPIOB, &portb);
    
    if (ret_a != ESP_OK || ret_b != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read GPIO ports");
        return;
    }
    
    // Print as single line that updates in place
    printf("\rInputs: A=0x%02X B=0x%02X | ", porta, portb);
    
    // Show individual pin states
    for (int i = 0; i < 8; i++) {
        printf("A%d=%d ", i, (porta >> i) & 1);
    }
    printf("| ");
    for (int i = 0; i < 8; i++) {
        printf("B%d=%d ", i, (portb >> i) & 1);
    }
    
    fflush(stdout);
}

void app_main(void) {
    (void)ots_logging_init();

    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    OTS Input Board Test               ║");
    ESP_LOGI(TAG, "║    MCP23017 @ 0x20                    ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    i2c_master_init();
    mcp23017_init_inputs();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Instructions:");
    ESP_LOGI(TAG, "  - All pins should read 1 (HIGH) with nothing connected");
    ESP_LOGI(TAG, "  - Connect a pin to GND to see it change to 0 (LOW)");
    ESP_LOGI(TAG, "  - Test each pin individually");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Reading inputs continuously...");
    ESP_LOGI(TAG, "");
    
    while (1) {
        print_input_state();
        vTaskDelay(pdMS_TO_TICKS(100));  // Read at 10 Hz
    }
}
