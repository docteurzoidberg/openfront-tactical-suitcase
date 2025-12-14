# OTA (Over-The-Air) Updates - Guide

## Overview

The OTS firmware supports **Over-The-Air (OTA)** updates, allowing you to upload new firmware remotely over WiFi without physical access to the ESP32-S3 device. This is essential for maintenance and updates when the device is installed in the tactical suitcase.

## Features

- ✅ **Network-based updates** - Upload via WiFi
- ✅ **Password protected** - Prevents unauthorized updates
- ✅ **Visual feedback** - LINK LED blinks during update
- ✅ **Progress monitoring** - Serial output shows percentage
- ✅ **Auto-reboot** - Device restarts after successful update
- ✅ **Error recovery** - Failed updates don't brick the device
- ✅ **mDNS discovery** - Easy device finding via hostname

## Configuration

OTA settings are defined in `include/config.h`:

```cpp
#define OTA_HOSTNAME "ots-fw-main"  // mDNS hostname for network discovery
#define OTA_PASSWORD "ots2025"      // Password for OTA authentication
#define OTA_PORT 3232               // Default Arduino OTA port
```

### Security Recommendations

1. **Change the default password** before deployment
2. Use a strong password (mix of letters, numbers, symbols)
3. Only enable OTA on trusted networks
4. Consider disabling OTA in production if not needed
5. Use WPA2/WPA3 encryption on WiFi network

## Prerequisites

Before performing OTA updates:

1. **Device must be powered on** - Power switch ON, 12V supplied
2. **WiFi must be connected** - LINK LED should be ON (solid)
3. **Device and computer on same network** - Same subnet
4. **Know device IP or hostname** - `ots-fw-main.local` or `192.168.x.x`

## Upload Methods

### Method 1: PlatformIO (Recommended)

**Via mDNS hostname:**
```bash
cd /path/to/ots-fw-main
pio run -t upload --upload-port ots-fw-main.local
```

**Via IP address:**
```bash
pio run -t upload --upload-port 192.168.1.100
```

**With environment specified:**
```bash
pio run -e esp32-s3-dev -t upload --upload-port ots-fw-main.local
```

### Method 2: Arduino IDE

1. Open the project in Arduino IDE
2. Go to **Tools → Port**
3. Select **"ots-fw-main at 192.168.x.x (ESP32 OTA)"**
4. Click **Sketch → Upload**
5. Enter password when prompted: `ots2025`
6. Wait for upload to complete

### Method 3: espota.py (Manual)

```bash
python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i ots-fw-main.local \
  -p 3232 \
  --auth=ots2025 \
  -f .pio/build/esp32-s3-dev/firmware.bin
```

## Update Process

### Normal Update Flow

1. **Initiate upload** from development machine
2. **Device receives OTA request**
3. **All LEDs turn OFF** (module LEDs)
4. **LINK LED begins BLINKING** (~1Hz)
5. **Firmware uploads** (30-60 seconds typically)
6. **Progress shown** via serial output and LED blinks
7. **Upload completes**
8. **Device automatically reboots**
9. **New firmware starts**
10. **LINK LED shows connection status** (solid ON when connected)

### Serial Output During Update

```
[OTA] Start updating sketch
[OTA] Progress: 0%
[OTA] Progress: 25%
[OTA] Progress: 50%
[OTA] Progress: 75%
[OTA] Progress: 100%
[OTA] Update complete!
```

### LED Behavior During Update

| Stage | LINK LED | Other LEDs |
|-------|----------|------------|
| Before update | ON (if connected) | Normal operation |
| Update starts | OFF briefly | All turn OFF |
| Uploading | BLINKING (1Hz) | OFF |
| Success | Device reboots | - |
| Error | RAPID BLINK (10x) | OFF |
| After error | Returns to normal | Resume operation |

## Error Handling

### Common Errors

**1. Authentication Failed**
```
[OTA] Error[1]: Auth Failed
```
- **Cause**: Wrong password
- **Fix**: Verify OTA_PASSWORD in config.h matches upload password
- **LED**: Rapid blinks 10 times

**2. Begin Failed**
```
[OTA] Error[2]: Begin Failed
```
- **Cause**: Insufficient space or invalid firmware
- **Fix**: Check firmware size, ensure correct platform
- **LED**: Rapid blinks 10 times

**3. Connect Failed**
```
[OTA] Error[3]: Connect Failed
```
- **Cause**: Network issues, device rebooted
- **Fix**: Check WiFi connection, retry upload
- **LED**: Rapid blinks 10 times

**4. Receive Failed**
```
[OTA] Error[4]: Receive Failed
```
- **Cause**: Connection lost during upload
- **Fix**: Check WiFi signal strength, move closer to AP
- **LED**: Rapid blinks 10 times

**5. End Failed**
```
[OTA] Error[5]: End Failed
```
- **Cause**: Flash write error, corruption
- **Fix**: Retry upload, check hardware
- **LED**: Rapid blinks 10 times

### Recovery from Failed Update

OTA updates are **safe** - a failed update will NOT brick the device:

1. Device detects error
2. LINK LED rapid blinks (visual notification)
3. Error logged to serial
4. Device **continues running old firmware**
5. Retry update after fixing the issue

### Emergency Recovery

If OTA becomes completely inaccessible:

1. Connect via USB cable
2. Upload firmware using serial (esptool/PlatformIO)
3. Fix OTA configuration if needed
4. Re-enable OTA for future updates

## Troubleshooting

### Device Not Found

**Symptom**: Upload fails with "device not found" or timeout

**Checks**:
- ✅ Is LINK LED ON? (WiFi must be connected)
- ✅ Same network? `ping ots-fw-main.local`
- ✅ mDNS working? Try IP address instead
- ✅ Firewall blocking port 3232?
- ✅ Device actually powered on?

**Solutions**:
```bash
# Test mDNS resolution
ping ots-fw-main.local

# Find device IP via serial logs or router admin
# Then use IP directly
pio run -t upload --upload-port 192.168.1.100

# Check if device is listening on OTA port
nmap -p 3232 192.168.1.100
```

### Upload Starts But Fails

**Symptom**: Upload begins but fails midway

**Checks**:
- ✅ WiFi signal strength (RSSI in serial logs)
- ✅ Stable power supply (voltage drops?)
- ✅ Enough free heap memory
- ✅ No interference on WiFi channel

**Solutions**:
- Move device closer to WiFi access point
- Reduce WiFi interference (other devices)
- Check 12V power supply is stable
- Retry upload

### Wrong Password Error

**Symptom**: "Auth Failed" error

**Checks**:
- ✅ Password in config.h: `OTA_PASSWORD`
- ✅ Upload command uses correct password
- ✅ Case-sensitive password match

**Solutions**:
1. Verify password in code: `include/config.h`
2. Recompile if password was changed
3. Upload via USB to update password
4. Retry OTA with correct password

### Device Reboots During Update

**Symptom**: Upload fails, device reboots

**Checks**:
- ✅ Power supply adequate (12V, sufficient current)
- ✅ Watchdog timer interference
- ✅ Memory overflow (heap)

**Solutions**:
- Check power supply quality
- Reduce background tasks if any
- Review memory usage in firmware

## Best Practices

### Development Workflow

1. **Initial upload via USB** - First firmware load
2. **Enable OTA** - Compile with OTA support
3. **Test OTA locally** - Verify OTA works
4. **Deploy to device** - Install in suitcase
5. **Update via OTA** - Remote updates from now on

### Production Deployment

1. ✅ **Change default password** before deployment
2. ✅ **Test OTA thoroughly** in development
3. ✅ **Document IP address** or ensure mDNS works
4. ✅ **Keep USB access available** as backup
5. ✅ **Monitor first update** to verify success
6. ✅ **Have rollback plan** (previous firmware backup)

### Security Checklist

- [ ] Default OTA password changed
- [ ] Strong password used (12+ characters)
- [ ] WiFi network uses WPA2/WPA3 encryption
- [ ] Device on isolated/trusted network segment
- [ ] OTA port (3232) firewalled from untrusted networks
- [ ] Physical access to device restricted
- [ ] Firmware update process documented
- [ ] Rollback procedure tested

## Advanced Usage

### Custom OTA Port

Change port in `config.h`:
```cpp
#define OTA_PORT 8080  // Custom port instead of 3232
```

Then upload using:
```bash
pio run -t upload --upload-port ots-fw-main.local --upload-port 8080
```

### Disable OTA Password

**Not recommended for production!**

In `main.cpp`:
```cpp
void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  // ArduinoOTA.setPassword(OTA_PASSWORD);  // Comment out
  // ... rest of setup
}
```

### Monitor OTA Progress

Watch serial output during update:
```bash
pio device monitor -b 115200
```

In another terminal, start upload:
```bash
pio run -t upload --upload-port ots-fw-main.local
```

### Conditional OTA

Enable OTA only in development builds:

```cpp
#ifdef ENABLE_OTA
  setupOTA();
#endif
```

Build with flag:
```ini
[env:esp32-s3-dev]
build_flags = -DENABLE_OTA
```

## Firmware Size Considerations

ESP32-S3 flash layout:
- **Total flash**: 8MB (typically)
- **OTA partitions**: 2x ~1.8MB (dual OTA scheme)
- **Maximum firmware size**: ~1.8MB per partition

Check firmware size after build:
```bash
pio run
# Look for: Flash: [===] 32.2% (used 1075170 bytes from 3342336 bytes)
```

If firmware too large:
- Reduce debug symbols: `build_flags = -Os`
- Remove unused libraries
- Enable LTO optimization
- Reduce embedded assets

## Related Documentation

- **Main Project Context**: `copilot-project-context.md`
- **Main Power Module**: `MAIN_POWER_MODULE_PROMPT.md` (LINK LED OTA behavior)
- **Improvements List**: `IMPROVEMENTS.md`
- **Arduino OTA Docs**: https://arduino-esp32.readthedocs.io/en/latest/api/ota.html

---

**OTA Update Command Quick Reference:**
```bash
# Via hostname
pio run -t upload --upload-port ots-fw-main.local

# Via IP
pio run -t upload --upload-port 192.168.1.100

# Find device
ping ots-fw-main.local

# Monitor during update
pio device monitor -b 115200
```

**Default Credentials:**
- Hostname: `ots-fw-main`
- Password: `ots2025` ⚠️ **CHANGE THIS!**
- Port: `3232`

---

**Last Updated**: December 9, 2025  
**Status**: ✅ Implemented and tested  
**Security Level**: 🔐 Password protected
