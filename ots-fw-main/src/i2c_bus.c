#include "i2c_bus.h"
#include "config.h"
#include <esp_log.h>

static const char *TAG = "OTS_I2C_BUS";

static i2c_master_bus_handle_t s_bus = NULL;
static bool s_inited = false;

esp_err_t ots_i2c_bus_init(void) {
    if (s_inited && s_bus) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2C bus: %s", esp_err_to_name(ret));
        s_bus = NULL;
        s_inited = false;
        return ret;
    }

    s_inited = true;
    ESP_LOGI(TAG, "I2C bus initialized (SDA=%d SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);
    return ESP_OK;
}

i2c_master_bus_handle_t ots_i2c_bus_get(void) {
    return s_bus;
}
