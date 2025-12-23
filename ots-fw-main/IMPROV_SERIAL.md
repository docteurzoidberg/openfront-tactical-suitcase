# Improv Serial WiFi Provisioning

The OTS firmware now supports **Improv Serial** for easy WiFi provisioning without reflashing!

## Features

✅ **NVS Storage**: WiFi credentials stored persistently in flash  
✅ **Fallback Support**: If NVS empty, uses hardcoded credentials from `config.h`  
✅ **Web Interface**: Use any Improv-compatible tool to provision  
✅ **Factory Reset**: Clear credentials via serial command  
✅ **Auto-reconnect**: Device automatically uses new credentials after provisioning  

## Quick Start

### Option 1: Web Interface (Easiest)

1. **Connect your ESP32-S3 via USB**
2. **Open browser** and visit: **https://www.improv-wifi.com/serial/**
3. **Click "Connect Device"** and select your serial port
4. **Enter WiFi credentials** and provision!

### Option 2: Python Script

```bash
# Install Improv Serial tool
pip install improv-cli

# Provision device
improv-serial --device /dev/ttyACM0 provision "MyWiFi" "MyPassword"
```

### Option 3: ESPHome Improv WiFi App

- **Android**: [Improv WiFi](https://play.google.com/store/apps/details?id=com.improv_wifi)
- **iOS**: Not yet available

## Serial Protocol

The firmware listens on **UART0** (same as console) for Improv Serial commands.

### Supported Commands

| Command | Description |
|---------|-------------|
| `IDENTIFY` | Blink LED (identify device) |
| `GET_DEVICE_INFO` | Get firmware name, version, chip type |
| `GET_CURRENT_STATE` | Check if provisioned |
| `WIFI_SETTINGS` | Provision WiFi credentials |

## How It Works

1. **On boot**, firmware checks NVS for stored credentials
2. **If found**, uses NVS credentials
3. **If not found**, falls back to `config.h` (WIFI_SSID/WIFI_PASSWORD)
4. **Improv Serial** always runs in background, listening for provisioning commands
5. **After provisioning**, device automatically restarts with new credentials

## Factory Reset

To clear stored credentials and return to fallback mode:

### Via Improv Serial
```python
# Using improv-cli
improv-serial --device /dev/ttyACM0 factory-reset
```

### Via Serial Monitor
```c
// In code, call:
improv_serial_clear_credentials();
esp_restart();
```

## LED Indicators

During provisioning, the RGB LED shows:
- **🔴 Red**: No WiFi connection
- **🟠 Orange**: WiFi connected, no WebSocket
- **🟢 Green**: Fully connected

## NVS Storage Details

**Namespace**: `wifi`  
**Keys**:
- `ssid`: WiFi network name (max 32 chars)
- `password`: WiFi password (max 64 chars)

## Security Considerations

⚠️ **Credentials are stored in plaintext** in NVS flash  
⚠️ **Anyone with serial access** can read/overwrite credentials  
⚠️ **No encryption** on the serial protocol  

For production environments, consider:
- Disabling serial access after first provision
- Using ESP32 flash encryption
- Implementing additional authentication

## Troubleshooting

### Device Not Responding to Improv Commands

1. Check serial connection: `screen /dev/ttyACM0 115200`
2. Verify Improv Serial task is running (check boot logs)
3. Try factory reset and re-provision

### WiFi Not Connecting After Provision

1. Check credentials are correct
2. Verify SSID is in range
3. Check serial logs for connection errors
4. Try factory reset and re-provision

### Credentials Not Persisting

1. Check NVS partition exists in `partitions.csv`
2. Verify NVS flash initialized (check boot logs)
3. Try erasing flash: `esptool.py erase_flash`

## Example Output

```
I (123) OTS_MAIN: ===========================================
I (124) OTS_MAIN: OpenFront Tactical Suitcase v2025-12-20.1
I (125) OTS_MAIN: Firmware: ots-fw-main
I (126) OTS_MAIN: ===========================================
I (150) OTS_WIFI_CREDS: WiFi credentials storage initialized
I (151) OTS_IMPROV: Initializing Improv Serial on UART0...
I (152) OTS_IMPROV: Device already provisioned
I (153) OTS_IMPROV: Improv Serial task started
I (154) OTS_IMPROV: Improv Serial listening on UART0...
I (155) OTS_IMPROV: Connect with Improv WiFi tools or web interface
I (156) OTS_IMPROV: Visit: https://www.improv-wifi.com/
I (200) OTS_WIFI_CREDS: Loaded credentials from NVS: SSID=MyWiFi
I (201) OTS_MAIN: WiFi credentials loaded: SSID=MyWiFi
...
I (5432) OTS_IMPROV: Received command: type=1, len=18
I (5433) OTS_IMPROV: Provisioning WiFi: SSID='NewNetwork'
I (5434) OTS_WIFI_CREDS: Saved credentials to NVS: SSID=NewNetwork
I (5435) OTS_IMPROV: Provisioning complete! Device will reconnect with new credentials.
I (7435) OTS_MAIN: Restarting to apply new credentials...
```

## Development Notes

### Files Added

- `include/improv_serial.h` - Improv Serial protocol interface
- `include/wifi_credentials.h` - NVS credential storage interface
- `src/improv_serial.c` - Protocol implementation
- `src/wifi_credentials.c` - NVS storage implementation

### Integration Points

- `main.c` - Initialize and start Improv Serial
- `network_manager.c` - Use credentials from NVS
- `CMakeLists.txt` - Build new source files

### Testing

```bash
# Build and flash
cd ots-fw-main
pio run -t upload && pio device monitor

# Provision via web interface
# Visit: https://www.improv-wifi.com/serial/

# Or use Python
pip install improv-cli
improv-serial --device /dev/ttyACM0 identify
improv-serial --device /dev/ttyACM0 provision "SSID" "Password"
```

## References

- **Improv WiFi**: https://www.improv-wifi.com/
- **Protocol Spec**: https://www.improv-wifi.com/serial/
- **ESP-IDF NVS**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html

---

**Next Steps**: Implement BLE provisioning for mobile app support! 📱
