#include "io_expander.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>

static const char *TAG = "IO_EXPANDER";

// MCP23017 Register addresses
#define MCP23017_IODIRA   0x00
#define MCP23017_IODIRB   0x01
#define MCP23017_GPIOA    0x12
#define MCP23017_GPIOB    0x13
#define MCP23017_OLATA    0x14
#define MCP23017_OLATB    0x15
#define MCP23017_GPPUA    0x0C
#define MCP23017_GPPUB    0x0D

typedef struct {
    i2c_master_dev_handle_t handle;
    uint8_t address;
    bool initialized;
} mcp23017_board_t;

static mcp23017_board_t boards[MAX_MCP_BOARDS];
static uint8_t board_count = 0;
static bool io_initialized = false;
static i2c_master_bus_handle_t i2c_bus = NULL;

// Write single register
static esp_err_t mcp23017_write_reg(i2c_master_dev_handle_t handle, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(handle, data, 2, 1000 / portTICK_PERIOD_MS);
}

// Read single register
static esp_err_t mcp23017_read_reg(i2c_master_dev_handle_t handle, uint8_t reg, uint8_t *value) {
    esp_err_t ret = i2c_master_transmit(handle, &reg, 1, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(handle, value, 1, 1000 / portTICK_PERIOD_MS);
}

bool io_expander_begin(const uint8_t *addresses, uint8_t count) {
    if (!addresses || count == 0 || count > MAX_MCP_BOARDS) {
        ESP_LOGE(TAG, "Invalid parameters (count=%d, max=%d)", count, MAX_MCP_BOARDS);
        return false;
    }

    ESP_LOGI(TAG, "Initializing %d MCP23017(s)...", count);

    // Initialize I2C bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return false;
    }

    board_count = 0;
    bool all_success = true;

    // Initialize each board
    for (uint8_t i = 0; i < count; i++) {
        uint8_t addr = addresses[i];

        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &boards[i].handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Board #%d failed at 0x%02X: %s", i, addr, esp_err_to_name(ret));
            all_success = false;
            continue;
        }

        boards[i].address = addr;
        boards[i].initialized = true;
        board_count++;
        ESP_LOGI(TAG, "Board #%d ready at 0x%02X", i, addr);
    }

    if (board_count == 0) {
        ESP_LOGE(TAG, "No boards initialized!");
        io_initialized = false;
        return false;
    }

    io_initialized = true;
    ESP_LOGI(TAG, "Ready: %d/%d board(s)", board_count, count);
    return all_success;
}

bool io_expander_set_pin_mode(uint8_t board, uint8_t pin, io_mode_t mode) {
    if (board >= board_count || !boards[board].initialized) {
        ESP_LOGW(TAG, "Invalid board: %d", board);
        return false;
    }
    if (pin >= 16) {
        ESP_LOGW(TAG, "Invalid pin: %d", pin);
        return false;
    }

    uint8_t reg = (pin < 8) ? MCP23017_IODIRA : MCP23017_IODIRB;
    uint8_t bit = pin % 8;
    uint8_t current;

    esp_err_t ret = mcp23017_read_reg(boards[board].handle, reg, &current);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read IODIR: %s", esp_err_to_name(ret));
        return false;
    }

    if (mode == IO_MODE_INPUT || mode == IO_MODE_INPUT_PULLUP) {
        current |= (1 << bit);  // Set bit for input
    } else {
        current &= ~(1 << bit); // Clear bit for output
    }

    ret = mcp23017_write_reg(boards[board].handle, reg, current);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write IODIR: %s", esp_err_to_name(ret));
        return false;
    }

    // Enable pull-up if requested
    if (mode == IO_MODE_INPUT_PULLUP) {
        uint8_t pullup_reg = (pin < 8) ? MCP23017_GPPUA : MCP23017_GPPUB;
        ret = mcp23017_read_reg(boards[board].handle, pullup_reg, &current);
        if (ret == ESP_OK) {
            current |= (1 << bit);
            mcp23017_write_reg(boards[board].handle, pullup_reg, current);
        }
    }

    return true;
}

bool io_expander_digital_write(uint8_t board, uint8_t pin, uint8_t value) {
    if (board >= board_count || !boards[board].initialized) {
        return false;
    }
    if (pin >= 16) {
        return false;
    }

    uint8_t reg = (pin < 8) ? MCP23017_OLATA : MCP23017_OLATB;
    uint8_t bit = pin % 8;
    uint8_t current;

    esp_err_t ret = mcp23017_read_reg(boards[board].handle, reg, &current);
    if (ret != ESP_OK) {
        return false;
    }

    if (value) {
        current |= (1 << bit);
    } else {
        current &= ~(1 << bit);
    }

    ret = mcp23017_write_reg(boards[board].handle, reg, current);
    return (ret == ESP_OK);
}

bool io_expander_digital_read(uint8_t board, uint8_t pin, uint8_t *value) {
    if (board >= board_count || !boards[board].initialized) {
        return false;
    }
    if (pin >= 16 || !value) {
        return false;
    }

    uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
    uint8_t bit = pin % 8;
    uint8_t current;

    esp_err_t ret = mcp23017_read_reg(boards[board].handle, reg, &current);
    if (ret != ESP_OK) {
        return false;
    }

    *value = (current & (1 << bit)) ? 1 : 0;
    return true;
}

bool io_expander_is_initialized(void) {
    return io_initialized;
}

uint8_t io_expander_get_board_count(void) {
    return board_count;
}
