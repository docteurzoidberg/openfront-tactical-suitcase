# OTS Firmware Tools

Development tools for OTS firmware development and device management.

## Tools Overview

- **embed_webapp.py** - Generates C header from webapp files with build hash injection
- **ots_device_tool.py** - Comprehensive device management CLI (serial monitor, OTA uploads, NVS management)
- **tests/** - Test scripts for firmware validation

## embed_webapp.py

**Purpose:** Generates C header from webapp files with build hash injection.

**When to use:** ALWAYS after editing files in `webapp/` directory.

**Usage:**
```bash
cd ots-fw-main/
python3 tools/embed_webapp.py
```

**What it does:**
1. Reads all files from `webapp/` directory
2. Computes SHA256 hash of all assets
3. Injects hash into HTML (replaces `__OTS_BUILD_HASH__` placeholder)
4. Gzip-compresses all files
5. Generates `include/webapp/ots_webapp.h` with C arrays

**Build Hash Verification:**
- Hash is displayed in webapp footer: `Build: [hash]`
- Available in JavaScript: `window.OTS_BUILD_HASH`
- Use this to confirm you're seeing the latest version

**Critical Workflow:**
```bash
# 1. Edit webapp files
vim webapp/app.js

# 2. Regenerate C header (REQUIRED!)
python3 tools/embed_webapp.py

# 3. Rebuild and upload firmware
pio run -e esp32-s3-dev -t upload

# 4. Verify build hash in browser footer
```

## ots_device_tool.py

**Purpose:** All-in-one device management tool.

**Features:**
- Serial monitoring with timestamps and log file support
- OTA firmware uploads via HTTP
- NVS (non-volatile storage) management
- Device reboot and command sending
- WebSocket testing

**Common Commands:**

### Serial Monitor
```bash
# Auto-detect port
python3 tools/ots_device_tool.py serial monitor --port auto

# With timestamps and log file
python3 tools/ots_device_tool.py serial monitor --port auto --timestamps --log-file logs/test.log
```

### OTA Firmware Upload
```bash
# Upload firmware to device
python3 tools/ots_device_tool.py ota upload \
  --host 192.168.77.153 \
  --port 3232 \
  --bin .pio/build/esp32-s3-dev/firmware.bin

# Using mDNS hostname
python3 tools/ots_device_tool.py ota upload \
  --host ots-fw-main.local \
  --bin .pio/build/esp32-s3-dev/firmware.bin
```

### Device Management
```bash
# Reboot device
python3 tools/ots_device_tool.py serial reboot --port auto

# Send command to device
python3 tools/ots_device_tool.py serial send --port auto --cmd "wifi-status"
```

### NVS Owner Management
```bash
# Set owner name
python3 tools/ots_device_tool.py nvs set-owner --port auto --name "DrZoid"

# Clear owner (triggers onboarding wizard)
python3 tools/ots_device_tool.py nvs clear-owner --port auto

# Get current owner
python3 tools/ots_device_tool.py nvs get owner_name --port auto
```

See [../docs/OTS_DEVICE_TOOL.md](../docs/OTS_DEVICE_TOOL.md) for full documentation.
