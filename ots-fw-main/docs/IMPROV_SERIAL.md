# Improv Serial WiFi Provisioning (Removed)

Improv Serial provisioning was removed from `ots-fw-main`.

The firmware now uses a simpler provisioning flow:
- Captive portal mode (SoftAP) when credentials are missing or STA connection fails repeatedly
- WiFi config web UI on port 80 (`/wifi`)
- UART0 serial commands (`wifi-status`, `wifi-clear`, `wifi-provision ...`)

See: `docs/WIFI_PROVISIONING.md`
