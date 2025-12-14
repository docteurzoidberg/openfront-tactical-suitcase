#pragma once

// Basic Wi-Fi and WebSocket configuration for the OTS firmware.
// Adjust these values to match your local network and OTS server.

static const char *WIFI_SSID = "IOT";
static const char *WIFI_PASSWORD = "totoaimeliot";

// Default OTS server WebSocket endpoint
static const char *OTS_WS_HOST = "192.168.77.121"; // IP or hostname of ots-server
static const uint16_t OTS_WS_PORT = 3000;          // Nuxt dev server port
static const char *OTS_WS_PATH = "/ws";            // WebSocket path

// I2C Configuration
#define I2C_SDA 21  // ESP32-S3 I2C SDA pin
#define I2C_SCL 22  // ESP32-S3 I2C SCL pin
#define I2C_FREQ 100000  // 100kHz I2C bus speed

// MCP23017 I2C addresses configuration
// Add or remove addresses as needed for your hardware setup

static const uint8_t MCP23017_ADDRESSES[] = {0x20, 0x21};
static const uint8_t MCP23017_COUNT = 2;

// OTA (Over-The-Air) Update Configuration
#define OTA_HOSTNAME "ots-fw-main"      // mDNS hostname for OTA discovery
#define OTA_PASSWORD "ots2025"          // Password for OTA updates (CHANGE THIS!)
#define OTA_PORT 3232                   // Default Arduino OTA port
