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
#include "ws_protocol.h"
#include "http_server.h"
#include "ws_handlers.h"
#include "webapp_handlers.h"
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
#include "rgb_status.h"
#include "wifi_credentials.h"
#include "ots_logging.h"
#include "serial_commands.h"

static const char *TAG = "OTS_MAIN";

// Module update task
static TaskHandle_t s_module_task = NULL;
static void module_update_task(void *pvParameters) {
    (void)pvParameters;
    // Keep this lightweight; modules are expected to be non-blocking.
    while (true) {
        (void)module_manager_update_all();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Event handlers
static bool handle_event(const internal_event_t *event);
static void handle_button_press(uint8_t button_index);
static void handle_network_event(network_event_type_t event_type, const char *ip_address);
static void handle_game_state_change(game_phase_t old_phase, game_phase_t new_phase);
static void handle_ws_connection(bool connected);
static void handle_io_expander_recovery(uint8_t board, bool was_down);

// Network event handler
static void handle_network_event(network_event_type_t event_type, const char *ip_address) {
    if (event_type == NETWORK_EVENT_CONNECTED) {
        // If a userscript is already connected (e.g. DHCP renew / reconnect edge cases),
        // keep the higher-priority purple state instead of overwriting it with yellow.
        if (ws_handlers_has_userscript()) {
            rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
        } else {
            // Match test-websocket semantics: as soon as WiFi associates, show YELLOW.
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        }
    } else if (event_type == NETWORK_EVENT_GOT_IP) {
        ESP_LOGI(TAG, "Network connected with IP: %s", ip_address);
        // Test-websocket stays YELLOW once WiFi is up, but do not override
        // an already-established userscript session.
        if (ws_handlers_has_userscript()) {
            rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
        } else {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        }
        
        // Start HTTP OTA server
        if (ota_manager_start() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTP OTA server");
        }
        
        // HTTP server is already running with WebSocket handlers registered.
        // Trigger display refresh now that network is ready.
        ESP_LOGI(TAG, "Network ready - WebSocket server listening");
        system_status_refresh_display();
    } else if (event_type == NETWORK_EVENT_PROVISIONING_REQUIRED) {
        ESP_LOGW(TAG, "Provisioning required; switching to captive portal mode");

        // Stop network and switch to portal mode
        (void)network_manager_stop();
        webapp_handlers_set_mode(WEBAPP_MODE_CAPTIVE_PORTAL);
        
        // Open AP (no password) for simplest provisioning.
        (void)network_manager_start_captive_portal("OTS-SETUP", "");
    } else if (event_type == NETWORK_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "Network disconnected");
        rgb_status_set(RGB_STATUS_DISCONNECTED);
    }
}

// WebSocket connection handler
static void handle_ws_connection(bool connected) {
    if (connected) {
        ESP_LOGI(TAG, "WebSocket client connected (has_userscript=%s)", ws_handlers_has_userscript() ? "yes" : "no");
        // Match test-websocket semantics: WS activity is reflected immediately,
        // even if the game had started (i.e. do not keep green).
        if (ws_handlers_has_userscript()) {
            rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
        } else {
            // Some client connected (e.g. browser), but not userscript yet.
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        }
    } else {
        ESP_LOGI(TAG, "WebSocket disconnected");
        if (network_manager_is_connected()) {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        } else {
            rgb_status_set(RGB_STATUS_DISCONNECTED);
        }
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
    
    // Set RGB to error state briefly to indicate hardware issue
    if (was_down) {
        rgb_status_set(RGB_STATUS_ERROR);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Show error for 2 seconds
        
        // Restore previous status based on the same semantics as test-websocket.
        if (ws_handlers_has_userscript()) {
            rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
        } else if (network_manager_is_connected()) {
            rgb_status_set(RGB_STATUS_WIFI_ONLY);
        } else {
            rgb_status_set(RGB_STATUS_DISCONNECTED);
        }
    }
    
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

        if (event->type == GAME_EVENT_GAME_START) {
            rgb_status_set(RGB_STATUS_GAME_STARTED);
        } else if (event->type == GAME_EVENT_GAME_END) {
            // Restore to purple/yellow based on current connectivity.
            if (ws_handlers_has_userscript()) {
                rgb_status_set(RGB_STATUS_USERSCRIPT_CONNECTED);
            } else if (network_manager_is_connected()) {
                rgb_status_set(RGB_STATUS_WIFI_ONLY);
            } else {
                rgb_status_set(RGB_STATUS_DISCONNECTED);
            }
        }
        return true;
    }
    
    return false;
}

void app_main(void) {
    // Configure serial log filtering as early as possible.
    (void)ots_logging_init();

    ESP_LOGI(TAG, "===========================================" );
    ESP_LOGI(TAG, "%s v%s", OTS_PROJECT_NAME, OTS_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Firmware: %s", OTS_FIRMWARE_NAME);
    ESP_LOGI(TAG, "===========================================" );
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize RGB status LED early so we can report boot failures
    if (rgb_status_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RGB status LED!");
        return;
    }
    rgb_status_set(RGB_STATUS_DISCONNECTED);
    
    // Initialize WiFi credentials storage
    ESP_LOGI(TAG, "Initializing WiFi credentials...");
    if (wifi_credentials_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi credentials!");
        return;
    }

    // Enable serial WiFi commands (wifi-clear / wifi-provision)
    (void)serial_commands_init();
    
    const bool have_stored_creds = wifi_credentials_exist();
    wifi_credentials_t wifi_creds;
    memset(&wifi_creds, 0, sizeof(wifi_creds));
    if (have_stored_creds) {
        if (wifi_credentials_load(&wifi_creds) != ESP_OK) {
            ESP_LOGW(TAG, "Expected stored credentials but could not load; entering portal mode");
        }
    }

    if (have_stored_creds) {
        ESP_LOGI(TAG, "Stored WiFi credentials found: SSID=%s", wifi_creds.ssid);
        // Mode will be set later after HTTP server init
    } else {
        ESP_LOGW(TAG, "No stored WiFi credentials (NVS clear); starting captive portal mode");
        // Mode will be set later after HTTP server init
    }
    
    // Initialize I/O expanders with error recovery
    ESP_LOGI(TAG, "Initializing I/O expanders...");
    bool io_expanders_ready = io_expander_begin(MCP23017_ADDRESSES, MCP23017_COUNT);
    if (!io_expanders_ready) {
        ESP_LOGE(TAG, "Failed to initialize I/O expanders - continuing without hardware I/O boards");
    } else {
        // Register recovery callback
        io_expander_set_recovery_callback(handle_io_expander_recovery);

        // Initialize module I/O
        if (module_io_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize module I/O - continuing without hardware modules");
            io_expanders_ready = false;
        }
    }

    // Initialize event dispatcher
    if (event_dispatcher_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize event dispatcher!");
        return;
    }
    
    // Register event handler (handles game state events)
    event_dispatcher_register(GAME_EVENT_INVALID, handle_event);

    // Initialize module manager (modules may not require MCP23017 boards).
    if (module_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize module manager");
        return;
    }

    // Route all events to modules as well.
    event_dispatcher_register(GAME_EVENT_INVALID, module_manager_route_event);

    // Register modules.
    // Always register SystemStatus so the LCD can show boot/connection screens
    // even when the MCP23017 I/O boards are absent.
    ESP_LOGI(TAG, "Registering hardware modules...");
    module_manager_register((hardware_module_t *)system_status_module_get());
    // Troops is LCD-driven and should be available regardless of MCP23017 presence.
    module_manager_register((hardware_module_t *)troops_module_get());
    
    if (io_expanders_ready) {
        module_manager_register(&nuke_module);
        module_manager_register(&alert_module);
        module_manager_register(&main_power_module);

        // Initialize all hardware modules (includes splash screen)
        if (module_manager_init_all() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize hardware modules - continuing without hardware modules");
            io_expanders_ready = false;
        }
    } else {
        // Still initialize SystemStatus (LCD screens) even without MCP23017 boards.
        if (module_manager_init_all() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SystemStatus module");
        }
    }

    // Start periodic module updates (LCD screen refresh, timers, etc.).
    if (!s_module_task) {
        BaseType_t ok = xTaskCreate(
            module_update_task,
            "mod_upd",
            4096,
            NULL,
            4,
            &s_module_task
        );
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Failed to start module update task");
            s_module_task = NULL;
        }
    }
    
    // Initialize game state manager
    if (game_state_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize game state!");
        return;
    }
    game_state_set_callback(handle_game_state_change);
    
    // Initialize LED controller
    if (io_expanders_ready) {
        if (led_controller_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize LED controller!");
            return;
        }
    }
    
    // Initialize button handler
    if (io_expanders_ready) {
        if (button_handler_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize button handler!");
            return;
        }
    }
    
    // Initialize ADC handler
    if (io_expanders_ready) {
        if (adc_handler_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize ADC handler!");
            return;
        }
    }
    
    // Initialize network manager with credentials from NVS/config
    if (network_manager_init(wifi_creds.ssid, wifi_creds.password, MDNS_HOSTNAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network manager!");
        return;
    }
    network_manager_set_event_callback(handle_network_event);

    // Initialize WebSocket protocol
    if (ws_protocol_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket protocol!");
        return;
    }

    // ========== HTTP SERVER INITIALIZATION ==========
    // Single HTTP/HTTPS server on port 3000 with TLS (WSS support).
    // Components register their handlers with this core server.
    
    // TLS credentials (extern from tls_creds.c)
    extern const char ots_server_cert_pem[];
    extern const size_t ots_server_cert_pem_len;
    extern const char ots_server_key_pem[];
    extern const size_t ots_server_key_pem_len;
    
    http_server_config_t server_config = {
        .port = WS_SERVER_PORT,  // 3000
        .use_tls = WS_USE_TLS,   // true for HTTPS/WSS
        .cert_pem = (const uint8_t *)ots_server_cert_pem,
        .cert_len = ots_server_cert_pem_len,
        .key_pem = (const uint8_t *)ots_server_key_pem,
        .key_len = ots_server_key_pem_len,
        .max_open_sockets = 4,  // ESP-IDF limit: 7 total (3 internal + 4 user)
        .max_uri_handlers = 32,
        .close_fn = NULL  // Auto-set by http_server from ws_handlers
    };
    
    if (http_server_init(&server_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HTTP server!");
        return;
    }
    
    if (http_server_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server!");
        return;
    }
    
    // Register WebSocket handlers (must be first for /ws route)
    if (ws_handlers_register(http_server_get_handle()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket handlers!");
        return;
    }
    ws_handlers_set_connection_callback(handle_ws_connection);
    
    // Register webapp handlers (UI and configuration endpoints)
    if (webapp_handlers_register(http_server_get_handle()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register webapp handlers!");
        return;
    }
    
    ESP_LOGI(TAG, "HTTP server ready with WebSocket and webapp handlers");
    // ================================================
    
    // Initialize OTA managers (HTTP and Arduino protocols)
    if (ota_manager_init(OTA_PORT, OTA_HOSTNAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HTTP OTA manager!");
        return;
    }
    
    // Start network services
    if (!have_stored_creds || strlen(wifi_creds.ssid) == 0) {
        // Portal mode: start AP only.
        rgb_status_set(RGB_STATUS_WIFI_CONNECTING);  // Blue for captive portal setup
        (void)network_manager_start_captive_portal("OTS-SETUP", "");
        webapp_handlers_set_mode(WEBAPP_MODE_CAPTIVE_PORTAL);
        ESP_LOGI(TAG, "*** Captive portal started, calling system_status_refresh_display() ***");
        // Trigger display refresh now that portal mode is active
        system_status_refresh_display();
    } else {
        rgb_status_set(RGB_STATUS_WIFI_CONNECTING);
        webapp_handlers_set_mode(WEBAPP_MODE_NORMAL);
        if (network_manager_start() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start network!");
            return;
        }
    }
    
    // Start dedicated I/O task
    if (io_expanders_ready) {
        if (io_task_start() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start I/O task!");
            return;
        }
    }
    
    ESP_LOGI(TAG, "OTS Firmware initialized successfully");
}
