# WiFi Config Web App (Embedded)

The firmware serves an embedded static web app for WiFi configuration.

It works in both:
- **Captive portal mode** (SoftAP + captive DNS)
- **Normal STA mode** (device connected to your WiFi)

## Endpoints

- `GET /` → WiFi setup UI
- `GET /device` → JSON device/status (mode, IP, saved SSID, owner name, serial, firmware version)
- `POST /device` → Save owner name (onboarding)
- `GET /api/status` → JSON status (legacy: mode/IP/saved SSID)
- `GET /api/scan` → JSON scan results (SSIDs/RSSI/auth)
- `POST /wifi` → Save SSID/password and reboot
- `POST /wifi/clear` → Clear stored credentials and reboot

## Onboarding

When the device boots into captive portal mode (no stored WiFi credentials), the UI shows a simple onboarding wizard:

1. Ask for the owner's name (stored in NVS)
2. WiFi setup (save SSID/password and reboot)

Once the device is in working STA mode, onboarding is hidden and the UI switches to tabbed views:
- **WiFi Setup**
- **Device Info**
- **About**

The onboarding wizard will also appear in STA mode if the owner name is not set in NVS, even if WiFi credentials are configured.

### Testing Onboarding

To reset the device and trigger the onboarding wizard again:

```bash
# Clear owner name via serial command
python3 tools/ots_device_tool.py nvs clear-owner --port /dev/ttyACM0

# Or via direct serial command
nvs erase owner_name
```

This is useful for testing the onboarding flow without clearing WiFi credentials.

## Build / Embed Pipeline

Static files live in [webapp/](../webapp):
- `index.html`
- `style.css`
- `app.js`

They are embedded into the firmware as a gzip-compressed C header:
- [include/webapp/ots_webapp.h](../include/webapp/ots_webapp.h)

### Build Hash Verification

Every webapp build includes a unique SHA256 hash (first 16 chars):
- Injected during `embed_webapp.py` execution
- Displayed in webapp footer: `Build: [hash]`
- Available in JavaScript: `window.OTS_BUILD_HASH`
- **Always check this hash** to confirm you're seeing the latest version

### Regenerate Manually

**CRITICAL:** After editing any webapp files, you MUST run:

```bash
cd ots-fw-main/
python3 tools/embed_webapp.py
```

Then rebuild and upload firmware:
```bash
pio run -e esp32-s3-dev -t upload
```

**Verify the build hash** in the browser footer to confirm changes were applied.

### Regenerate Automatically (PlatformIO)

PlatformIO builds run a pre-build script that regenerates the header automatically.

## Notes

- `/api/scan` may take a couple seconds while the WiFi scan runs.
- In captive portal mode, unknown paths are redirected to `/`.
