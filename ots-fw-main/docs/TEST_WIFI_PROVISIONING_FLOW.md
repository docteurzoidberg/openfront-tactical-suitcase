# Test: WiFi Provisioning Flow (Serial + Captive Portal + STA + Web UI)

This document describes an end-to-end test that validates the firmware’s WiFi provisioning workflow.

The test is implemented as a Python script in `tools/tests/` and uses:
- `tools/ots_device_tool.py` for sending serial commands
- Serial log assertions for reboot/mode checks
- HTTP checks against the WiFi config web server in STA mode

## Test goals

Validate all of the following:

1. Clearing saved WiFi credentials via serial works and the device reboots.
2. After reboot, the firmware enters **captive portal mode** (SoftAP provisioning mode).
3. Provisioning credentials via serial works and the device reboots.
4. After reboot, the firmware connects in **STA mode** to the provided SSID and obtains an IP.
5. In STA mode, the WiFi config web server is reachable at `http://<device-ip>/wifi` and the credential-change route works.

## Preconditions / assumptions

- You have a device flashed with current firmware.
- You know the host machine’s serial port path (e.g. `/dev/ttyACM0`).
- The SSID/password you provide are reachable from the device.
- SSID/password **must not include** `&`, `=`, `%`, or spaces (the WiFi config form parser is intentionally minimal and does not decode `%xx`).
- Your host machine can reach the device over the LAN once it joins STA WiFi.

Notes:
- During captive portal (SoftAP) mode, your host is typically **not** connected to the device AP, so the HTTP config page may not be reachable from your host. This test therefore confirms portal mode via **serial logs**.

## Observability contract (serial logs)

The test relies on these log patterns:

- Reboot marker:
  - `OTS_MAIN: ============================================`

- Entering portal mode:
  - `No stored WiFi credentials (NVS clear); starting captive portal mode`
  - `Starting captive portal AP: OTS-SETUP`

- STA connection:
  - `WiFi started, connecting to <SSID>`
  - `[WiFi] STA_CONNECTED` (or equivalent)
  - `[IP] GOT_IP: <ip>`

- WiFi config web server:
  - `WiFi config HTTP server started on port 80`

If these strings change in firmware, update the test regex patterns.

## Web UI checks (STA mode)

After the device obtains an IP in STA mode:

1. `GET http://<ip>/wifi`
   - Expected `200 OK`
   - Body contains `OTS WiFi Setup`

2. `POST http://<ip>/wifi` with `application/x-www-form-urlencoded`
   - Body includes `ssid=<SSID>&password=<PASSWORD>`
   - Expected `200 OK` and response contains `Saved. Rebooting...`
   - The device should reboot and return to STA mode.

## Running the test

From `ots-fw-main/`:

```bash
python3 tools/tests/wifi_provisioning_flow.py \
  --serial-port /dev/ttyACM0 \
  --ssid "MySSID" \
  --password "MyPassword"
```

Optional flags:
- `--serial-baud` (default `115200`)
- `--ota-port` (default `3232`, not required for this test)
- `--http-timeout` (default small, e.g. `5s`)

## Expected outcomes

- The script prints `PASS:` lines for each stage.
- On failure, the script prints a serial tail to help diagnose what phase failed.
