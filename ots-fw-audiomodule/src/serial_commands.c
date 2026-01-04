/**
 * @file serial_commands.c
 * @brief Serial UART Command Processing Implementation
 */

#include "serial_commands.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SERIAL_CMD";

// Module state
static const serial_command_entry_t *s_command_table = NULL;
static size_t s_command_table_len = 0;
static play_wav_callback_t s_play_callback = NULL;

/**
 * @brief Trim newline characters from end of string
 */
static void trim_newline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

esp_err_t serial_commands_init(const serial_command_entry_t *command_table,
                               size_t table_len,
                               play_wav_callback_t play_callback)
{
    if (!command_table || table_len == 0 || !play_callback) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    s_command_table = command_table;
    s_command_table_len = table_len;
    s_play_callback = play_callback;
    
    ESP_LOGI(TAG, "Initialized with %d commands", table_len);
    return ESP_OK;
}

esp_err_t serial_commands_handle(const char *cmd)
{
    if (!s_command_table || !s_play_callback) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Command: '%s'", cmd);

    for (size_t i = 0; i < s_command_table_len; ++i) {
        if (strcmp(cmd, s_command_table[i].cmd) == 0) {
            ESP_LOGI(TAG, "-> play '%s'", s_command_table[i].filename);
            
            esp_err_t ret = s_play_callback(s_command_table[i].filename);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Play failed: %s", esp_err_to_name(ret));
                return ret;
            }
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "Unknown command: '%s'", cmd);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Serial command processing task
 */
static void serial_command_task(void *arg)
{
    char line[128];
    
    ESP_LOGI(TAG, "Serial command task started");

    while (1) {
        // Wait for input without printing prompt
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        trim_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        serial_commands_handle(line);
    }
}

esp_err_t serial_commands_start_task(void)
{
    if (!s_command_table || !s_play_callback) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    BaseType_t ret = xTaskCreate(serial_command_task,
                                 "serial_cmd",
                                 4096,
                                 NULL,
                                 5,
                                 NULL);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Task started");
    return ESP_OK;
}
