# WiFi Provisioning (Captive Portal + Web UI + Serial Commands)

This firmware supports WiFi provisioning without reflashing.

It provides:
- A **WiFi config web page** (HTTP, port 80) to set/clear credentials.
- A lightweight **captive portal mode** (SoftAP) when provisioning is required.
- A **captive DNS** responder (UDP/53) that answers all DNS queries with the AP IP.
- An **HTTP redirect** for unknown paths to the config page while in portal mode.
- **UART0 serial commands** to inspect/clear/set credentials.

## Storage

WiFi credentials are stored in **NVS** (flash).

- Namespace: `wifi`
- Keys: `ssid`, `password`

Implementation: `wifi_credentials.c`.

## Boot + Fallback Workflow

At boot:

1. Firmware checks whether stored WiFi credentials exist.
2. If credentials are missing, it enters **portal mode** (SoftAP) immediately.
3. If credentials exist, it starts normal **STA mode** and attempts to connect.
4. If STA mode fails to connect after **3 retries**, the firmware switches to **portal mode**.

In portal mode:
- The **WebSocket server does not start**.
- The WiFi config web UI is available over HTTP.

Key logic:
- STA retry threshold: `NETWORK_MANAGER_MAX_STA_RETRIES` (currently 3) in `network_manager.c`.
- WS gating: only starts when *not* in portal mode.

## Captive Portal Behavior

### Access Point

When provisioning is required, the firmware starts a SoftAP:
- SSID: `OTS-SETUP`
- Password: empty (open AP)

### Captive DNS (UDP/53)

While in portal mode, the firmware starts a minimal DNS server that:
- Listens on **UDP port 53**
- Responds to queries with an **A record pointing to 192.168.4.1** (default ESP-IDF SoftAP IP)

This makes most devices resolve any hostname to the portal IP.

### HTTP Redirect

While in portal mode, unknown HTTP paths are redirected:
- Any `404` is returned as `302 Found` with `Location: /wifi`

This helps OS captive portal detection probes land on the provisioning page.

## WiFi Config Web UI (HTTP)

The WiFi config server listens on port **80**.

Endpoints:
- `GET /` and `GET /wifi` — shows the setup form
- `POST /wifi` — saves `ssid` + `password` to NVS and reboots
- `POST /wifi/clear` — clears saved credentials and reboots

Notes:
- Form parsing is intentionally minimal:
  - Supports `+` → space
  - Does not decode `%xx`

Implementation: `webapp_server.c`.

## Serial Commands (UART0)

The firmware installs a UART0 reader task and accepts line-based commands.

Supported commands:
- `wifi-status`
  - Prints whether credentials are stored and the SSID.
- `wifi-clear`
  - Clears stored credentials (NVS) and reboots.
- `wifi-provision <ssid> <password>`
  - Saves credentials and reboots.
  - Aliases: `wifi-provisioning`, `wifi-provisionning`

Implementation: `serial_commands.c`.

### Example session

```text
wifi-status
wifi-provision MySSID MyPassword
# device reboots
wifi-status
wifi-clear
# device reboots into portal mode
```

## Troubleshooting

- If you connect to `OTS-SETUP` but don’t see a page:
  - Browse to `http://192.168.4.1/wifi`
- If a phone/laptop doesn’t auto-open the captive portal:
  - Some OSes require HTTPS success checks; this portal is HTTP-only.
  - Manually open `http://192.168.4.1/`.
- If you need to change the AP IP away from `192.168.4.1`:
  - Update the SoftAP netif IP configuration and also update the captive DNS response IP.
