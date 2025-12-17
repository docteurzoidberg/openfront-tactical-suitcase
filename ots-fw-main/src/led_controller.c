#include "led_controller.h"
#include "module_io.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "LED_CTRL";

#define LED_COMMAND_QUEUE_SIZE 16
#define LED_TASK_STACK_SIZE 3072

// LED state tracking
typedef struct {
    led_effect_t effect;
    uint32_t effect_end_time;    // Tick count when effect should end (0 = infinite)
    uint32_t blink_rate_ms;
    uint32_t last_blink_time;
    bool current_state;
} led_state_t;

static led_state_t nuke_led_states[3] = {0};
static led_state_t alert_led_states[6] = {0};
static led_state_t link_led_state = {0};

static QueueHandle_t led_command_queue = NULL;
static TaskHandle_t led_task_handle = NULL;

// Forward declarations
static void led_controller_task(void *pvParameters);
static void update_led_state(led_state_t *state, led_type_t type, uint8_t index);
static void apply_led_command(const led_command_t *cmd);

esp_err_t led_controller_init(void) {
    ESP_LOGI(TAG, "Initializing LED controller...");
    
    // Create command queue
    led_command_queue = xQueueCreate(LED_COMMAND_QUEUE_SIZE, sizeof(led_command_t));
    if (led_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create LED command queue");
        return ESP_FAIL;
    }
    
    // Initialize all LED states to OFF
    for (int i = 0; i < 3; i++) {
        nuke_led_states[i].effect = LED_EFFECT_OFF;
        nuke_led_states[i].current_state = false;
        nuke_led_states[i].blink_rate_ms = LED_BLINK_INTERVAL_MS;
    }
    
    for (int i = 0; i < 6; i++) {
        alert_led_states[i].effect = LED_EFFECT_OFF;
        alert_led_states[i].current_state = false;
        alert_led_states[i].blink_rate_ms = LED_BLINK_INTERVAL_MS;
    }
    
    link_led_state.effect = LED_EFFECT_OFF;
    link_led_state.current_state = false;
    
    // Create LED controller task
    BaseType_t result = xTaskCreate(
        led_controller_task,
        "led_ctrl",
        LED_TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY_LED_BLINK,
        &led_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED controller task");
        vQueueDelete(led_command_queue);
        led_command_queue = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LED controller initialized");
    return ESP_OK;
}

bool led_controller_send_command(const led_command_t *cmd) {
    if (!cmd || !led_command_queue) {
        return false;
    }
    
    return xQueueSend(led_command_queue, cmd, 0) == pdTRUE;
}

bool led_controller_nuke_blink(uint8_t index, uint32_t duration_ms) {
    if (index > 2) {
        return false;
    }
    
    led_command_t cmd = {
        .type = LED_TYPE_NUKE,
        .index = index,
        .effect = LED_EFFECT_BLINK_TIMED,
        .duration_ms = duration_ms,
        .blink_rate_ms = LED_BLINK_INTERVAL_MS
    };
    
    return led_controller_send_command(&cmd);
}

bool led_controller_alert_on(uint8_t index, uint32_t duration_ms) {
    if (index > 5) {
        return false;
    }
    
    led_command_t cmd = {
        .type = LED_TYPE_ALERT,
        .index = index,
        .effect = duration_ms > 0 ? LED_EFFECT_BLINK_TIMED : LED_EFFECT_ON,
        .duration_ms = duration_ms,
        .blink_rate_ms = 0  // Solid on, not blinking
    };
    
    return led_controller_send_command(&cmd);
}

bool led_controller_link_set(bool on) {
    led_command_t cmd = {
        .type = LED_TYPE_LINK,
        .index = 0,
        .effect = on ? LED_EFFECT_ON : LED_EFFECT_OFF,
        .duration_ms = 0,
        .blink_rate_ms = 0
    };
    
    return led_controller_send_command(&cmd);
}

bool led_controller_link_blink(uint32_t blink_rate_ms) {
    led_command_t cmd = {
        .type = LED_TYPE_LINK,
        .index = 0,
        .effect = LED_EFFECT_BLINK,
        .duration_ms = 0,  // Blink indefinitely
        .blink_rate_ms = blink_rate_ms
    };
    
    return led_controller_send_command(&cmd);
}

QueueHandle_t led_controller_get_queue(void) {
    return led_command_queue;
}

static void apply_led_command(const led_command_t *cmd) {
    led_state_t *state = NULL;
    
    // Get the appropriate state structure
    switch (cmd->type) {
        case LED_TYPE_NUKE:
            if (cmd->index < 3) {
                state = &nuke_led_states[cmd->index];
            }
            break;
        case LED_TYPE_ALERT:
            if (cmd->index < 6) {
                state = &alert_led_states[cmd->index];
            }
            break;
        case LED_TYPE_LINK:
            state = &link_led_state;
            break;
    }
    
    if (!state) {
        ESP_LOGW(TAG, "Invalid LED command: type=%d, index=%d", cmd->type, cmd->index);
        return;
    }
    
    // Apply the command
    state->effect = cmd->effect;
    state->blink_rate_ms = cmd->blink_rate_ms > 0 ? cmd->blink_rate_ms : LED_BLINK_INTERVAL_MS;
    
    // Calculate end time for timed effects
    if (cmd->duration_ms > 0 && cmd->effect == LED_EFFECT_BLINK_TIMED) {
        state->effect_end_time = xTaskGetTickCount() + pdMS_TO_TICKS(cmd->duration_ms);
    } else {
        state->effect_end_time = 0;  // Infinite
    }
    
    // For immediate effects (ON/OFF), set the state now
    if (cmd->effect == LED_EFFECT_ON) {
        state->current_state = true;
        switch (cmd->type) {
            case LED_TYPE_NUKE:
                module_io_set_nuke_led(cmd->index, true);
                break;
            case LED_TYPE_ALERT:
                module_io_set_alert_led(cmd->index, true);
                // Also turn on warning LED when any alert is active
                if (cmd->index > 0) {
                    alert_led_states[0].effect = LED_EFFECT_ON;
                    alert_led_states[0].current_state = true;
                    module_io_set_alert_led(0, true);
                }
                break;
            case LED_TYPE_LINK:
                module_io_set_link_led(true);
                break;
        }
    } else if (cmd->effect == LED_EFFECT_OFF) {
        state->current_state = false;
        switch (cmd->type) {
            case LED_TYPE_NUKE:
                module_io_set_nuke_led(cmd->index, false);
                break;
            case LED_TYPE_ALERT:
                module_io_set_alert_led(cmd->index, false);
                break;
            case LED_TYPE_LINK:
                module_io_set_link_led(false);
                break;
        }
    }
    
    ESP_LOGD(TAG, "LED command applied: type=%d, index=%d, effect=%d", 
             cmd->type, cmd->index, cmd->effect);
}

static void update_led_state(led_state_t *state, led_type_t type, uint8_t index) {
    uint32_t now = xTaskGetTickCount();
    
    // Check if timed effect has expired
    if (state->effect_end_time > 0 && now >= state->effect_end_time) {
        state->effect = LED_EFFECT_OFF;
        state->effect_end_time = 0;
        state->current_state = false;
        
        // Turn off the LED
        switch (type) {
            case LED_TYPE_NUKE:
                module_io_set_nuke_led(index, false);
                break;
            case LED_TYPE_ALERT:
                module_io_set_alert_led(index, false);
                // Check if we should turn off warning LED
                if (index > 0) {
                    bool any_alert_active = false;
                    for (int i = 1; i < 6; i++) {
                        if (alert_led_states[i].effect != LED_EFFECT_OFF) {
                            any_alert_active = true;
                            break;
                        }
                    }
                    if (!any_alert_active) {
                        alert_led_states[0].effect = LED_EFFECT_OFF;
                        alert_led_states[0].current_state = false;
                        module_io_set_alert_led(0, false);
                    }
                }
                break;
            case LED_TYPE_LINK:
                module_io_set_link_led(false);
                break;
        }
        return;
    }
    
    // Handle blinking effects
    if (state->effect == LED_EFFECT_BLINK || state->effect == LED_EFFECT_BLINK_TIMED) {
        uint32_t time_since_blink = (now - state->last_blink_time) * portTICK_PERIOD_MS;
        
        if (time_since_blink >= state->blink_rate_ms) {
            state->current_state = !state->current_state;
            state->last_blink_time = now;
            
            // Update physical LED
            switch (type) {
                case LED_TYPE_NUKE:
                    module_io_set_nuke_led(index, state->current_state);
                    break;
                case LED_TYPE_ALERT:
                    module_io_set_alert_led(index, state->current_state);
                    break;
                case LED_TYPE_LINK:
                    module_io_set_link_led(state->current_state);
                    break;
            }
        }
    }
}

static void led_controller_task(void *pvParameters) {
    ESP_LOGI(TAG, "LED controller task started");
    
    led_command_t cmd;
    const TickType_t update_interval = pdMS_TO_TICKS(50);  // 50ms update rate
    
    while (1) {
        // Process any pending commands (non-blocking)
        while (xQueueReceive(led_command_queue, &cmd, 0) == pdTRUE) {
            apply_led_command(&cmd);
        }
        
        // Update all LED states
        for (int i = 0; i < 3; i++) {
            update_led_state(&nuke_led_states[i], LED_TYPE_NUKE, i);
        }
        
        for (int i = 0; i < 6; i++) {
            update_led_state(&alert_led_states[i], LED_TYPE_ALERT, i);
        }
        
        update_led_state(&link_led_state, LED_TYPE_LINK, 0);
        
        // Wait before next update cycle
        vTaskDelay(update_interval);
    }
}
