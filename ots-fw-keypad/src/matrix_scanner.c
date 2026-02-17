/**
 * @file matrix_scanner.c
 * @brief Matrix Scanner Module Implementation
 * 
 * Scans 3×7 key matrix at 200Hz with debouncing and event generation.
 */

#include "matrix_scanner.h"
#include "config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "MATRIX";

// Private data structures
typedef struct {
    key_state_t current_state;      // Current stable state
    key_state_t raw_state;          // Last raw scan result
    uint32_t debounce_timer;        // Timestamp for debounce (ms)
    bool debounce_active;           // Debouncing in progress
} key_state_internal_t;

typedef struct {
    key_state_internal_t keys[NUM_KEYS];  // State for all 15 keys
    key_event_callback_t callback;         // Event callback
    TaskHandle_t scan_task;                // FreeRTOS task handle
    bool running;                          // Scanner active flag
    bool initialized;                      // Init complete flag
} matrix_scanner_ctx_t;

// Global context
static matrix_scanner_ctx_t s_ctx = {0};

// GPIO pin arrays
static const gpio_num_t row_pins[MATRIX_ROWS] = {GPIO_ROW0, GPIO_ROW1, GPIO_ROW2};
static const gpio_num_t col_pins[MATRIX_COLS] = {
    GPIO_COL0, GPIO_COL1, GPIO_COL2, GPIO_COL3,
    GPIO_COL4, GPIO_COL5, GPIO_COL6
};

/**
 * @brief Get current time in milliseconds
 */
static inline uint32_t get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/**
 * @brief Convert matrix (row, col) to key_id (1-15)
 * 
 * Key mapping:
 * Row0: K1-K7 (cols 0-6)
 * Row1: K8-K14 (cols 0-6)
 * Row2: K15 (col 3 only, spacebar)
 */
static uint8_t matrix_to_key_id(uint8_t row, uint8_t col) {
    // Special case: Row 2, Col 3 = K15 (spacebar)
    if (row == 2 && col == 3) {
        return 15;
    }
    
    // Row 2 other positions are unused
    if (row == 2) {
        return 0;  // Invalid key ID
    }
    
    // Sequential mapping for rows 0 and 1
    return (row * MATRIX_COLS) + col + 1;
}

/**
 * @brief Configure GPIO pins for matrix scanning
 */
static esp_err_t configure_gpio(void) {
    esp_err_t ret;
    
    // Configure row pins as outputs (initially high-Z)
    for (int i = 0; i < MATRIX_ROWS; i++) {
        gpio_config_t row_conf = {
            .pin_bit_mask = (1ULL << row_pins[i]),
            .mode = GPIO_MODE_INPUT,  // Start as input (high-Z)
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        ret = gpio_config(&row_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure row pin %d", row_pins[i]);
            return ret;
        }
    }
    
    // Configure column pins as inputs with pull-ups
    for (int i = 0; i < MATRIX_COLS; i++) {
        gpio_config_t col_conf = {
            .pin_bit_mask = (1ULL << col_pins[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        ret = gpio_config(&col_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure col pin %d", col_pins[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "GPIO configured: %d rows, %d cols", MATRIX_ROWS, MATRIX_COLS);
    return ESP_OK;
}

/**
 * @brief Scan matrix once and update raw states
 * 
 * @param row_states Output: 2D array of raw key states [row][col]
 */
static void scan_matrix_once(uint8_t row_states[MATRIX_ROWS][MATRIX_COLS]) {
    for (int row = 0; row < MATRIX_ROWS; row++) {
        // Set all rows to high-Z
        for (int r = 0; r < MATRIX_ROWS; r++) {
            gpio_set_direction(row_pins[r], GPIO_MODE_INPUT);
        }
        
        // Set current row to output LOW
        gpio_set_direction(row_pins[row], GPIO_MODE_OUTPUT);
        gpio_set_level(row_pins[row], 0);
        
        // Small delay for signal to settle
        esp_rom_delay_us(10);
        
        // Read all column pins
        for (int col = 0; col < MATRIX_COLS; col++) {
            int level = gpio_get_level(col_pins[col]);
            row_states[row][col] = (level == 0) ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
        }
    }
    
    // Return all rows to high-Z
    for (int r = 0; r < MATRIX_ROWS; r++) {
        gpio_set_direction(row_pins[r], GPIO_MODE_INPUT);
    }
}

/**
 * @brief Process key state with debouncing
 */
static void process_key_state(uint8_t key_id, key_state_t raw_state, uint32_t now_ms) {
    if (key_id == 0 || key_id > NUM_KEYS) {
        return;  // Invalid key ID
    }
    
    key_state_internal_t *key = &s_ctx.keys[key_id - 1];
    
    // Check if raw state changed
    if (raw_state != key->raw_state) {
        // Raw state changed - start/restart debounce timer
        key->raw_state = raw_state;
        key->debounce_timer = now_ms;
        key->debounce_active = true;
        return;
    }
    
    // Raw state stable
    if (key->debounce_active) {
        // Check if debounce time elapsed
        uint32_t elapsed = now_ms - key->debounce_timer;
        if (elapsed >= KEYPAD_DEBOUNCE_MS) {
            // Debounce complete - check if stable state changed
            if (key->raw_state != key->current_state) {
                key->current_state = key->raw_state;
                key->debounce_active = false;
                
                // Trigger callback
                if (s_ctx.callback) {
                    s_ctx.callback(key_id, key->current_state);
                }
                
                ESP_LOGD(TAG, "Key K%d %s", key_id,
                         key->current_state == KEY_STATE_PRESSED ? "PRESSED" : "RELEASED");
            } else {
                key->debounce_active = false;
            }
        }
    }
}

/**
 * @brief Matrix scanning task
 */
static void matrix_scanner_task(void *arg) {
    uint8_t row_states[MATRIX_ROWS][MATRIX_COLS] = {0};
    
    ESP_LOGI(TAG, "Scanner task started (200Hz)");
    
    while (s_ctx.running) {
        uint32_t now_ms = get_time_ms();
        
        // Scan matrix
        scan_matrix_once(row_states);
        
        // Process all keys
        for (int row = 0; row < MATRIX_ROWS; row++) {
            for (int col = 0; col < MATRIX_COLS; col++) {
                uint8_t key_id = matrix_to_key_id(row, col);
                if (key_id > 0) {
                    process_key_state(key_id, row_states[row][col], now_ms);
                }
            }
        }
        
        // Delay for 5ms (200Hz scan rate)
        vTaskDelay(pdMS_TO_TICKS(KEYPAD_SCAN_RATE_MS));
    }
    
    ESP_LOGI(TAG, "Scanner task stopped");
    s_ctx.scan_task = NULL;
    vTaskDelete(NULL);
}

// Public API implementation

esp_err_t matrix_scanner_init(key_event_callback_t callback) {
    if (s_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    if (!callback) {
        ESP_LOGE(TAG, "Callback required");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize context
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.callback = callback;
    
    // Initialize all keys to released state
    for (int i = 0; i < NUM_KEYS; i++) {
        s_ctx.keys[i].current_state = KEY_STATE_RELEASED;
        s_ctx.keys[i].raw_state = KEY_STATE_RELEASED;
    }
    
    // Configure GPIO
    esp_err_t ret = configure_gpio();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_ctx.initialized = true;
    ESP_LOGI(TAG, "Initialized (%d keys, %dms debounce)", NUM_KEYS, KEYPAD_DEBOUNCE_MS);
    return ESP_OK;
}

esp_err_t matrix_scanner_start(void) {
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_ctx.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    s_ctx.running = true;
    
    // Create scanning task
    BaseType_t ret = xTaskCreate(
        matrix_scanner_task,
        "matrix_scan",
        2048,                           // Stack size
        NULL,                           // Parameters
        tskIDLE_PRIORITY + 2,          // Priority
        &s_ctx.scan_task
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scanner task");
        s_ctx.running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Scanner started");
    return ESP_OK;
}

esp_err_t matrix_scanner_stop(void) {
    if (!s_ctx.running) {
        return ESP_OK;
    }
    
    s_ctx.running = false;
    
    // Wait for task to exit (max 100ms)
    for (int i = 0; i < 20 && s_ctx.scan_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    if (s_ctx.scan_task != NULL) {
        ESP_LOGW(TAG, "Task did not exit cleanly");
    }
    
    ESP_LOGI(TAG, "Scanner stopped");
    return ESP_OK;
}

key_state_t matrix_scanner_get_key_state(uint8_t key_id) {
    if (key_id == 0 || key_id > NUM_KEYS) {
        return KEY_STATE_RELEASED;
    }
    
    return s_ctx.keys[key_id - 1].current_state;
}
