# Conditional source list based on build type
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_websocket.c")
    # Check if we're building a test by looking for the test file in sources
    get_filename_component(PARENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
    if(
        EXISTS "${PARENT_DIR}/../.pio/build/test-websocket"
        OR EXISTS "${PARENT_DIR}/../.pio/build/test-websocket-noimprov"
    )
        message(STATUS "Building test-websocket: minimal sources")
        set(TEST_BUILD TRUE)
    endif()
endif()

if(TEST_BUILD)
    # Test-websocket: minimal sources (no I2C hardware)
    set(COMPONENT_SRCS
        "protocol.c"
        "tls_creds.c"
        "ws_server.c"
        "ws_protocol.c"
        "game_state.c"
        "network_manager.c"
        "rgb_status.c"
        "wifi_credentials.c"
        "improv_serial.c"
        "tests/test_websocket.c"
    )
    set(COMPONENT_REQUIRES
        espressif__mdns
        esp_wifi
        nvs_flash
        esp_timer
        json
        esp_http_server
        esp_https_server
    )
else()
    # Production firmware: all sources
    set(COMPONENT_SRCS
        "main.c"
        "io_expander.c"
        "module_io.c"
        "protocol.c"
        "tls_creds.c"
        "ws_server.c"
        "ws_protocol.c"
        "led_controller.c"
        "button_handler.c"
        "adc_handler.c"
        "io_task.c"
        "game_state.c"
        "network_manager.c"
        "ota_manager.c"
        "event_dispatcher.c"
        "module_manager.c"
        "nuke_tracker.c"
        "nuke_module.c"
        "alert_module.c"
        "main_power_module.c"
        "system_status_module.c"
        "troops_module.c"
        "lcd_driver.c"
        "adc_driver.c"
        "rgb_status.c"
        "wifi_credentials.c"
        "improv_serial.c"
    )
    set(COMPONENT_REQUIRES
        espressif__mdns
        esp_wifi
        nvs_flash
        esp_timer
        json
        driver
        esp_http_server
        app_update
    )
endif()

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS 
        "../include"
    REQUIRES ${COMPONENT_REQUIRES}
)
