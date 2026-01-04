/**
 * @file i2c.c
 * @brief I2C Bus Hardware Abstraction Layer Implementation
 */

#include "i2c.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t bus_handle = NULL;

esp_err_t i2c_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C master bus @ %d Hz", I2C_FREQ_HZ);

    // New I2C master bus configuration
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus creation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C initialized on SDA=%d SCL=%d", I2C_SDA_IO, I2C_SCL_IO);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_get_bus_handle(void)
{
    return bus_handle;
}
