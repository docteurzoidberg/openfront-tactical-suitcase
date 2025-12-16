#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "config.h"
#include "io_expander.h"
#include "module_io.h"
#include "protocol.h"
#include "ws_client.h"
#include "ws_protocol.h"
#include "led_controller.h"
#include "button_handler.h"
#include "io_task.h"
#include "network_manager.h"
#include "ota_manager.h"
#include "game_state.h"
#include "event_dispatcher.h"
#include "module_manager.h"
#include "nuke_module.h"
#include "alert_module.h"
#include "main_power_module.h"

static const char *TAG = "MAIN";

// Event handlers
static bool handle_event(const internal_event_t *event);
static void handle_button_press(uint8_t button_index);
static void handle_network_event(network_event_type_t event_type, const char *ip_address);
static void handle_game_state_change(game_phase_t old_phase, game_phase_t new_phase);
static void handle_ws_connection(bool connected);

// Network event handler
static void handle_network_event(network_event_type_t event_type, const char *ip_address) {
    if (event_type == NETWORK_EVENT_GOT_IP) {
        ESP_LOGI(TAG, "Network connected with IP: %s", ip_address);
        
        // Start OTA server
        if (ota_manager_start() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start OTA server");
        }
        
        // Start WebSocket client
        ws_client_start();
    } else if (event_type == NETWORK_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "Network disconnected");
    }
}

// WebSocket connection handler
static void handle_ws_connection(bool connected) {
    if (connected) {
        ESP_LOGI(TAG, "WebSocket connected");
    } else {
        ESP_LOGI(TAG, "WebSocket disconnected");
    }
}

// Game state change handler
static void handle_game_state_change(game_phase_t old_phase, game_phase_t new_phase) {
    ESP_LOGI(TAG, "Game state changed: %d -> %d", old_phase, new_phase);
}

// Centralized event handler (now just delegates to game state)
static bool handle_event(const internal_event_t *event) {
    ESP_LOGD(TAG, "Handling event: type=%s, source=%d", 
             event_type_to_string(event->type), event->source);
    
    // Handle game state events
    if (event->type == GAME_EVENT_GAME_START ||
        event->type == GAME_EVENT_GAME_END ||
        event->type == GAME_EVENT_WIN ||
        event->type == GAME_EVENT_LOOSE) {
        game_state_update(event->type);
        return true;
    }
    
    return false;
}

// Button mapping table
typedef struct {
    game_event_type_t event_type;
    const char *nuke_type;
} button_mapping_t;

static const button_mapping_t button_map[] = {
    {GAME_EVENT_NUKE_LAUNCHED, "atom"},
    {GAME_EVENT_HYDRO_LAUNCHED, "hydro"},
    {GAME_EVENT_MIRV_LAUNCHED, "mirv"}
};

// Button press handler
static void handle_button_press(uint8_t button_index) {
    if (button_index >= sizeof(button_map) / sizeof(button_map[0])) {
        ESP_LOGW(TAG, "Invalid button index: %d", button_index);
        return;
    }
    
    ESP_LOGI(TAG, "Button %d pressed (%s)", button_index, button_map[button_index].nuke_type);
    
    // Create and post button event using lookup table
    game_event_t game_event = {0};
    game_event.timestamp = esp_timer_get_time() / 1000;
    game_event.type = button_map[button_index].event_type;
    strncpy(game_event.message, "Nuke sent", sizeof(game_event.message) - 1);
    snprintf(game_event.data, sizeof(game_event.data), "{\"nukeType\":\"%s\"}", 
             button_map[button_index].nuke_type);
    
    // Post to event dispatcher (for local handling)
    event_dispatcher_post_game_event(&game_event, EVENT_SOURCE_BUTTON);
    
    // Send to WebSocket server
    ws_client_send_event(&game_event);
}

void app_main(void) {
    ESP_LOGI(TAG, "OTS Firmware Main Controller Starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize I/O expanders
    ESP_LOGI(TAG, "Initializing I/O expanders...");
    if (!io_expander_begin(MCP23017_ADDRESSES, MCP23017_COUNT)) {
        ESP_LOGE(TAG, "Failed to initialize I/O expanders!");
        return;
    }
    
    // Initialize module I/O
    if (module_io_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize module I/O!");
        return;
    }
    
    // Initialize event dispatcher
    if (event_dispatcher_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize event dispatcher!");
        return;
    }
    
    // Register event handler (handles game state events)
    event_dispatcher_register(GAME_EVENT_INVALID, handle_event);
    
    // Initialize module manager
    if (module_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize module manager!");
        return;
    }
    
    // Register hardware modules
    ESP_LOGI(TAG, "Registering hardware modules...");
    module_manager_register(&nuke_module);
    module_manager_register(&alert_module);
    module_manager_register(&main_power_module);
    
    // Initialize all hardware modules
    if (module_manager_init_all() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize hardware modules!");
        return;
    }
    
    // Initialize game state manager
    if (game_state_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize game state!");
        return;
    }
    game_state_set_callback(handle_game_state_change);
    
    // Initialize LED controller
    if (led_controller_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED controller!");
        return;
    }
    
    // Initialize button handler
    if (button_handler_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button handler!");
        return;
    }
    button_handler_set_callback(handle_button_press);
    
    // Initialize network manager
    if (network_manager_init(WIFI_SSID, WIFI_PASSWORD, OTA_HOSTNAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network manager!");
        return;
    }
    network_manager_set_event_callback(handle_network_event);
    
    // Initialize OTA manager
    if (ota_manager_init(OTA_PORT, OTA_HOSTNAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OTA manager!");
        return;
    }
    
    // Initialize WebSocket protocol
    if (ws_protocol_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket protocol!");
        return;
    }
    
    // Initialize WebSocket client
    if (ws_client_init(WS_SERVER_URL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client!");
        return;
    }
    ws_client_set_connection_callback(handle_ws_connection);
    
    // Start network services
    if (network_manager_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start network!");
        return;
    }
    
    // Start dedicated I/O task
    if (io_task_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I/O task!");
        return;
    }
    
    ESP_LOGI(TAG, "OTS Firmware initialized successfully");
}
