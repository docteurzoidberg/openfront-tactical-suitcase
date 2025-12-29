#include "io_expander.h"
#include "config.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "OTS_IO_EXP";

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
    io_expander_health_t health;
} mcp23017_board_t;

static mcp23017_board_t boards[MAX_MCP_BOARDS];
static uint8_t board_count = 0;
static bool io_initialized = false;
static i2c_master_bus_handle_t i2c_bus = NULL;
static io_expander_recovery_callback_t recovery_callback = NULL;

// Forward declarations
static void record_error(uint8_t board);
static void record_success(uint8_t board);
static esp_err_t init_single_board(uint8_t board_idx, uint8_t address);

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

// Record error for a board
static void record_error(uint8_t board) {
    if (board >= MAX_MCP_BOARDS) return;
    
    boards[board].health.error_count++;
    boards[board].health.consecutive_errors++;
    boards[board].health.last_error_time = esp_timer_get_time() / 1000;
    
    if (boards[board].health.consecutive_errors >= IO_EXPANDER_MAX_CONSECUTIVE_ERRORS) {
        boards[board].health.healthy = false;
        ESP_LOGW(TAG, "Board #%d marked unhealthy (%lu consecutive errors)",
                 board, (unsigned long)boards[board].health.consecutive_errors);
    }
}

// Record successful operation
static void record_success(uint8_t board) {
    if (board >= MAX_MCP_BOARDS) return;
    
    bool was_unhealthy = !boards[board].health.healthy;
    
    boards[board].health.consecutive_errors = 0;
    boards[board].health.healthy = true;
    
    if (was_unhealthy) {
        boards[board].health.recovery_count++;
        ESP_LOGI(TAG, "Board #%d recovered (recovery count: %lu)",
                 board, (unsigned long)boards[board].health.recovery_count);
        
        if (recovery_callback) {
            recovery_callback(board, true);
        }
    }
}

// Initialize a single board with retry logic
static esp_err_t init_single_board(uint8_t board_idx, uint8_t address) {
    esp_err_t ret = ESP_FAIL;
    uint32_t retry_delay = IO_EXPANDER_INITIAL_RETRY_DELAY_MS;
    
    for (int retry = 0; retry < IO_EXPANDER_MAX_RETRIES; retry++) {
        if (retry > 0) {
            ESP_LOGW(TAG, "Retry #%d for board 0x%02X (delay: %lums)",
                     retry, address, (unsigned long)retry_delay);
            vTaskDelay(pdMS_TO_TICKS(retry_delay));
            
            // Exponential backoff
            retry_delay *= 2;
            if (retry_delay > IO_EXPANDER_MAX_RETRY_DELAY_MS) {
                retry_delay = IO_EXPANDER_MAX_RETRY_DELAY_MS;
            }
        }
        
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 100000,
        };
        
        ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &boards[board_idx].handle);
        if (ret == ESP_OK) {
            // Test communication with a read
            uint8_t test_value;
            ret = mcp23017_read_reg(boards[board_idx].handle, MCP23017_IODIRA, &test_value);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Board #%d initialized at 0x%02X (attempt %d)",
                         board_idx, address, retry + 1);
                return ESP_OK;
            }
            
            // Failed read, remove device
            i2c_master_bus_rm_device(boards[board_idx].handle);
            boards[board_idx].handle = NULL;
        }
    }
    
    ESP_LOGE(TAG, "Board #%d at 0x%02X failed after %d retries",
             board_idx, address, IO_EXPANDER_MAX_RETRIES);
    return ret;
}

bool io_expander_begin(const uint8_t *addresses, uint8_t count) {
    if (!addresses || count == 0 || count > MAX_MCP_BOARDS) {
        ESP_LOGE(TAG, "Invalid parameters (count=%d, max=%d)", count, MAX_MCP_BOARDS);
        return false;
    }

    ESP_LOGI(TAG, "Initializing %d MCP23017(s) with error recovery...", count);

    // Use shared I2C bus (owned by i2c_bus.c)
    esp_err_t ret = ots_i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize shared I2C bus: %s", esp_err_to_name(ret));
        return false;
    }
    i2c_bus = ots_i2c_bus_get();
    if (!i2c_bus) {
        ESP_LOGE(TAG, "Shared I2C bus handle is NULL");
        return false;
    }

    // Initialize board structures
    memset(boards, 0, sizeof(boards));
    board_count = 0;
    bool all_success = true;

    // Initialize each board with retry logic
    for (uint8_t i = 0; i < count; i++) {
        uint8_t addr = addresses[i];
        
        boards[i].address = addr;
        boards[i].health.initialized = false;
        boards[i].health.healthy = false;
        boards[i].health.last_health_check = esp_timer_get_time() / 1000;
        
        ret = init_single_board(i, addr);
        if (ret == ESP_OK) {
            boards[i].initialized = true;
            boards[i].health.initialized = true;
            boards[i].health.healthy = true;
            board_count++;
        } else {
            ESP_LOGE(TAG, "Board #%d at 0x%02X failed to initialize", i, addr);
            all_success = false;
        }
    }

    if (board_count == 0) {
        ESP_LOGE(TAG, "No boards initialized!");
        io_initialized = false;
        return false;
    }

    io_initialized = true;
    ESP_LOGI(TAG, "Ready: %d/%d board(s) initialized successfully", board_count, count);
    
    if (!all_success) {
        ESP_LOGW(TAG, "Some boards failed - recovery available via io_expander_attempt_recovery()");
    }
    
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
        record_error(board);
        return false;
    }

    if (value) {
        current |= (1 << bit);
    } else {
        current &= ~(1 << bit);
    }

    ret = mcp23017_write_reg(boards[board].handle, reg, current);
    if (ret != ESP_OK) {
        record_error(board);
        return false;
    }
    
    record_success(board);
    return true;
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
        record_error(board);
        return false;
    }

    *value = (current & (1 << bit)) ? 1 : 0;
    record_success(board);
    return true;
}

bool io_expander_is_initialized(void) {
    return io_initialized;
}

bool io_expander_is_board_present(uint8_t board) {
    if (board >= MAX_MCP_BOARDS || board >= board_count) {
        return false;
    }
    return boards[board].initialized && boards[board].health.healthy;
}

uint8_t io_expander_get_board_count(void) {
    return board_count;
}

esp_err_t io_expander_reinit_board(uint8_t board) {
    if (board >= MAX_MCP_BOARDS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Attempting to reinitialize board #%d (0x%02X)...",
             board, boards[board].address);
    
    // Remove old device if exists
    if (boards[board].handle) {
        i2c_master_bus_rm_device(boards[board].handle);
        boards[board].handle = NULL;
    }
    
    boards[board].initialized = false;
    boards[board].health.healthy = false;
    
    // Attempt reinit with retry logic
    esp_err_t ret = init_single_board(board, boards[board].address);
    if (ret == ESP_OK) {
        boards[board].initialized = true;
        boards[board].health.initialized = true;
        boards[board].health.healthy = true;
        boards[board].health.consecutive_errors = 0;
        boards[board].health.recovery_count++;
        
        if (recovery_callback) {
            recovery_callback(board, true);
        }
        
        ESP_LOGI(TAG, "Board #%d successfully reinitialized", board);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to reinitialize board #%d", board);
    return ret;
}

bool io_expander_health_check(void) {
    if (!io_initialized) {
        return false;
    }
    
    bool all_healthy = true;
    uint64_t now = esp_timer_get_time() / 1000;
    
    for (uint8_t i = 0; i < board_count; i++) {
        if (!boards[i].initialized) {
            all_healthy = false;
            continue;
        }
        
        // Skip if checked recently
        if (now - boards[i].health.last_health_check < IO_EXPANDER_HEALTH_CHECK_INTERVAL_MS) {
            if (!boards[i].health.healthy) {
                all_healthy = false;
            }
            continue;
        }
        
        boards[i].health.last_health_check = now;
        
        // Test read from IODIRA register
        uint8_t test_value;
        esp_err_t ret = mcp23017_read_reg(boards[i].handle, MCP23017_IODIRA, &test_value);
        
        if (ret == ESP_OK) {
            record_success(i);
        } else {
            record_error(i);
            all_healthy = false;
            ESP_LOGW(TAG, "Health check failed for board #%d (0x%02X)",
                     i, boards[i].address);
        }
    }
    
    return all_healthy;
}

bool io_expander_get_health(uint8_t board, io_expander_health_t *status) {
    if (board >= MAX_MCP_BOARDS || !status) {
        return false;
    }
    
    if (!boards[board].initialized) {
        memset(status, 0, sizeof(io_expander_health_t));
        return false;
    }
    
    memcpy(status, &boards[board].health, sizeof(io_expander_health_t));
    return true;
}

void io_expander_set_recovery_callback(io_expander_recovery_callback_t callback) {
    recovery_callback = callback;
    ESP_LOGI(TAG, "Recovery callback %s", callback ? "registered" : "cleared");
}

uint8_t io_expander_attempt_recovery(void) {
    if (!io_initialized) {
        return 0;
    }
    
    uint8_t recovered = 0;
    
    ESP_LOGI(TAG, "Attempting recovery for unhealthy boards...");
    
    for (uint8_t i = 0; i < MAX_MCP_BOARDS; i++) {
        if (!boards[i].initialized || boards[i].health.healthy) {
            continue;
        }
        
        ESP_LOGI(TAG, "Recovering board #%d (errors: %lu, consecutive: %lu)",
                 i, (unsigned long)boards[i].health.error_count,
                 (unsigned long)boards[i].health.consecutive_errors);
        
        if (io_expander_reinit_board(i) == ESP_OK) {
            recovered++;
        }
    }
    
    if (recovered > 0) {
        ESP_LOGI(TAG, "Recovered %d board(s)", recovered);
    } else {
        ESP_LOGW(TAG, "No boards recovered");
    }
    
    return recovered;
}

void io_expander_reset_errors(uint8_t board) {
    if (board >= MAX_MCP_BOARDS) {
        return;
    }
    
    boards[board].health.error_count = 0;
    boards[board].health.consecutive_errors = 0;
    boards[board].health.healthy = true;
    
    ESP_LOGI(TAG, "Error counters reset for board #%d", board);
}

i2c_master_bus_handle_t io_expander_get_bus(void) {
    return i2c_bus;
}
