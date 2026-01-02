#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Project Identification
#define OTS_PROJECT_NAME        "OpenFront Tactical Suitcase"
#define OTS_PROJECT_ABBREV      "OTS"
#define OTS_FIRMWARE_NAME       "ots-fw-main"
#define OTS_FIRMWARE_VERSION    "2025-12-31.1"  // Updated by release.sh

// Device Identity (for hardware diagnostic)
#define OTS_DEVICE_SERIAL_NUMBER "OTS-FW-000001"  // Unique device serial number
#define OTS_DEVICE_OWNER        "OpenFront Team"  // Device owner/organization

// Wi-Fi Configuration (Fallback)
//
// Credentials are normally provisioned via the captive portal and stored in NVS.
// These values act only as a fallback when no credentials exist in NVS.
#define WIFI_SSID "IOT"
#define WIFI_PASSWORD "totoaimeliot"

// mDNS Configuration
// Default should match the TLS certificate hostname (see tls_creds.c).
#ifndef MDNS_HOSTNAME
#define MDNS_HOSTNAME "ots-fw-main"  // Device will be accessible at <hostname>.local
#endif

// WebSocket Server Configuration
#define WS_USE_TLS 1                // 1 = WSS (HTTPS), 0 = WS (HTTP)
#ifndef WS_SERVER_PORT
#define WS_SERVER_PORT 3000         // Port for WebSocket server
#endif

#if WS_USE_TLS
    // WSS (WebSocket Secure) - Required for userscript on HTTPS pages
    #define WS_PROTOCOL "wss://"
    #warning Building with WSS support - browsers will show security warning for self-signed certificate
#else
    // WS (WebSocket) - Only works with localhost or HTTP pages
    #define WS_PROTOCOL "ws://"
    #warning Building with WS support - will NOT work with userscript on HTTPS pages
#endif

// MCP23017 I2C addresses
static const uint8_t MCP23017_ADDRESSES[] = {0x20, 0x21};
#define MCP23017_COUNT 2

// Captive Portal Configuration
#define CAPTIVE_PORTAL_IP_A 192
#define CAPTIVE_PORTAL_IP_B 168
#define CAPTIVE_PORTAL_IP_C 4
#define CAPTIVE_PORTAL_IP_D 1

// I2C GPIO pins for ESP32-S3
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// RGB LED GPIO pin (onboard WS2812 on most ESP32-S3 devboards)
#define RGB_LED_GPIO 48

// Serial logging filter
//
// Controls which logs appear on the serial port.
// Values match ESP-IDF's esp_log_level_t:
//   0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE
//
// Override per build via PlatformIO build flags: -DOTS_LOG_LEVEL=2
#ifndef OTS_LOG_LEVEL
#define OTS_LOG_LEVEL 3
#endif

// Task priorities
#define TASK_PRIORITY_BUTTON_MONITOR 5
#define TASK_PRIORITY_LED_BLINK 4

// Timing constants
#define BUTTON_DEBOUNCE_MS 50
#define LED_BLINK_INTERVAL_MS 500

// OTA Configuration
#define OTA_HOSTNAME OTS_FIRMWARE_NAME
#define OTA_PASSWORD "ots2025"
#define OTA_PORT 3232

// Naming Conventions
// All logging TAGs should use prefix: OTS_<COMPONENT>
// Examples: OTS_MAIN, OTS_NETWORK, OTS_WS_SERVER, OTS_NUKE, OTS_ALERT
#define OTS_TAG_PREFIX "OTS_"

#endif // CONFIG_H
