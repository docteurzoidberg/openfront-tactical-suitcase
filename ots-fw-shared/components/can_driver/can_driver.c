#include "can_driver.h"
#include "driver/twai.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CAN_DRV";

// Driver state
typedef struct {
    bool initialized;
    bool mock_mode;
    can_config_t config;
    
    // Statistics
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t tx_errors;
    uint32_t rx_errors;
} can_driver_state_t;

static can_driver_state_t s_driver = {0};

/**
 * @brief Initialize CAN driver with automatic fallback to mock mode
 */
esp_err_t can_driver_init(const can_config_t *config) {
    if (s_driver.initialized) {
        ESP_LOGW(TAG, "CAN driver already initialized");
        return ESP_OK;
    }
    
    // Use default config if none provided
    if (config == NULL) {
        can_config_t default_config = CAN_CONFIG_DEFAULT();
        s_driver.config = default_config;
    } else {
        s_driver.config = *config;
    }
    
    s_driver.mock_mode = s_driver.config.mock_mode;
    
    if (s_driver.mock_mode) {
        ESP_LOGI(TAG, "Initializing CAN driver in MOCK mode (explicit)");
        ESP_LOGW(TAG, "Physical CAN bus disabled - frames will be logged only");
        ESP_LOGI(TAG, "Config: TX_GPIO=%d RX_GPIO=%d bitrate=%lu", 
                 s_driver.config.tx_gpio, s_driver.config.rx_gpio, 
                 (unsigned long)s_driver.config.bitrate);
    } else {
        ESP_LOGI(TAG, "Attempting to initialize CAN driver in PHYSICAL mode");
        ESP_LOGI(TAG, "Config: TX_GPIO=%d RX_GPIO=%d bitrate=%lu loopback=%d", 
                 s_driver.config.tx_gpio, s_driver.config.rx_gpio,
                 (unsigned long)s_driver.config.bitrate, s_driver.config.loopback);
        
        // Configure TWAI general settings
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
            s_driver.config.tx_gpio, 
            s_driver.config.rx_gpio,
            s_driver.config.loopback ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL
        );
        
        // Disable alerts we don't need to reduce interrupt overhead
        g_config.alerts_enabled = TWAI_ALERT_NONE;
        g_config.tx_queue_len = 5;
        g_config.rx_queue_len = 10;
        
        // Configure timing based on bitrate
        twai_timing_config_t t_config;
        if (s_driver.config.bitrate == 500000) {
            t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
        } else if (s_driver.config.bitrate == 250000) {
            t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
        } else if (s_driver.config.bitrate == 125000) {
            t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
        } else if (s_driver.config.bitrate == 1000000) {
            t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
        } else {
            ESP_LOGE(TAG, "Unsupported bitrate: %lu (use 125k/250k/500k/1M)", 
                     (unsigned long)s_driver.config.bitrate);
            ESP_LOGW(TAG, "Falling back to MOCK mode");
            s_driver.mock_mode = true;
            goto mock_fallback;
        }
        
        // Configure filter to accept all messages
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        
        // Attempt to install TWAI driver
        esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
            ESP_LOGW(TAG, "This is normal if no CAN transceiver hardware is present");
            ESP_LOGI(TAG, "Falling back to MOCK mode");
            s_driver.mock_mode = true;
            goto mock_fallback;
        }
        
        // Attempt to start TWAI driver
        ret = twai_start();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(ret));
            ESP_LOGW(TAG, "CAN bus may not be properly terminated or hardware missing");
            ESP_LOGI(TAG, "Falling back to MOCK mode");
            twai_driver_uninstall();
            s_driver.mock_mode = true;
            goto mock_fallback;
        }
        
        ESP_LOGI(TAG, "Physical CAN bus initialized successfully!");
        ESP_LOGI(TAG, "Mode: %s | Bitrate: %lu bps", 
                 s_driver.config.loopback ? "LOOPBACK" : "NORMAL",
                 (unsigned long)s_driver.config.bitrate);
    }
    
mock_fallback:
    if (s_driver.mock_mode) {
        ESP_LOGI(TAG, "CAN driver running in MOCK mode");
    }
    
    s_driver.initialized = true;
    s_driver.tx_count = 0;
    s_driver.rx_count = 0;
    s_driver.tx_errors = 0;
    s_driver.rx_errors = 0;
    
    ESP_LOGI(TAG, "CAN driver initialized successfully");
    return ESP_OK;
}

/**
 * @brief Deinitialize CAN driver
 */
esp_err_t can_driver_deinit(void) {
    if (!s_driver.initialized) {
        return ESP_OK;
    }
    
    if (!s_driver.mock_mode) {
        ESP_LOGI(TAG, "Stopping physical CAN bus...");
        esp_err_t ret = twai_stop();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "TWAI stop failed: %s", esp_err_to_name(ret));
        }
        
        ret = twai_driver_uninstall();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "TWAI uninstall failed: %s", esp_err_to_name(ret));
        }
    }
    
    s_driver.initialized = false;
    ESP_LOGI(TAG, "CAN driver deinitialized");
    return ESP_OK;
}

/**
 * @brief Check if CAN dri
 */
esp_err_t can_driver_send(const can_frame_t *frame) {
    if (!s_driver.initialized) {
        ESP_LOGE(TAG, "CAN driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!frame) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (frame->dlc > 8) {
        ESP_LOGE(TAG, "Invalid DLC: %d (max 8)", frame->dlc);
        s_driver.tx_errors++;
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_driver.mock_mode) {
        // Mock mode: Log the frame
        ESP_LOGI(TAG, "TX: ID=0x%03X DLC=%d RTR=%d EXT=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                 frame->id, frame->dlc, frame->rtr, frame->extended,
                 frame->data[0], frame->data[1], frame->data[2], frame->data[3],
                 frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
        
        s_driver.tx_count++;
        return ESP_OK;
    }
    
#if CONFIG_CAN_DRIVER_USE_TWAI
    // Physical mode: Send via TWAI
    twai_message_t msg = {0};
    msg.identifier = frame->id;
    msg.data_length_code = frame->dlc;
    msg.rtr = frame->rtr ? 1 : 0;
    msg.extd = frame->extended ? 1 : 0;
    memcpy(msg.data, frame->data, 8);
    
    // Try to transmit with 100ms timeout
    esp_err_t ret = twai_transmit(&msg, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        s_driver.tx_count++;
        ESP_LOGD(TAG, "TX: ID=0x%03X DLC=%d", frame->id, frame->dlc);
    } else if (ret == ESP_ERR_TIMEOUT) {
        s_driver.tx_errors++;
        ESP_LOGW(TAG, "TX timeout (bus busy or not connected)");
    } else {
        s_driver.tx_errors++;
        ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
#else
    ESP_LOGE(TAG, "Physical mode not implemented");
    s_driver.tx_errors++;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

/**
 */
esp_err_t can_driver_receive(can_frame_t *frame, uint32_t timeout_ms) {
    if (!s_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!frame) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_driver.mock_mode) {
        // Mock mode: No incoming frames
        return ESP_ERR_TIMEOUT;
    }
    
#if CONFIG_CAN_DRIVER_USE_TWAI
    // Physical mode: Receive via TWAI
    twai_message_t msg;
    esp_err_t ret = twai_receive(&msg, pdMS_TO_TICKS(timeout_ms));
    
    if (ret == ESP_OK) {
        frame->id = msg.identifier;
        frame->dlc = msg.data_length_code;
        frame->rtr = msg.rtr ? true : false;
        frame->extended = msg.extd ? true : false;
        memcpy(frame->data, msg.data, 8);
        
        s_driver.rx_count++;
        
        ESP_LOGI(TAG, "RX: ID=0x%03X DLC=%d RTR=%d EXT=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                 frame->id, frame->dlc, frame->rtr, frame->extended,
                 frame->data[0], frame->data[1], frame->data[2], frame->data[3],
                 frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
    } else if (ret == ESP_ERR_TIMEOUT) {
        // Timeout is normal, not an error
    } else {
        s_driver.rx_errors++;
        ESP_LOGW(TAG, "RX error: %s", esp_err_to_name(ret));
    }
    
    return ret;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

/**
 * @brief Get number of frames available in RX queue
 */
uint32_t can_driver_rx_available(void) {
    if (!s_driver.initialized || s_driver.mock_mode) {
        return 0;
    }
#if CONFIG_CAN_DRIVER_USE_TWAI
    // Query TWAI RX queue depth
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
        return status.msgs_to_rx;
    }
#endif
    
    return 0;
}

/**
 * @brief Get CAN bus statistics
 */
esp_err_t can_driver_get_stats(uint32_t *tx_count, uint32_t *rx_count,
                               uint32_t *tx_errors, uint32_t *rx_errors) {
    if (!s_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (tx_count) *tx_count = s_driver.tx_count;
    if (rx_count) *rx_count = s_driver.rx_count;
    if (tx_errors) *tx_errors = s_driver.tx_errors;
    if (rx_errors) *rx_errors = s_driver.rx_errors;
    
    return ESP_OK;
}

/**
 * @brief Reset CAN bus statistics
 */
void can_driver_reset_stats(void) {
    s_driver.tx_count = 0;
    s_driver.rx_count = 0;
    s_driver.tx_errors = 0;
    s_driver.rx_errors = 0;
}

/**
 * @brief Set CAN RX filter
 */
esp_err_t can_driver_set_filter(uint16_t filter_id, uint16_t filter_mask) {
    if (!s_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_driver.mock_mode) {
        ESP_LOGI(TAG, "Filter set (mock): ID=0x%03X MASK=0x%03X", filter_id, filter_mask);
        return ESP_OK;
    }
    
    // Dynamic filter reconfiguration requires stopping and restarting TWAI driver
    // For most applications, ACCEPT_ALL filter set during init is sufficient
    ESP_LOGW(TAG, "Dynamic filter configuration requires driver restart");
    ESP_LOGW(TAG, "Consider using acceptance filtering in application layer");
    return ESP_ERR_NOT_SUPPORTED;
}
