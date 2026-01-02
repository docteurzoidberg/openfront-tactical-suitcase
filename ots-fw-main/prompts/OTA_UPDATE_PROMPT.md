# OTA Update - Implementation Prompt

## Overview

This prompt guides the implementation of **Over-The-Air (OTA) firmware updates** in the OTS firmware. OTA allows remote firmware updates without physical access to the device.

## Purpose

OTA updates provide:
- **Remote updates**: Update firmware without USB cable
- **Safety**: Dual-partition system prevents bricking
- **Convenience**: Update multiple devices on network
- **Rollback**: Automatic rollback on boot failure

## OTA Architecture

```
┌─────────────────────────────────────────────┐
│           Flash Memory Layout               │
├─────────────────────────────────────────────┤
│  Bootloader      (64 KB)                    │
│  Partition Table (4 KB)                     │
│  NVS             (20 KB)                    │
│  OTA Data        (8 KB)   ← Boot partition  │
│  Factory App     (2 MB)   ← Initial FW      │
│  OTA_0 (App_0)   (2 MB)   ← Partition A     │
│  OTA_1 (App_1)   (2 MB)   ← Partition B     │
└─────────────────────────────────────────────┘
```

### Dual Partition System

- **Active partition**: Currently running firmware
- **Inactive partition**: Target for new firmware
- **OTA data partition**: Tracks which partition to boot from
- **Factory partition**: Original firmware (optional recovery)

## Partition Table

File: `partitions.csv`

```csv
# Name,   Type, SubType,  Offset,  Size,    Flags
nvs,      data, nvs,      0x9000,  0x5000,
otadata,  data, ota,      0xe000,  0x2000,
phy_init, data, phy,      0x10000, 0x1000,
factory,  app,  factory,  0x20000, 0x200000,
ota_0,    app,  ota_0,    0x220000,0x200000,
ota_1,    app,  ota_1,    0x420000,0x200000,
```

**Key points:**
- Factory: Initial firmware (never overwritten)
- ota_0 / ota_1: Alternating update partitions
- otadata: Boot partition selector
- Each OTA partition: 2 MB (enough for typical firmware)

## OTA Manager Implementation

### 1. Initialization

```c
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_log.h"

static const char* TAG = "OTS_OTA";

esp_err_t ota_manager_init(void) {
    ESP_LOGI(TAG, "Initializing OTA manager");
    
    // Get current partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // New firmware just booted, mark as valid
            ESP_LOGI(TAG, "New firmware boot successful, marking as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    
    ESP_LOGI(TAG, "Running partition: %s at 0x%08lx", 
             running->label, running->address);
    
    return ESP_OK;
}
```

### 2. HTTP OTA Server

Create an HTTP endpoint for firmware upload:

```c
static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t* update_partition = NULL;
static size_t bytes_written = 0;

static esp_err_t ota_upload_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA update started");
    
    // Get update partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition: %s at 0x%08lx", 
             update_partition->label, update_partition->address);
    
    // Begin OTA
    esp_err_t ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ret;
    }
    
    // Turn off all module LEDs during update
    disable_all_module_leds();
    
    // Receive firmware data
    char buf[1024];
    int remaining = req->content_len;
    bytes_written = 0;
    
    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;  // Retry
            }
            
            ESP_LOGE(TAG, "Receive failed");
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        
        // Write to flash
        ret = esp_ota_write(ota_handle, buf, received);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(ret));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ret;
        }
        
        remaining -= received;
        bytes_written += received;
        
        // Progress indication (blink LINK LED every 5%)
        int progress = (bytes_written * 100) / req->content_len;
        if (progress % 5 == 0) {
            toggle_link_led();
            ESP_LOGI(TAG, "OTA progress: %d%%", progress);
        }
    }
    
    // Finalize OTA
    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed");
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ret;
    }
    
    // Set boot partition
    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ret;
    }
    
    ESP_LOGI(TAG, "OTA update successful, wrote %d bytes", bytes_written);
    
    // Send success response
    const char* resp = "OTA update successful, rebooting...";
    httpd_resp_send(req, resp, strlen(resp));
    
    // Schedule reboot
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

static const httpd_uri_t ota_upload = {
    .uri       = "/update",
    .method    = HTTP_POST,
    .handler   = ota_upload_handler,
};
```

### 3. OTA Error Handling

```c
static void ota_error_handler(esp_err_t err) {
    switch (err) {
        case ESP_ERR_OTA_VALIDATE_FAILED:
            ESP_LOGE(TAG, "Image validation failed");
            // Rapid blink LINK LED (error indication)
            for (int i = 0; i < 10; i++) {
                toggle_link_led();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
            
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memory");
            break;
            
        case ESP_ERR_FLASH_OP_FAIL:
            ESP_LOGE(TAG, "Flash operation failed");
            break;
            
        default:
            ESP_LOGE(TAG, "OTA error: %s", esp_err_to_name(err));
            break;
    }
}
```

## Automatic Rollback

### Enable Rollback Protection

```c
void ota_enable_rollback_protection(void) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // Wait for system to stabilize (e.g., 30 seconds)
            ESP_LOGI(TAG, "Waiting 30s before validating new firmware...");
            vTaskDelay(pdMS_TO_TICKS(30000));
            
            // Check if system is stable (all modules initialized, WiFi connected, etc.)
            if (system_is_stable()) {
                ESP_LOGI(TAG, "System stable, marking firmware as valid");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "System unstable, will rollback on next boot");
                // Don't mark as valid - will rollback automatically
            }
        }
    }
}

bool system_is_stable(void) {
    // Check critical systems
    bool stable = true;
    
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected");
        stable = false;
    }
    
    if (!all_modules_initialized()) {
        ESP_LOGW(TAG, "Not all modules initialized");
        stable = false;
    }
    
    // Add other stability checks
    
    return stable;
}
```

## Uploading Firmware

### Using curl (Command Line)

```bash
# Build firmware
cd ots-fw-main
pio run

# Upload via OTA
curl -X POST --data-binary @.pio/build/esp32-s3-dev/firmware.bin \
     http://192.168.x.x:3232/update

# Or with progress bar
curl -X POST --data-binary @.pio/build/esp32-s3-dev/firmware.bin \
     http://192.168.x.x:3232/update \
     --progress-bar -o /dev/null
```

### Using Python Script

```python
#!/usr/bin/env python3
import requests
import sys

def upload_firmware(host, firmware_path):
    url = f"http://{host}:3232/update"
    
    print(f"Uploading {firmware_path} to {url}")
    
    with open(firmware_path, 'rb') as f:
        files = {'file': f}
        response = requests.post(url, data=f.read(), stream=True)
        
        if response.status_code == 200:
            print("Upload successful!")
            print(response.text)
        else:
            print(f"Upload failed: {response.status_code}")
            print(response.text)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: upload_ota.py <host> <firmware.bin>")
        sys.exit(1)
    
    upload_firmware(sys.argv[1], sys.argv[2])
```

### Using PlatformIO

Add to `platformio.ini`:

```ini
[env:esp32-s3-dev]
; ... existing config ...
upload_protocol = espota
upload_port = 192.168.x.x
upload_flags =
    --port=3232
```

Then upload:
```bash
pio run -t upload --upload-port 192.168.x.x
```

## OTA Configuration

### In config.h

```c
// OTA settings
#define OTA_SERVER_PORT 3232
#define OTA_BUFFER_SIZE 1024
#define OTA_TIMEOUT_MS 30000
#define OTA_MAX_RETRIES 3

// OTA status LED behavior
#define OTA_LED_BLINK_INTERVAL_MS 500  // Blink during update
#define OTA_ERROR_BLINK_COUNT 10       // Rapid blinks on error
```

## mDNS for Device Discovery

```c
#include "mdns.h"

esp_err_t ota_start_mdns(const char* hostname) {
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    mdns_hostname_set(hostname);
    mdns_instance_name_set("OTS Firmware Main");
    
    // Advertise HTTP service
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_add(NULL, "_ota", "_tcp", OTA_SERVER_PORT, NULL, 0);
    
    ESP_LOGI(TAG, "mDNS started: %s.local", hostname);
    return ESP_OK;
}
```

Now you can use: `http://ots-fw-main.local:3232/update`

## Verification

### Check Running Partition

```c
void ota_print_info(void) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    
    ESP_LOGI(TAG, "Running partition: %s", running->label);
    ESP_LOGI(TAG, "Boot partition: %s", boot->label);
    
    if (running != boot) {
        ESP_LOGW(TAG, "Boot partition mismatch! Running: %s, Boot: %s", 
                 running->label, boot->label);
    }
    
    // Get app description
    esp_app_desc_t app_desc;
    const esp_partition_t* partition = esp_ota_get_running_partition();
    esp_ota_get_partition_description(partition, &app_desc);
    
    ESP_LOGI(TAG, "Firmware version: %s", app_desc.version);
    ESP_LOGI(TAG, "Compile time: %s %s", app_desc.date, app_desc.time);
    ESP_LOGI(TAG, "IDF version: %s", app_desc.idf_ver);
}
```

### Validate Firmware Image

```c
bool ota_validate_image(const esp_partition_t* partition) {
    esp_app_desc_t app_desc;
    esp_err_t ret = esp_ota_get_partition_description(partition, &app_desc);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition description");
        return false;
    }
    
    ESP_LOGI(TAG, "Image version: %s", app_desc.version);
    ESP_LOGI(TAG, "Image date: %s %s", app_desc.date, app_desc.time);
    
    // Additional validation checks
    if (app_desc.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "Invalid magic word");
        return false;
    }
    
    return true;
}
```

## Testing OTA Updates

### Test Sequence

1. **Build initial firmware**
   ```bash
   pio run
   pio run -t upload  # USB upload
   ```

2. **Verify firmware boots**
   ```bash
   pio device monitor
   # Check logs for version number
   ```

3. **Modify firmware version**
   ```c
   // In config.h
   #define OTS_FIRMWARE_VERSION "1.0.1"  // Increment version
   ```

4. **Build updated firmware**
   ```bash
   pio run
   ```

5. **Upload via OTA**
   ```bash
   curl -X POST --data-binary @.pio/build/esp32-s3-dev/firmware.bin \
        http://ots-fw-main.local:3232/update
   ```

6. **Monitor reboot**
   ```bash
   pio device monitor
   # Check logs for new version number
   ```

7. **Verify rollback protection**
   - Wait 30 seconds
   - Check logs for "marking firmware as valid"

### Test Rollback

1. **Create intentionally broken firmware**
   ```c
   void app_main(void) {
       // Force crash after boot
       vTaskDelay(pdMS_TO_TICKS(5000));
       abort();  // Will trigger watchdog
   }
   ```

2. **Upload broken firmware**
   ```bash
   curl -X POST --data-binary @firmware.bin http://device/update
   ```

3. **Observe automatic rollback**
   - Device reboots
   - Crashes after 5 seconds
   - Watchdog triggers reboot
   - Boots into previous partition
   - Logs show: "Rollback to previous version"

## Security Considerations

### 1. Firmware Signing (Advanced)

Enable secure boot and firmware signing:

```c
// In menuconfig
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME=y
CONFIG_SECURE_BOOT_VERIFICATION_KEY="path/to/key.pem"
```

### 2. Encrypted OTA (HTTPS)

Use HTTPS instead of HTTP:

```c
esp_http_client_config_t config = {
    .url = "https://ota.example.com/firmware.bin",
    .cert_pem = server_cert_pem_start,
    .timeout_ms = OTA_TIMEOUT_MS,
};
```

### 3. Authentication

Add password protection:

```c
static esp_err_t ota_upload_handler(httpd_req_t *req) {
    // Check for authentication header
    char auth[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
        return ESP_FAIL;
    }
    
    // Verify password (use proper authentication in production!)
    if (strcmp(auth, "Bearer " OTA_PASSWORD) != 0) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid credentials");
        return ESP_FAIL;
    }
    
    // Continue with OTA...
}
```

## Troubleshooting

### Issue: OTA Upload Fails

**Symptoms**: Connection timeout or error during upload

**Solutions**:
1. Check network connectivity
2. Verify HTTP server is running (`netstat -an | grep 3232`)
3. Check firewall not blocking port 3232
4. Increase timeout values
5. Try smaller firmware (reduce features)

### Issue: Device Reboots During Update

**Symptoms**: Upload starts but device reboots before completion

**Solutions**:
1. Check power supply (OTA increases power draw)
2. Disable watchdog during OTA
3. Increase HTTP receive timeout
4. Check for memory leaks

### Issue: Device Won't Boot After OTA

**Symptoms**: Continuous reboot loop after OTA

**Solutions**:
1. Wait for automatic rollback (30 seconds)
2. Manually flash via USB: `pio run -t upload`
3. Erase flash and start fresh: `esptool.py erase_flash`
4. Check partition table is correct

### Issue: Wrong Partition Boots

**Symptoms**: Old firmware runs after OTA

**Solutions**:
1. Check `esp_ota_set_boot_partition()` was called
2. Verify OTA data partition not corrupted
3. Check return values from OTA functions
4. Use `ota_print_info()` to debug

## Checklist

- [ ] Partition table configured (factory + ota_0 + ota_1)
- [ ] OTA manager initialized
- [ ] HTTP server with `/update` endpoint
- [ ] Firmware upload handler implemented
- [ ] Progress indication (LED blinks)
- [ ] Error handling for all failure modes
- [ ] Automatic rollback protection enabled
- [ ] System stability checks implemented
- [ ] mDNS for device discovery
- [ ] Version information in app_desc
- [ ] Tested with real OTA update
- [ ] Tested rollback on bad firmware
- [ ] Security measures (if needed)

## References

- OTA guide: `docs/OTA_GUIDE.md`
- Partition table: `partitions.csv`
- Config: `include/config.h`
- OTA manager: `src/ota_manager.c`
- ESP-IDF OTA API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- ESP-IDF App Update API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/app_update.html
