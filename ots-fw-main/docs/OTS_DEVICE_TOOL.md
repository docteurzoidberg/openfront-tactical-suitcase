# OTS Device Tool (CLI)

Comprehensive Python CLI for OTS firmware device management and testing.

**Location:** `ots-fw-main/tools/ots_device_tool.py`

**Features:**
- Serial console monitoring with timestamps and logging
- OTA firmware uploads via HTTP
- NVS (non-volatile storage) management
- WiFi provisioning and management
- WebSocket testing capabilities
- Device version and status queries

**No Dependencies:** Pure Python 3 stdlib - works without pip install.

## Quick start

From `ots-fw-main/`:

```bash
# Show help
python3 tools/ots_device_tool.py --help

# Monitor logs (auto-detect port if possible)
python3 tools/ots_device_tool.py serial monitor --port auto

# Monitor logs and write to a file
python3 tools/ots_device_tool.py serial monitor --port /dev/ttyACM0 --log-file logs/device.log

# Send a single serial command (newline terminated)
python3 tools/ots_device_tool.py serial send --port /dev/ttyACM0 --cmd "wifi-status"

# WiFi provisioning and management
python3 tools/ots_device_tool.py wifi provision --ssid "MySSID" --password "MyPassword"
python3 tools/ots_device_tool.py wifi status --port /dev/ttyACM0
python3 tools/ots_device_tool.py wifi clear --port /dev/ttyACM0

# Get firmware version
python3 tools/ots_device_tool.py version --port /dev/ttyACM0

# Reboot device (requires firmware serial command support)
python3 tools/ots_device_tool.py serial reboot --port /dev/ttyACM0

# NVS management - Owner name
python3 tools/ots_device_tool.py nvs set-owner --port /dev/ttyACM0 --name "DrZoid"
python3 tools/ots_device_tool.py nvs get-owner --port /dev/ttyACM0
python3 tools/ots_device_tool.py nvs clear-owner --port /dev/ttyACM0

# NVS management - Serial number
python3 tools/ots_device_tool.py nvs set-serial --port /dev/ttyACM0 --serial "OTS-DEV-001"
python3 tools/ots_device_tool.py nvs get-serial --port /dev/ttyACM0
python3 tools/ots_device_tool.py nvs clear-serial --port /dev/ttyACM0
```

## Serial commands supported by firmware

The firmware implements a small line-based serial command handler on the active ESP-IDF console.

On the `esp32-s3-dev` build, the console is routed to **USB Serial/JTAG**, so commands/logs go over the same `/dev/ttyACM*` device.

Supported commands:
- `wifi-status`
- `wifi-clear` (clears NVS creds + reboots)
- `wifi-provision <ssid> <password>`
  - Aliases: `wifi-provisioning`, `wifi-provisionning`
- `version` (shows firmware name and version)
  - Alias: `fw-version`
- `nvs set owner_name <name>` (sets device owner name in NVS)
- `nvs erase owner_name` (clears device owner name from NVS)
- `nvs get owner_name` (shows current owner name)
- `nvs set serial_number <serial>` (sets device serial number in NVS)
- `nvs erase serial_number` (clears device serial number from NVS)
- `nvs get serial_number` (shows current serial number)
- `reboot` (reboots immediately)
  - Alias: `reset`

See `docs/WIFI_PROVISIONING.md` for the provisioning workflow.

## OTA upload

The firmware exposes an OTA endpoint:
- `POST http://<device-ip>:3232/update`

Examples:

```bash
# Upload a built firmware binary
python3 tools/ots_device_tool.py ota upload \
  --host 192.168.1.50 \
  --bin .pio/build/esp32-s3-dev/firmware.bin

# Auto-detect host by watching serial logs for GOT_IP
python3 tools/ots_device_tool.py ota upload \
  --serial-port /dev/ttyACM0 --auto-host \
  --bin .pio/build/esp32-s3-dev/firmware.bin
```

Notes:
- OTA is only available in normal STA mode (it starts after `GOT_IP`).
- Portal (SoftAP) mode does not start WebSocket, but HTTP config is still available on port 80.

## Subcommands

### `serial monitor`

Streams serial logs to stdout.

Common flags:
- `--port` (or `--serial-port`): serial device path, or `auto`
- `--baud`: default `115200`
- `--log-file`: optional file path to append logs
- `--timestamps`: prefix each line with local timestamp

### `serial send`

Sends a single command line to the firmware over UART0.

Flags:
- `--cmd`: command string (a newline is appended)

### `serial reboot`

Sends `reboot` over UART0.

### `wifi provision`

Provisions WiFi credentials via serial command.

Flags:
- `--ssid`: WiFi SSID (required)
- `--password`: WiFi password (required)
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py wifi provision --ssid "MyNetwork" --password "MyPassword123"
```

### `wifi status`

Gets the current WiFi status and stored credentials via serial command.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py wifi status
```

### `wifi clear`

Clears stored WiFi credentials from NVS via serial command. Device will reboot into captive portal mode.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py wifi clear
```

### `version`

Gets the firmware name and version from the device via serial command.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py version
```

Output:
```
Firmware: ots-fw-main v2025-12-31.1
```

### `nvs set-owner`

Sets the device owner name in NVS via serial command.

Flags:
- `--name`: owner name to set (required)
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs set-owner --name "DrZoid"
```

### `nvs get-owner`

Gets the current device owner name from NVS via serial command.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs get-owner
```

### `nvs clear-owner`

Clears the device owner name from NVS via serial command. This will trigger the onboarding wizard on next webapp load.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs clear-owner
```

### `nvs set-serial`

Sets the device serial number in NVS via serial command. Persists across OTA updates.

Flags:
- `--serial`: serial number to set (required)
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs set-serial --serial "OTS-DEV-001"
```

### `nvs get-serial`

Gets the current device serial number via serial command.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs get-serial
```

### `nvs clear-serial`

Clears the device serial number from NVS via serial command. Reverts to build-time default or MAC-derived serial.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs clear-serial
```

### `ota upload`

Uploads a `.bin` to the OTA endpoint.

Flags:
- `--host`: device IP/hostname
- `--auto-host --serial-port <port>`: derive host from serial logs
- `--bin`: firmware binary
- `--port`: OTA TCP port (default `3232`)
- `--timeout`: upload timeout

## Tips

- Use `serial monitor` in one terminal and run `ota upload` in another.
- If auto-host is flaky, pass `--host` explicitly.
- If you see repeated disconnects, confirm WiFi credentials and AP signal strength.

## Related Tools

### embed_webapp.py

Generates C header from webapp files with build hash injection. **Must run after editing webapp files.**

```bash
cd ots-fw-main/
python3 tools/embed_webapp.py
```

See `tools/README.md` for full documentation.

### Test Scripts

Automated test scripts are in `tools/tests/`:
- `ws_test.py` - WebSocket protocol testing
- `test_websocket.sh` - Quick WebSocket smoke tests

## See Also

- `tools/README.md` - Complete tools overview
- `docs/WIFI_PROVISIONING.md` - WiFi setup workflow
- `docs/OTA_GUIDE.md` - OTA update details
