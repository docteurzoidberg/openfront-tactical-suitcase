/**
 * @file i2c_scan.c
 * @brief I2C Bus Scanner Utility
 */

#include "i2c_scan.h"
#include "i2c.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCAN";

void i2c_scan_bus(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    ESP_LOGI(TAG, "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
    
    int found_count = 0;
    i2c_master_bus_handle_t bus_handle = i2c_get_bus_handle();
    
    if (!bus_handle) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return;
    }
    
    for (int addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) {
            printf("%02x: ", addr);
        }
        
        // Try to probe the device by reading 1 byte
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };
        
        i2c_master_dev_handle_t dev_handle;
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
        
        if (ret == ESP_OK) {
            // Try to read 1 byte to detect device presence
            uint8_t data;
            ret = i2c_master_receive(dev_handle, &data, 1, 100);
            
            if (ret == ESP_OK) {
                printf("%02x ", addr);
                found_count++;
                ESP_LOGI(TAG, "Device found at 0x%02x", addr);
            } else {
                printf("-- ");
            }
            
            i2c_master_bus_rm_device(dev_handle);
        } else {
            printf("-- ");
        }
        
        if ((addr + 1) % 16 == 0) {
            printf("\n");
        }
    }
    
    ESP_LOGI(TAG, "Scan complete. Found %d device(s)", found_count);
}
