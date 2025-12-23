/*
 * I2C Bus Scan Test
 * 
 * Scans I2C bus and reports all detected devices.
 * Run this test to verify I2C bus functionality and detect connected hardware.
 * 
 * Expected devices:
 *   0x20 - MCP23017 Input Board
 *   0x21 - MCP23017 Output Board
 *   0x27 - PCF8574 LCD Backpack (Troops Module)
 *   0x48 - ADS1015 ADC (Troops Module)
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

static const char *TAG = "TEST_I2C";

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
    ESP_LOGI(TAG, "I2C master initialized (SDA=%d, SCL=%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}

void i2c_scan(void) {
    ESP_LOGI(TAG, "=== Starting I2C scan ===");
    int devices_found = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            const char *device_name = "Unknown";
            switch (addr) {
                case 0x20: device_name = "Input Board (MCP23017)"; break;
                case 0x21: device_name = "Output Board (MCP23017)"; break;
                case 0x27: device_name = "LCD Backpack (PCF8574)"; break;
                case 0x48: device_name = "ADC (ADS1015)"; break;
            }
            ESP_LOGI(TAG, "  [0x%02X] %s", addr, device_name);
            devices_found++;
        }
    }
    
    ESP_LOGI(TAG, "Scan complete: %d device%s found", devices_found, devices_found == 1 ? "" : "s");
    
    if (devices_found == 0) {
        ESP_LOGW(TAG, "No devices detected! Check:");
        ESP_LOGW(TAG, "  - I2C connections (SDA/SCL)");
        ESP_LOGW(TAG, "  - Board power (12V)");
        ESP_LOGW(TAG, "  - Solder joints on I2C bus");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    OTS I2C Bus Scan Test              ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    
    i2c_master_init();
    
    while (1) {
        i2c_scan();
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Next scan in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
