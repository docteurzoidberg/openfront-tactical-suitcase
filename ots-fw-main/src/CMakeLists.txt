set(TEST_BUILD FALSE)
set(TEST_NAME "")

# Prefer PlatformIO build_flags (-DTEST_*) for selecting test sources.
# These are the most reliable signal and avoid mis-detection when build
# directories are reused or named unexpectedly.
set(_ALL_CFLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} ${CMAKE_ASM_FLAGS}")

function(_ots_has_define out_var define_name)
    string(FIND "${_ALL_CFLAGS}" "-D${define_name}" _idx)
    if(_idx EQUAL -1)
        set(${out_var} FALSE PARENT_SCOPE)
    else()
        set(${out_var} TRUE PARENT_SCOPE)
    endif()
endfunction()

_ots_has_define(_HAS_TEST_WEBSOCKET TEST_WEBSOCKET)
_ots_has_define(_HAS_TEST_I2C TEST_I2C)
_ots_has_define(_HAS_TEST_OUTPUTS TEST_OUTPUTS)
_ots_has_define(_HAS_TEST_INPUTS TEST_INPUTS)
_ots_has_define(_HAS_TEST_ADC TEST_ADC)
_ots_has_define(_HAS_TEST_LCD TEST_LCD)

if(_HAS_TEST_WEBSOCKET)
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-websocket")
elseif(_HAS_TEST_I2C)
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-i2c")
elseif(_HAS_TEST_OUTPUTS)
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-outputs")
elseif(_HAS_TEST_INPUTS)
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-inputs")
elseif(_HAS_TEST_ADC)
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-adc")
elseif(_HAS_TEST_LCD)
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-lcd")
endif()

# PlatformIO builds each env in its own build directory:
#   ots-fw-main/.pio/build/<env-name>
# That means the most reliable way to detect the active env is the current
# CMake binary dir name, NOT the existence of other env build folders.
get_filename_component(PIO_ENV_NAME "${CMAKE_BINARY_DIR}" NAME)

if(NOT TEST_BUILD AND PIO_ENV_NAME STREQUAL "test-websocket")
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-websocket")
elseif(NOT TEST_BUILD AND PIO_ENV_NAME STREQUAL "test-i2c")
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-i2c")
elseif(NOT TEST_BUILD AND PIO_ENV_NAME STREQUAL "test-outputs")
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-outputs")
elseif(NOT TEST_BUILD AND PIO_ENV_NAME STREQUAL "test-inputs")
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-inputs")
elseif(NOT TEST_BUILD AND PIO_ENV_NAME STREQUAL "test-adc")
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-adc")
elseif(NOT TEST_BUILD AND PIO_ENV_NAME STREQUAL "test-lcd")
    set(TEST_BUILD TRUE)
    set(TEST_NAME "test-lcd")
endif()

# Fallback: if someone runs a non-PlatformIO CMake build but still passes
# -DSDKCONFIG_DEFAULTS=sdkconfig.test-*, try to infer from that.
if(NOT TEST_BUILD AND DEFINED SDKCONFIG_DEFAULTS)
    get_filename_component(SDKCONFIG_DEFAULTS_BASENAME "${SDKCONFIG_DEFAULTS}" NAME)
    if(SDKCONFIG_DEFAULTS_BASENAME STREQUAL "sdkconfig.test-websocket")
        set(TEST_BUILD TRUE)
        set(TEST_NAME "test-websocket")
    elseif(SDKCONFIG_DEFAULTS_BASENAME STREQUAL "sdkconfig.test-i2c")
        set(TEST_BUILD TRUE)
        set(TEST_NAME "test-i2c")
    elseif(SDKCONFIG_DEFAULTS_BASENAME STREQUAL "sdkconfig.test-outputs")
        set(TEST_BUILD TRUE)
        set(TEST_NAME "test-outputs")
    elseif(SDKCONFIG_DEFAULTS_BASENAME STREQUAL "sdkconfig.test-inputs")
        set(TEST_BUILD TRUE)
        set(TEST_NAME "test-inputs")
    elseif(SDKCONFIG_DEFAULTS_BASENAME STREQUAL "sdkconfig.test-adc")
        set(TEST_BUILD TRUE)
        set(TEST_NAME "test-adc")
    elseif(SDKCONFIG_DEFAULTS_BASENAME STREQUAL "sdkconfig.test-lcd")
        set(TEST_BUILD TRUE)
        set(TEST_NAME "test-lcd")
    endif()
endif()

if(TEST_BUILD)
    message(STATUS "Building ${TEST_NAME}: minimal sources")

    if(TEST_NAME STREQUAL "test-websocket")
        # WebSocket test: WiFi + HTTP/HTTPS server + WebSocket handlers + RGB status
        set(COMPONENT_SRCS
            "protocol.c"
            "tls_creds.c"
            "http_server.c"
            "ws_handlers.c"
            "ws_protocol.c"
            "event_dispatcher.c"
            "led_controller.c"
            "game_state.c"
            "network_manager.c"
            "i2c_bus.c"
            "io_expander.c"
            "lcd_driver.c"
            "adc_driver.c"
            "adc_handler.c"
            "rgb_status.c"
            "ots_logging.c"
            "wifi_credentials.c"
            "tests/test_websocket.c"
        )
        set(COMPONENT_REQUIRES
            espressif__mdns
            esp_wifi
            nvs_flash
            esp_timer
            json
            driver
            esp_http_server
            esp_https_server
        )
    elseif(TEST_NAME STREQUAL "test-i2c")
        set(COMPONENT_SRCS
            "ots_logging.c"
            "tests/test_i2c_scan.c"
        )
        set(COMPONENT_REQUIRES
            driver
        )
    elseif(TEST_NAME STREQUAL "test-outputs")
        set(COMPONENT_SRCS
            "ots_logging.c"
            "tests/test_output_board.c"
        )
        set(COMPONENT_REQUIRES
            driver
        )
    elseif(TEST_NAME STREQUAL "test-inputs")
        set(COMPONENT_SRCS
            "ots_logging.c"
            "tests/test_input_board.c"
        )
        set(COMPONENT_REQUIRES
            driver
        )
    elseif(TEST_NAME STREQUAL "test-adc")
        set(COMPONENT_SRCS
            "ots_logging.c"
            "i2c_bus.c"
            "adc_driver.c"
            "tests/test_adc.c"
        )
        set(COMPONENT_REQUIRES
            driver
        )
    elseif(TEST_NAME STREQUAL "test-lcd")
        set(COMPONENT_SRCS
            "ots_logging.c"
            "i2c_bus.c"
            "lcd_driver.c"
            "tests/test_lcd.c"
        )
        set(COMPONENT_REQUIRES
            driver
        )
    else()
        message(FATAL_ERROR "Unknown test build: ${TEST_NAME}")
    endif()
else()
    # Production firmware: all sources
    set(COMPONENT_SRCS
        "main.c"
        "i2c_bus.c"
        "io_expander.c"
        "module_io.c"
        "protocol.c"
        "tls_creds.c"
        "http_server.c"
        "ws_handlers.c"
        "webapp_handlers.c"
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
        "ots_logging.c"
        "wifi_credentials.c"
        "device_settings.c"
        "serial_commands.c"
    )
    set(COMPONENT_REQUIRES
        espressif__mdns
        esp_wifi
        nvs_flash
        esp_timer
        json
        driver
        esp_http_server
        esp_https_server
        app_update
    )
endif()

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS 
        "../include"
    REQUIRES ${COMPONENT_REQUIRES}
)
