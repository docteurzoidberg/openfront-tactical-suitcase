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
        // For TJA1050 transceiver: Use NO_ACK mode for loopback testing (single node)
        // Use NORMAL mode only when bus is properly terminated (120Ω at both ends)
        twai_mode_t selected_mode = s_driver.config.loopback ? TWAI_MODE_NO_ACK : TWAI_MODE_NORMAL;
        ESP_LOGI(TAG, "Selected TWAI mode: %d (%s)", selected_mode, 
                 selected_mode == TWAI_MODE_NO_ACK ? "NO_ACK" : 
                 selected_mode == TWAI_MODE_NORMAL ? "NORMAL" : "OTHER");
        
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
            s_driver.config.tx_gpio, 
            s_driver.config.rx_gpio,
            selected_mode
        );
        
        ESP_LOGI(TAG, "TWAI g_config.mode after init: %d", g_config.mode);
        
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
 * @brief Log detailed TWAI peripheral status for debugging
 */
void can_driver_log_twai_status(void) {
    if (!s_driver.initialized || s_driver.mock_mode) {
        ESP_LOGI(TAG, "TWAI status: Not available (mock mode or not initialized)");
        return;
    }
    
#if CONFIG_CAN_DRIVER_USE_TWAI
    twai_status_info_t status;
    esp_err_t ret = twai_get_status_info(&status);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get TWAI status: %s", esp_err_to_name(ret));
        return;
    }
    
    const char *state_str = "UNKNOWN";
    switch (status.state) {
        case TWAI_STATE_STOPPED: state_str = "STOPPED"; break;
        case TWAI_STATE_RUNNING: state_str = "RUNNING"; break;
        case TWAI_STATE_BUS_OFF: state_str = "BUS_OFF"; break;
        case TWAI_STATE_RECOVERING: state_str = "RECOVERING"; break;
    }
    
    ESP_LOGI(TAG, "=== TWAI Peripheral Status ===");
    ESP_LOGI(TAG, "  State: %s", state_str);
    ESP_LOGI(TAG, "  TX Error Counter: %lu (BUS_OFF at 256)", (unsigned long)status.tx_error_counter);
    ESP_LOGI(TAG, "  RX Error Counter: %lu (BUS_OFF at 256)", (unsigned long)status.rx_error_counter);
    ESP_LOGI(TAG, "  TX Queue: %lu msgs waiting", (unsigned long)status.msgs_to_tx);
    ESP_LOGI(TAG, "  RX Queue: %lu msgs waiting", (unsigned long)status.msgs_to_rx);
    ESP_LOGI(TAG, "  TX Failed: %lu", (unsigned long)status.tx_failed_count);
    ESP_LOGI(TAG, "  RX Missed: %lu", (unsigned long)status.rx_missed_count);
    ESP_LOGI(TAG, "  RX Queue Full: %lu", (unsigned long)status.rx_overrun_count);
    ESP_LOGI(TAG, "  Bus Errors: %lu", (unsigned long)status.bus_error_count);
    ESP_LOGI(TAG, "  Arbitration Lost: %lu", (unsigned long)status.arb_lost_count);
#endif
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
        ESP_LOGI(TAG, "✓ TX: ID=0x%03X DLC=%d", frame->id, frame->dlc);
    } else if (ret == ESP_ERR_TIMEOUT) {
        s_driver.tx_errors++;
        ESP_LOGW(TAG, "✗ TX timeout (bus busy or not connected): ID=0x%03X", frame->id);
    } else {
        s_driver.tx_errors++;
        ESP_LOGW(TAG, "✗ TX failed: %s (ID=0x%03X)", esp_err_to_name(ret), frame->id);
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
        ESP_LOGD(TAG, "can_driver_receive: Mock mode, returning timeout");
        return ESP_ERR_TIMEOUT;
    }
    
#if CONFIG_CAN_DRIVER_USE_TWAI
    // Physical mode: Receive via TWAI
    ESP_LOGD(TAG, "→ can_driver_receive: timeout=%lu ms", (unsigned long)timeout_ms);
    
    twai_message_t msg;
    esp_err_t ret = twai_receive(&msg, pdMS_TO_TICKS(timeout_ms));
    
    ESP_LOGD(TAG, "  twai_receive returned: %s", esp_err_to_name(ret));
    
    if (ret == ESP_OK) {
        frame->id = msg.identifier;
        frame->dlc = msg.data_length_code;
        frame->rtr = msg.rtr ? true : false;
        frame->extended = msg.extd ? true : false;
        memcpy(frame->data, msg.data, 8);
        
        s_driver.rx_count++;
        
        ESP_LOGI(TAG, "✓ RX: ID=0x%03X DLC=%d RTR=%d EXT=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                 frame->id, frame->dlc, frame->rtr, frame->extended,
                 frame->data[0], frame->data[1], frame->data[2], frame->data[3],
                 frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
    } else if (ret == ESP_ERR_TIMEOUT) {
        // Timeout is normal, not an error
        ESP_LOGD(TAG, "  (timeout - no message available)");
    } else {
        s_driver.rx_errors++;
        ESP_LOGW(TAG, "✗ RX error: %s", esp_err_to_name(ret));
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

/**
 * @brief Recover from BUS_OFF state
 */
esp_err_t can_driver_recover(void) {
    if (!s_driver.initialized) {
        ESP_LOGE(TAG, "Driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_driver.mock_mode) {
        ESP_LOGI(TAG, "Recovery (mock mode - no action needed)");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initiating TWAI recovery from BUS_OFF...");
    esp_err_t ret = twai_initiate_recovery();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initiate recovery: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Recovery initiated successfully");
    return ESP_OK;
}

/**
 * @brief Start the TWAI peripheral
 */
esp_err_t can_driver_start(void) {
    if (!s_driver.initialized) {
        ESP_LOGE(TAG, "Driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_driver.mock_mode) {
        ESP_LOGI(TAG, "Start (mock mode - no action needed)");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting TWAI driver...");
    esp_err_t ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ TWAI driver started");
    return ESP_OK;
}

/**
 * @brief Stop the TWAI peripheral
 */
esp_err_t can_driver_stop(void) {
    if (!s_driver.initialized) {
        ESP_LOGE(TAG, "Driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_driver.mock_mode) {
        ESP_LOGI(TAG, "Stop (mock mode - no action needed)");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping TWAI driver...");
    esp_err_t ret = twai_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop TWAI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ TWAI driver stopped");
    return ESP_OK;
}
