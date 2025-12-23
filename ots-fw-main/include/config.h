#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Project Identification
#define OTS_PROJECT_NAME        "OpenFront Tactical Suitcase"
#define OTS_PROJECT_ABBREV      "OTS"
#define OTS_FIRMWARE_NAME       "ots-fw-main"
#define OTS_FIRMWARE_VERSION    "2025-12-20.1"  // Updated by release.sh

// Wi-Fi Configuration (Fallback)
//
// Preferred workflow: provision via Improv Serial (stored in NVS).
// Leave these empty to force "Improv-only" behavior.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

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

// I2C GPIO pins for ESP32-S3
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// RGB LED GPIO pin (onboard WS2812 on most ESP32-S3 devboards)
#define RGB_LED_GPIO 48

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
// Examples: OTS_MAIN, OTS_NETWORK, OTS_WS_CLIENT, OTS_NUKE, OTS_ALERT
#define OTS_TAG_PREFIX "OTS_"

#endif // CONFIG_H
