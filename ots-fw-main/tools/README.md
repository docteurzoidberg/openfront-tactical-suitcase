# OTS Firmware Tools

Quick reference for development tools in this directory.

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

**Purpose:** Comprehensive device management CLI (serial + OTA).

**Common Commands:**

### Serial Monitor
```bash
python3 tools/ots_device_tool.py serial monitor --port auto
```

### Send Commands
```bash
# WiFi status
python3 tools/ots_device_tool.py serial send --port auto --cmd "wifi-status"

# Reboot
python3 tools/ots_device_tool.py serial reboot --port auto
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

### OTA Upload
```bash
python3 tools/ots_device_tool.py ota upload \
  --host 192.168.1.50 \
  --bin .pio/build/esp32-s3-dev/firmware.bin
```

See [../docs/OTS_DEVICE_TOOL.md](../docs/OTS_DEVICE_TOOL.md) for full documentation.

## reset_owner.py

**Purpose:** HTTP-based owner name reset (alternative to serial method).

**Usage:**
```bash
python3 tools/reset_owner.py 192.168.77.153
```

**When to use:** When device is accessible via HTTP but serial is unavailable.

**Equivalent to:** `python3 tools/ots_device_tool.py nvs clear-owner --port auto`
