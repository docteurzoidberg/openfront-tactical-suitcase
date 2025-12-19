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
#include "adc_handler.h"
#include "lcd_driver.h"
#include "io_task.h"
#include "network_manager.h"
#include "ota_manager.h"
#include "game_state.h"
#include "event_dispatcher.h"
#include "module_manager.h"
#include "nuke_module.h"
#include "alert_module.h"
#include "main_power_module.h"
#include "system_status_module.h"
#include "troops_module.h"

static const char *TAG = "OTS_MAIN";

// Event handlers
static bool handle_event(const internal_event_t *event);
static void handle_button_press(uint8_t button_index);
static void handle_network_event(network_event_type_t event_type, const char *ip_address);
static void handle_game_state_change(game_phase_t old_phase, game_phase_t new_phase);
static void handle_ws_connection(bool connected);
static void handle_io_expander_recovery(uint8_t board, bool was_down);

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

// I/O expander recovery callback
static void handle_io_expander_recovery(uint8_t board, bool was_down) {
    ESP_LOGI(TAG, "I/O Expander board #%d recovered (was_down: %s)", 
             board, was_down ? "yes" : "no");
    
    // Post internal event for modules to react
    event_dispatcher_post_simple(INTERNAL_EVENT_WS_CONNECTED, EVENT_SOURCE_SYSTEM);
    
    // Reinitialize module I/O configuration
    if (module_io_reinit() == ESP_OK) {
        ESP_LOGI(TAG, "Module I/O reconfigured after recovery");
    }
}

// Centralized event handler (now just delegates to game state)
static bool handle_event(const internal_event_t *event) {
    ESP_LOGD(TAG, "Handling event: type=%s, source=%d", 
             event_type_to_string(event->type), event->source);
    
    // Handle game state events
    if (event->type == GAME_EVENT_GAME_SPAWNING ||
        event->type == GAME_EVENT_GAME_START ||
        event->type == GAME_EVENT_GAME_END) {
        game_state_update(event->type);
        return true;
    }
    
    return false;
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
    
    // Initialize I/O expanders with error recovery
    ESP_LOGI(TAG, "Initializing I/O expanders...");
    if (!io_expander_begin(MCP23017_ADDRESSES, MCP23017_COUNT)) {
        ESP_LOGE(TAG, "Failed to initialize I/O expanders!");
        return;
    }
    
    // Register recovery callback
    io_expander_set_recovery_callback(handle_io_expander_recovery);
    
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
    module_manager_register(system_status_module_get());  // Handles boot/lobby screens
    module_manager_register(troops_module_get());         // Handles in-game troop display
    
    // Initialize all hardware modules (includes splash screen)
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
    
    // Initialize ADC handler
    if (adc_handler_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC handler!");
        return;
    }
    
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
