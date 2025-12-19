#include "button_handler.h"
#include "module_io.h"
#include "config.h"
#include "event_dispatcher.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "OTS_BUTTONS";

#define BUTTON_EVENT_QUEUE_SIZE 8
#define BUTTON_COUNT 3

// Button state tracking for debouncing
typedef struct {
    bool current_state;       // Current debounced state
    bool raw_state;           // Last raw reading
    uint32_t last_change_time; // Time of last state change (ms)
    uint32_t press_time;      // Time when button was pressed (ms)
} button_state_t;

static button_state_t button_states[BUTTON_COUNT] = {0};
static QueueHandle_t button_event_queue = NULL;
static button_event_callback_t button_callback = NULL;

esp_err_t button_handler_init(void) {
    ESP_LOGI(TAG, "Initializing button handler...");
    
    // Create button event queue
    button_event_queue = xQueueCreate(BUTTON_EVENT_QUEUE_SIZE, sizeof(button_event_t));
    if (button_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return ESP_FAIL;
    }
    
    // Initialize button states
    for (int i = 0; i < BUTTON_COUNT; i++) {
        button_states[i].current_state = false;
        button_states[i].raw_state = false;
        button_states[i].last_change_time = 0;
        button_states[i].press_time = 0;
    }
    
    ESP_LOGI(TAG, "Button handler initialized");
    return ESP_OK;
}

esp_err_t button_handler_scan(void) {
    uint32_t now = esp_timer_get_time() / 1000;  // Convert to ms
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
        bool pressed = false;
        
        // Read current button state
        if (!module_io_read_nuke_button(i, &pressed)) {
            continue;  // Skip if read failed
        }
        
        // Debouncing logic
        if (pressed != button_states[i].raw_state) {
            // State has changed, update raw state and timestamp
            button_states[i].raw_state = pressed;
            button_states[i].last_change_time = now;
        }
        
        // Check if debounce period has elapsed
        uint32_t time_since_change = now - button_states[i].last_change_time;
        if (time_since_change >= BUTTON_DEBOUNCE_MS) {
            // Debounce period has passed, update current state if changed
            if (pressed != button_states[i].current_state) {
                button_states[i].current_state = pressed;
                
                // Generate button event
                button_event_t event = {
                    .button_index = i,
                    .pressed = pressed,
                    .timestamp_ms = now
                };
                
                // Send to queue (non-blocking)
                xQueueSend(button_event_queue, &event, 0);
                
                // Post internal event (for press events only)
                if (pressed) {
                    internal_event_t int_event = {
                        .type = INTERNAL_EVENT_BUTTON_PRESSED,
                        .source = EVENT_SOURCE_BUTTON,
                        .timestamp = now,
                        .data = {0}
                    };
                    // Store button index in first byte of data
                    int_event.data[0] = i;
                    event_dispatcher_post(&int_event);
                }
                
                // Track press time
                if (pressed) {
                    button_states[i].press_time = now;
                    ESP_LOGI(TAG, "Button %d pressed", i);
                } else {
                    uint32_t press_duration = now - button_states[i].press_time;
                    ESP_LOGD(TAG, "Button %d released (held %lu ms)", i, press_duration);
                }
            }
        }
    }
    
    return ESP_OK;
}

void button_handler_set_callback(button_event_callback_t callback) {
    button_callback = callback;
}

QueueHandle_t button_handler_get_queue(void) {
    return button_event_queue;
}

bool button_handler_is_pressed(uint8_t button_index) {
    if (button_index >= BUTTON_COUNT) {
        return false;
    }
    return button_states[button_index].current_state;
}
