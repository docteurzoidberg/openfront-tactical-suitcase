# HTTP/HTTPS Server Core Component

**Component**: `esp_http_server_core`  
**Location**: `ots-fw-main/components/esp_http_server_core/`  
**Status**: ✅ Active (WiFi provisioning, OTA updates, WebSocket server)

## Overview

Reusable ESP-IDF component providing a unified HTTP/HTTPS server instance with TLS support. Other components register their URI handlers with this core server, eliminating duplicate server instances.

## Purpose

**Problem Solved:**
- Multiple components need HTTP endpoints (WiFi provisioning, OTA, WebSocket)
- Running multiple `httpd_handle_t` instances wastes resources
- Conflicts when components try to bind to same port

**Solution:**
- Single shared HTTP/HTTPS server instance
- Components register handlers via `http_server_register_handler()`
- Lifecycle managed by core component

## Features

- ✅ Single unified HTTP/HTTPS server instance
- ✅ TLS support with self-signed certificates
- ✅ Handler registration from multiple components
- ✅ Port 3000 (configurable)
- ✅ Automatic TLS mode detection
- ✅ Clean shutdown and cleanup

## API Reference

### Server Lifecycle

```c
#include "http_server.h"

// Configuration
http_server_config_t config = {
    .port = 3000,
    .max_handlers = 20,
    .max_uri_handlers = 20,
    .stack_size = 8192,
    .use_tls = true,                 // Enable HTTPS
    .cert_pem = server_cert_pem,     // TLS certificate
    .key_pem = server_key_pem,       // TLS private key
};

// Initialize and start server
esp_err_t http_server_init(http_server_config_t *config);

// Check if server is running
bool http_server_is_running(void);

// Stop and cleanup
void http_server_stop(void);
```

### Handler Registration

```c
// Register URI handler
esp_err_t http_server_register_handler(httpd_uri_t *uri_handler);

// Example: Register GET handler
httpd_uri_t my_handler = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
    .user_ctx = NULL
};
http_server_register_handler(&my_handler);
```

### Handler Implementation

```c
static esp_err_t status_get_handler(httpd_req_t *req) {
    const char *resp = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}
```

## Usage Example

### Component Integration

```c
#include "http_server.h"

// WiFi Provisioning Component
void wifi_register_handlers(void) {
    static httpd_uri_t wifi_page = {
        .uri = "/wifi",
        .method = HTTP_GET,
        .handler = wifi_page_handler,
        .user_ctx = NULL
    };
    http_server_register_handler(&wifi_page);
    
    static httpd_uri_t wifi_scan = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = wifi_scan_handler,
        .user_ctx = NULL
    };
    http_server_register_handler(&wifi_scan);
}

// OTA Update Component
void ota_register_handlers(void) {
    static httpd_uri_t ota_update = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_update_handler,
        .user_ctx = NULL
    };
    http_server_register_handler(&ota_update);
}

// WebSocket Component
void ws_register_handlers(void) {
    static httpd_uri_t ws_handler = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler_func,
        .is_websocket = true,
        .user_ctx = NULL
    };
    http_server_register_handler(&ws_handler);
}
```

### Main Application

```c
#include "http_server.h"

void app_main(void) {
    // Initialize WiFi first
    wifi_init();
    
    // Configure HTTP server
    http_server_config_t config = HTTP_SERVER_DEFAULT_CONFIG();
    config.use_tls = true;  // Enable HTTPS for WebSocket over WSS
    
    // Start server
    esp_err_t ret = http_server_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server init failed");
        return;
    }
    
    // Register component handlers
    wifi_register_handlers();
    ota_register_handlers();
    ws_register_handlers();
    
    ESP_LOGI(TAG, "HTTP server running on port %d", config.port);
}
```

## Configuration

**Default Configuration:**
```c
#define HTTP_SERVER_DEFAULT_CONFIG() { \
    .port = 3000, \
    .max_handlers = 20, \
    .max_uri_handlers = 20, \
    .stack_size = 8192, \
    .use_tls = false, \
    .cert_pem = NULL, \
    .key_pem = NULL, \
}
```

**TLS Configuration:**
```c
// Embedded certificates (from PEM files)
extern const char server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_server_cert_pem_end");
extern const char server_key_pem_start[] asm("_binary_server_key_pem_start");
extern const char server_key_pem_end[] asm("_binary_server_key_pem_end");

config.use_tls = true;
config.cert_pem = server_cert_pem_start;
config.key_pem = server_key_pem_start;
```

**Port Configuration:**
```c
// HTTP (plaintext)
config.port = 80;
config.use_tls = false;

// HTTPS (TLS)
config.port = 3000;
config.use_tls = true;
```

## Integration Points

**Components Using This Server:**

1. **WiFi Provisioning** (`network_manager.c`)
   - `/wifi` - WiFi configuration page
   - `/api/scan` - WiFi network scan
   - `/api/connect` - WiFi credentials submission

2. **OTA Updates** (`ota_manager.c`)
   - `/update` - Firmware upload endpoint
   - Port 3232 alternative configuration

3. **WebSocket Server** (`ws_server.c`)
   - `/ws` - WebSocket endpoint
   - Requires TLS for userscript on HTTPS pages

## TLS/HTTPS Mode

**Why HTTPS?**
- Userscript runs on `https://openfront.io`
- Browsers block mixed content (HTTPS page → WS connection)
- Solution: Use WSS (WebSocket Secure) via HTTPS server

**Self-Signed Certificate:**
```bash
# Generate certificate (development only)
openssl req -newkey rsa:2048 -nodes -keyout server_key.pem \
  -x509 -days 3650 -out server_cert.pem \
  -subj "/CN=ots-fw-main.local"

# Embed in firmware
cp server_cert.pem ots-fw-main/certs/
cp server_key.pem ots-fw-main/certs/
```

**Browser Warnings:**
- Self-signed cert triggers security warning
- User must manually accept certificate
- Production: Use valid certificate from CA

## Handler Ordering

**Handler Priority:**
- Handlers matched in registration order
- More specific URIs should be registered first

**Example:**
```c
// ✅ Correct order
http_server_register_handler(&exact_match);    // /api/status
http_server_register_handler(&wildcard_match); // /api/*

// ❌ Wrong order (wildcard catches everything)
http_server_register_handler(&wildcard_match); // /api/*
http_server_register_handler(&exact_match);    // /api/status (never reached)
```

## Memory Management

**Server Resources:**
- Single `httpd_handle_t` instance (shared)
- Stack size per handler: 8KB default
- Max open connections: 4 (ESP-IDF default)

**Handler Context:**
```c
// Pass component state to handlers
typedef struct {
    int connection_count;
    bool ready;
} my_component_ctx_t;

static my_component_ctx_t ctx = {0};

httpd_uri_t handler = {
    .uri = "/api/data",
    .method = HTTP_GET,
    .handler = data_handler,
    .user_ctx = &ctx,  // Available in handler
};
```

## Error Handling

**Server Start Failures:**
```c
esp_err_t ret = http_server_init(&config);
if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Server already running");
} else if (ret == ESP_ERR_NO_MEM) {
    ESP_LOGE(TAG, "Insufficient memory");
} else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Server init failed: %s", esp_err_to_name(ret));
}
```

**Handler Registration Failures:**
```c
esp_err_t ret = http_server_register_handler(&my_handler);
if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Server not initialized");
} else if (ret == ESP_ERR_HTTPD_HANDLERS_FULL) {
    ESP_LOGE(TAG, "Too many handlers registered");
}
```

## Testing

**Check Server Status:**
```bash
# From device serial console
> http-status

# From network
curl http://ots-fw-main.local:3000/
curl https://ots-fw-main.local:3000/ -k  # -k ignores cert errors
```

**WebSocket Test:**
```bash
# Use wscat or similar tool
wscat -c wss://192.168.1.100:3000/ws --no-check
```

**Handler List:**
```c
// Add debug endpoint to list registered handlers
static esp_err_t handlers_list_handler(httpd_req_t *req) {
    // Enumerate registered handlers
    // Return JSON array of URIs
}
```

## Troubleshooting

**Port already in use:**
- Check if another server instance is running
- Verify only one `httpd_start()` call
- Use `http_server_is_running()` before init

**Handler not called:**
- Check URI string matches exactly
- Verify method (GET/POST/etc.)
- Check handler registered before request
- Review handler ordering (wildcards)

**TLS handshake fails:**
- Verify certificate embedded correctly
- Check certificate validity period
- Browser may block self-signed cert
- Use `curl -k` for testing

**Memory errors:**
- Reduce `max_handlers` if limited RAM
- Decrease `stack_size` if tasks fail to create
- Monitor free heap: `esp_get_free_heap_size()`

## Related Documentation

- **WiFi Provisioning**: `/ots-fw-main/docs/WIFI_PROVISIONING.md`
- **OTA Updates**: `/ots-fw-main/docs/OTA_GUIDE.md`
- **WebSocket Server**: `/ots-fw-main/src/ws_server.c`
- **ESP-IDF HTTP Server**: [ESP HTTP Server Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html)

## Implementation Notes

**Architecture:**
- Singleton pattern (single server instance)
- Handler registry maintains list of registered URIs
- Lifecycle: init → register handlers → serve → stop

**Thread Safety:**
- `http_server_init()` not thread-safe (call once)
- `http_server_register_handler()` safe after init
- Handler functions execute in HTTP server thread

**Component Dependencies:**
```yaml
dependencies:
  idf:
    version: ">=5.0.0"
  esp_http_server:
    version: "*"
  esp_https_server:
    version: "*"  # For TLS support
```

---

**Last Updated**: January 5, 2026  
**Maintained By**: OTS Firmware Team
