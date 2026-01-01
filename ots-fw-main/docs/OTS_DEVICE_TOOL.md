# OTS Device Tool (CLI)

This repo includes a durable, stdlib-only Python CLI for interacting with an OTS firmware device over:
- **Serial console** (send commands, monitor logs, save logs)
- **OTA HTTP** (upload firmware)

Tool location:
- `ots-fw-main/tools/ots_device_tool.py`

## Goals

- No external Python dependencies (stdlib-only).
- Works in CI/dev shells without `pip install`.
- Ergonomic defaults (auto-detect serial port, auto-detect device IP from serial logs when possible).

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

# Provision WiFi via serial then reboot
python3 tools/ots_device_tool.py serial send --port /dev/ttyACM0 --cmd "wifi-provision MySSID MyPassword"

# Clear WiFi creds (device will reboot)
python3 tools/ots_device_tool.py serial send --port /dev/ttyACM0 --cmd "wifi-clear"

# Reboot device (requires firmware serial command support)
python3 tools/ots_device_tool.py serial reboot --port /dev/ttyACM0

# Set device owner name in NVS
python3 tools/ots_device_tool.py nvs set-owner --port /dev/ttyACM0 --name "DrZoid"

# Clear device owner name from NVS (triggers onboarding wizard)
python3 tools/ots_device_tool.py nvs clear-owner --port /dev/ttyACM0
```

## Serial commands supported by firmware

The firmware implements a small line-based serial command handler on the active ESP-IDF console.

On the `esp32-s3-dev` build, the console is routed to **USB Serial/JTAG**, so commands/logs go over the same `/dev/ttyACM*` device.

Supported commands:
- `wifi-status`
- `wifi-clear` (clears NVS creds + reboots)
- `wifi-provision <ssid> <password>`
  - Aliases: `wifi-provisioning`, `wifi-provisionning`
- `nvs set owner_name <name>` (sets device owner name in NVS)
- `nvs erase owner_name` (clears device owner name from NVS)
- `nvs get owner_name` (shows current owner name)
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

### `nvs clear-owner`

Clears the device owner name from NVS via serial command. This will trigger the onboarding wizard on next webapp load.

Flags:
- `--port`: serial device path, or `auto`
- `--baud`: default `115200`

Example:
```bash
python3 tools/ots_device_tool.py nvs clear-owner
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
