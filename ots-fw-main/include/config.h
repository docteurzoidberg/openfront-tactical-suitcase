#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Project Identification
#define OTS_PROJECT_NAME        "OpenFront Tactical Suitcase"
#define OTS_PROJECT_ABBREV      "OTS"
#define OTS_FIRMWARE_NAME       "ots-fw-main"
#define OTS_FIRMWARE_VERSION    "0.1.0"

// Wi-Fi Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// WebSocket Server Configuration
#define WS_SERVER_URL "ws://192.168.77.121:3000/ws"

// MCP23017 I2C addresses
static const uint8_t MCP23017_ADDRESSES[] = {0x20, 0x21};
#define MCP23017_COUNT 2

// I2C GPIO pins for ESP32-S3
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

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
