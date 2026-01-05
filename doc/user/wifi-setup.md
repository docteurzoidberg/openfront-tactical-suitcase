# WiFi Setup Guide

Complete guide to connecting your OTS device to your network.

## 🌐 Connection Methods

Your OTS device supports two WiFi connection methods:

1. **Access Point (AP) Mode** - Device creates its own network (first-time setup)
2. **Station (STA) Mode** - Device connects to your existing network (normal operation)

## 🚀 Method 1: Access Point Setup (Recommended)

### Step 1: Enter AP Mode

Your device automatically enters AP mode when:
- First powered on (no WiFi credentials saved)
- Unable to connect to saved network
- Reset to factory defaults

**Indicators**:
- LINK LED is **OFF** or **blinking slowly**
- WiFi network `OTS-XXXXXX` is visible

### Step 2: Connect to Device

1. **Find the network**:
   - Network name: `OTS-XXXXXX` (XXXXXX = last 6 digits of MAC address)
   - **No password required** for initial setup

2. **Connect** from your computer, phone, or tablet

3. **Wait for portal**: Some devices auto-open the configuration portal
   - If not, open browser and go to: `http://192.168.4.1`

### Step 3: Configure WiFi

1. **Web portal opens** showing WiFi setup form

2. **Enter your network details**:
   ```
   WiFi SSID: [Your Network Name]
   Password: [Your WiFi Password]
   ```

3. **Click "Connect"**

4. **Wait 30 seconds**:
   - Device saves credentials
   - Attempts to connect to your network
   - LINK LED turns **solid green** = Success!

### Step 4: Find Device IP

Once connected, find your device's IP address:

**Method A: Router Admin Panel**
- Log into your router
- Look for device named "OTS-XXXXXX" or "ots-fw-main"
- Note the IP address (e.g., `192.168.1.100`)

**Method B: mDNS/Bonjour** (if supported)
- Device hostname: `ots-fw-main.local`
- Try accessing: `http://ots-fw-main.local:3000`

**Method C: Network Scanner**
- Use app like "Fing" or "Network Scanner"
- Look for ESP32 device on port 3000

## 🔧 Method 2: Serial Configuration

If web portal doesn't work, use serial commands via USB:

### Requirements
- USB-C cable
- Serial terminal software (PuTTY, Arduino Serial Monitor, screen)

### Steps

1. **Connect USB cable** from device to computer

2. **Open serial terminal**:
   - **Baud rate**: 115200
   - **Port**: `/dev/ttyUSB0` (Linux/Mac) or `COM3` (Windows)

3. **Send WiFi command**:
   ```
   wifi-set "YourNetworkName" "YourPassword"
   ```

4. **Device responds**:
   ```
   [OTS_NETWORK] WiFi credentials saved
   [OTS_NETWORK] Connecting to: YourNetworkName
   [OTS_NETWORK] Connected! IP: 192.168.1.100
   ```

5. **Save configuration**:
   - Credentials saved to non-volatile storage
   - Device auto-connects on next boot

## 🔄 Changing WiFi Networks

### Option A: Web Portal
1. Connect to current network
2. Browse to device IP: `http://[device-ip]:3000`
3. Navigate to WiFi settings
4. Enter new credentials

### Option B: Serial Commands
1. Connect via USB
2. Send: `wifi-set "NewNetwork" "NewPassword"`
3. Device reconnects automatically

### Option C: Factory Reset
1. Hold RESET button (if available) for 10 seconds
2. Device clears WiFi credentials
3. Returns to AP mode
4. Follow Method 1 to reconfigure

## 🐛 Troubleshooting

### Device Not Creating AP Network

**Symptoms**: Can't find `OTS-XXXXXX` network

**Solutions**:
1. **Power cycle**: Unplug, wait 10 seconds, plug back in
2. **Check antenna**: Ensure WiFi antenna is connected (if external)
3. **Factory reset**: Clear saved credentials via serial or reset button
4. **Check logs**: Connect via USB serial to see error messages

### Can't Connect to Device AP

**Symptoms**: Connection times out or fails

**Solutions**:
1. **Try different device**: Phone vs laptop may behave differently
2. **Disable cellular data**: On phone, turn off mobile data
3. **Clear WiFi cache**: Forget network and reconnect
4. **Check distance**: Stay within 10 meters during setup

### Portal Page Won't Load

**Symptoms**: Connected to AP but `http://192.168.4.1` doesn't open

**Solutions**:
1. **Manual URL**: Type `http://192.168.4.1` (not https://)
2. **Disable VPN**: Turn off VPN on your device
3. **Try different browser**: Chrome, Firefox, or Safari
4. **Check IP**: Device might use `192.168.4.1` or `10.0.0.1`
5. **Serial fallback**: Use USB serial method instead

### Device Won't Connect to WiFi

**Symptoms**: LINK LED stays off, no connection

**Solutions**:
1. **Check credentials**: SSID and password must be exact (case-sensitive)
2. **Check frequency**: Some devices only support 2.4GHz, not 5GHz
3. **Check security**: WPA2/WPA3 supported, WEP not supported
4. **Router distance**: Move router closer (< 20 meters)
5. **Router settings**:
   - Enable DHCP
   - Disable MAC filtering temporarily
   - Disable AP isolation
   - Check if too many devices connected

### Keeps Disconnecting

**Symptoms**: LINK LED blinks on/off repeatedly

**Solutions**:
1. **Signal strength**: Move device closer to router
2. **Channel interference**: Change router channel (1, 6, or 11 for 2.4GHz)
3. **Power supply**: Use quality USB adapter (2A minimum)
4. **Router load**: Too many devices on network
5. **Update firmware**: Install latest OTS firmware

### Can't Find Device IP

**Symptoms**: Connected but don't know device address

**Solutions**:
1. **Check router**: Log into router admin panel
2. **mDNS**: Try `ots-fw-main.local` in browser
3. **Network scan**: Use Fing or nmap
4. **Serial check**: Connect USB, device logs IP on connection
5. **Fixed IP**: Configure static IP in router DHCP settings

## 🔐 Security Notes

### Default AP Mode
- **No password** during initial setup for ease of use
- **Temporary** - Only active until WiFi configured
- **Local only** - Not connected to internet in AP mode

### Station Mode
- Device uses your network's security (WPA2/WPA3)
- mDNS hostname broadcast: `ots-fw-main.local`
- WebSocket server requires connection to local network
- No external internet access required for operation

## 📡 Network Requirements

**Minimum Requirements**:
- WiFi: 802.11 b/g/n
- Frequency: 2.4GHz or 5GHz (device-dependent)
- Security: WPA2-PSK or WPA3
- DHCP: Enabled
- Ports: 3000 (WebSocket), 3232 (OTA updates)

**Optimal Setup**:
- Strong signal strength (> -70 dBm)
- Low latency (< 50ms)
- Dedicated 2.4GHz network
- Static DHCP reservation for device

## 🌐 Advanced Configuration

### Static IP Address

Configure via serial commands:
```
wifi-set-static 192.168.1.100 255.255.255.0 192.168.1.1
```

### Custom Hostname

Change mDNS hostname:
```
hostname-set my-ots-device
```
Access via: `http://my-ots-device.local`

### WiFi Scan

List available networks:
```
wifi-scan
```

## 📖 See Also

- [Quick Start Guide](quick-start.md) - Complete setup process
- [Troubleshooting](troubleshooting.md) - General issues
- [Device Guide](device-guide.md) - Understanding your hardware

---

**Last Updated**: January 2026  
**Firmware Version**: 0.1.0+
