# Development Setup - OTS Project

## Overview

This guide walks you through setting up a complete development environment for the OpenFront Tactical Suitcase (OTS) project. The setup includes three main components:
- **ots-userscript**: TypeScript Tampermonkey userscript
- **ots-fw-main**: ESP32-S3 firmware (C/ESP-IDF)
- **ots-server**: Nuxt 4 dashboard and WebSocket server

## Prerequisites

### Required Software

#### 1. Node.js and npm
```bash
# Ubuntu/Debian
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs

# macOS (Homebrew)
brew install node@20

# Verify installation
node --version  # Should be v20.x or higher
npm --version   # Should be v10.x or higher
```

#### 2. Git
```bash
# Ubuntu/Debian
sudo apt-get install git

# macOS
brew install git

# Configure
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

#### 3. Python 3.8+
Required for PlatformIO and ESP-IDF:
```bash
# Ubuntu/Debian
sudo apt-get install python3 python3-pip python3-venv

# macOS
brew install python@3.11

# Verify
python3 --version  # Should be 3.8 or higher
```

#### 4. PlatformIO Core (for firmware)
```bash
# Install via pip
pip3 install platformio

# Or via installer script
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py

# Verify
pio --version  # Should be 6.x or higher
```

#### 5. ESP-IDF (Optional - PlatformIO handles this automatically)
Only needed if using `idf.py` directly instead of PlatformIO:
```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

### Optional Tools

#### VS Code Extensions
```bash
# Install VS Code first
# Ubuntu
sudo snap install code --classic

# macOS
brew install --cask visual-studio-code
```

**Recommended Extensions:**
- **PlatformIO IDE** (`platformio.platformio-ide`)
- **ESLint** (`dbaeumer.vscode-eslint`)
- **Volar** (`Vue.volar`) - Vue 3 support
- **GitHub Copilot** (`github.copilot`) - AI assistance
- **GitLens** (`eamonn.gitlens`) - Git integration
- **Prettier** (`esbenp.prettier-vscode`) - Code formatter
- **Tampermonkey** - Browser extension for userscript testing

#### Browser Setup
For userscript development:
```bash
# Chrome/Edge: Install Tampermonkey
# https://chrome.google.com/webstore/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo

# Firefox: Install Tampermonkey
# https://addons.mozilla.org/en-US/firefox/addon/tampermonkey/
```

## Repository Setup

### 1. Clone Repository

```bash
cd ~/dev  # or your preferred workspace directory
git clone <repository-url> ots
cd ots
```

### 2. Verify Project Structure

```bash
ls -la
# Should see:
# - ots-userscript/
# - ots-fw-main/
# - ots-server/
# - ots-shared/
# - ots-hardware/
# - prompts/
# - release.sh
# - weekly_announces.md
```

## Component Setup

### A. Userscript (`ots-userscript`)

#### 1. Install Dependencies
```bash
cd ots-userscript
npm install
```

#### 2. Build Userscript
```bash
npm run build
# Output: build/userscript.ots.user.js
```

#### 3. Install in Browser
1. Open Tampermonkey dashboard in your browser
2. Click **"+"** to create new script
3. Delete default content
4. Open `build/userscript.ots.user.js` and copy contents
5. Paste into Tampermonkey editor
6. Save (Ctrl+S or Cmd+S)

**Or drag-and-drop:**
- Drag `build/userscript.ots.user.js` into browser window
- Tampermonkey will prompt to install

#### 4. Test Userscript
```bash
# Open browser to OpenFront.io
# Userscript should inject HUD in top-right corner
# Console should show: [OTS Userscript] Version X.X.X
```

#### 5. Development Workflow
```bash
# Watch mode (auto-rebuild on changes)
npm run watch

# Edit src/main.user.ts
# Save file
# Refresh browser page to test changes
```

### B. Server (`ots-server`)

#### 1. Install Dependencies
```bash
cd ots-server
npm install
```

#### 2. Configure Environment (Optional)
```bash
# Create .env file if needed
cat > .env << EOF
# Server configuration
PORT=3000
NODE_ENV=development
EOF
```

#### 3. Start Development Server
```bash
npm run dev
# Server starts at http://localhost:3000
```

#### 4. Build for Production
```bash
npm run build
# Output: .output/ directory

# Test production build
node .output/server/index.mjs
```

#### 5. Verify Dashboard
Open browser to http://localhost:3000:
- Should see "Game Dashboard" header
- UI WS status pill (green = connected)
- USERSCRIPT status pill (initially red)
- Hardware module emulators (Nuke, Alert, Main Power)
- Recent events list (empty initially)

### C. Firmware (`ots-fw-main`)

#### 1. Install PlatformIO Dependencies
```bash
cd ots-fw-main

# PlatformIO will auto-download ESP32-S3 toolchain
# on first build (takes 5-10 minutes)
```

#### 2. Configure WiFi Credentials
```bash
# Edit include/config.h
vim include/config.h

# Update these lines:
# #define WIFI_SSID "YourWiFiName"
# #define WIFI_PASSWORD "YourWiFiPassword"
# #define WS_SERVER_HOST "192.168.1.100"  // Your PC's IP
```

#### 3. Build Firmware
```bash
# Clean build (recommended first time)
pio run -t clean
pio run

# Build takes 2-5 minutes on first run
# Output: .pio/build/esp32-s3-dev/firmware.bin
```

#### 4. Connect Hardware
```bash
# Connect ESP32-S3 dev board via USB

# Check device detected
pio device list
# Should show /dev/ttyUSB0 (Linux) or /dev/cu.usbserial-* (macOS)
```

#### 5. Flash Firmware
```bash
# Upload firmware to device
pio run -t upload

# Upload and monitor serial output
pio run -t upload && pio device monitor
```

#### 6. Monitor Serial Output
```bash
# View logs in real-time
pio device monitor

# Press Ctrl+] to exit

# Expected output:
# [MAIN] OTS Firmware v2025-12-20.1
# [NETWORK] Connecting to WiFi...
# [NETWORK] Connected! IP: 192.168.1.50
# [WS] Connecting to ws://192.168.1.100:3000/ws
# [WS] Connected to server
```

#### 7. Development Workflow
```bash
# Edit firmware source files
vim src/main.c

# Build and flash
pio run -t upload

# Monitor output
pio device monitor
```

### D. Shared Types (`ots-shared`)

#### 1. Install Dependencies
```bash
cd ots-shared
npm install
```

#### 2. Build Types (if needed)
```bash
npm run build
# Outputs TypeScript declarations
```

**Note:** `ots-shared` is imported directly by server and userscript via relative paths. No separate build needed unless publishing as package.

## Testing the Complete System

### 1. Start All Components

**Terminal 1 - Server:**
```bash
cd ots-server
npm run dev
# Server at http://localhost:3000
```

**Terminal 2 - Firmware (if hardware available):**
```bash
cd ots-fw-main
pio device monitor
# Watch serial logs
```

**Terminal 3 - Userscript Development:**
```bash
cd ots-userscript
npm run watch
# Auto-rebuilds on changes
```

### 2. Open Game in Browser
1. Navigate to https://openfront.io/
2. Start or join a game
3. Userscript HUD should appear in top-right
4. Check console for connection logs

### 3. Verify WebSocket Connections

**In Server Dashboard (http://localhost:3000):**
- UI WS pill: Green (dashboard connected)
- USERSCRIPT pill: Green (userscript connected)
- Events list: Should populate with game events

**In Browser Console:**
```javascript
// Should see:
[OTS Userscript] Version 2025-12-20.1
[WS] Connected to ws://localhost:3000/ws
[WS] Handshake sent
```

**In Firmware Serial Monitor:**
```
[WS] Connected to server
[EVENT] GAME_START received
[NUKE] Button pressed - launching ATOM
```

### 4. Test Event Flow

**Scenario 1: Nuke Launch**
1. In game, launch a nuke
2. Userscript detects launch → sends event to server
3. Server dashboard shows "NUKE_LAUNCHED" in events list
4. Firmware receives event → blinks LED

**Scenario 2: Button Press (if hardware)**
1. Press nuke button on physical device
2. Firmware sends event to server
3. Dashboard shows event
4. Game receives launch command (future)

## Troubleshooting

### Userscript Issues

**Problem: Userscript not loading**
```bash
# Check Tampermonkey dashboard
# Enable userscript if disabled
# Check match pattern: https://openfront.io/*

# Verify build output exists
ls -la ots-userscript/build/userscript.ots.user.js
```

**Problem: HUD not appearing**
```bash
# Open browser console (F12)
# Look for errors
# Check userscript is active (Tampermonkey icon shows "1")
```

**Problem: WebSocket connection fails**
```bash
# Check server is running: http://localhost:3000
# Check WebSocket URL in userscript
# Default: ws://localhost:3000/ws
# Edit src/main.user.ts if different
```

### Server Issues

**Problem: Port already in use**
```bash
# Find process using port 3000
lsof -i :3000  # macOS/Linux
netstat -ano | findstr :3000  # Windows

# Kill process or change port in nuxt.config.ts
```

**Problem: npm install fails**
```bash
# Clear npm cache
npm cache clean --force
rm -rf node_modules package-lock.json
npm install
```

### Firmware Issues

**Problem: Device not detected**
```bash
# Linux: Check USB permissions
sudo usermod -a -G dialout $USER
# Log out and back in

# Check device path
ls -la /dev/ttyUSB*
ls -la /dev/ttyACM*

# Specify port manually
pio run -t upload --upload-port /dev/ttyUSB0
```

**Problem: Flash fails**
```bash
# Hold BOOT button on ESP32-S3 during upload
# Release after "Connecting..." appears

# Or use esptool directly
esptool.py --port /dev/ttyUSB0 erase_flash
pio run -t upload
```

**Problem: Serial monitor shows garbage**
```bash
# Check baud rate (should be 115200)
pio device monitor -b 115200

# Or in platformio.ini:
# monitor_speed = 115200
```

**Problem: WiFi connection fails**
```bash
# Check credentials in include/config.h
# Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
# Check IP address is reachable
ping 192.168.1.100  # Your server IP
```

### Build Issues

**Problem: TypeScript errors in userscript**
```bash
# Check TypeScript version
npm list typescript

# Reinstall dependencies
rm -rf node_modules package-lock.json
npm install
```

**Problem: PlatformIO build errors**
```bash
# Clean build artifacts
pio run -t clean

# Update PlatformIO
pio upgrade

# Reinstall platform
pio platform uninstall espressif32
pio platform install espressif32
```

## IDE Configuration

### VS Code Settings

Create `.vscode/settings.json` in project root:
```json
{
  "editor.formatOnSave": true,
  "editor.codeActionsOnSave": {
    "source.fixAll.eslint": true
  },
  "eslint.workingDirectories": [
    "ots-userscript",
    "ots-server"
  ],
  "files.exclude": {
    "**/node_modules": true,
    "**/.pio": true,
    "**/build": true,
    "**/.output": true
  },
  "search.exclude": {
    "**/node_modules": true,
    "**/.pio": true,
    "**/build": true,
    "**/.output": true
  },
  "[typescript]": {
    "editor.defaultFormatter": "esbenp.prettier-vscode"
  },
  "[vue]": {
    "editor.defaultFormatter": "Vue.volar"
  },
  "C_Cpp.default.configurationProvider": "platformio-ide"
}
```

### VS Code Launch Configuration

Create `.vscode/launch.json`:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "PlatformIO Debug",
      "type": "platformio-debug",
      "request": "launch",
      "cwd": "${workspaceFolder}/ots-fw-main"
    }
  ]
}
```

## Environment Variables

### Server Environment

Create `ots-server/.env`:
```bash
# Server Configuration
PORT=3000
NODE_ENV=development

# Future: Database connection
# DATABASE_URL=postgresql://user:pass@localhost:5432/ots

# Future: Authentication
# JWT_SECRET=your-secret-key
```

### Firmware Configuration

Edit `ots-fw-main/include/config.h`:
```c
// WiFi Configuration
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

// WebSocket Server
#define WS_SERVER_HOST "192.168.1.100"  // Your PC's IP
#define WS_SERVER_PORT 3000
#define WS_SERVER_PATH "/ws"

// OTA Configuration
#define OTA_HOSTNAME "ots-fw-main"
#define OTA_PASSWORD "ots2025"  // CHANGE THIS!
#define OTA_PORT 3232

// Hardware Configuration
#define I2C_SDA_PIN GPIO_NUM_8
#define I2C_SCL_PIN GPIO_NUM_9
#define I2C_FREQ_HZ 100000
```

## Quick Start Checklist

### First Time Setup
- [ ] Install Node.js 20+
- [ ] Install Python 3.8+
- [ ] Install PlatformIO
- [ ] Install Git
- [ ] Clone repository
- [ ] Install Tampermonkey browser extension
- [ ] Configure WiFi credentials in firmware

### Userscript Development
- [ ] `cd ots-userscript && npm install`
- [ ] `npm run build`
- [ ] Install in Tampermonkey
- [ ] Test on https://openfront.io/

### Server Development
- [ ] `cd ots-server && npm install`
- [ ] `npm run dev`
- [ ] Open http://localhost:3000
- [ ] Verify dashboard loads

### Firmware Development
- [ ] `cd ots-fw-main`
- [ ] Update WiFi config in `include/config.h`
- [ ] `pio run` (builds firmware)
- [ ] Connect ESP32-S3 via USB
- [ ] `pio run -t upload` (flashes device)
- [ ] `pio device monitor` (view logs)

## Next Steps

Once your environment is set up:
1. Read [`prompts/GIT_WORKFLOW.md`](GIT_WORKFLOW.md) for contribution guidelines
2. Review [`prompts/protocol-context.md`](protocol-context.md) for WebSocket protocol
3. Check [`prompts/RELEASE.md`](RELEASE.md) for release procedures
4. Explore component-specific docs:
   - `ots-userscript/copilot-project-context.md`
   - `ots-server/copilot-project-context.md`
   - `ots-fw-main/copilot-project-context.md`

## Resources

- **PlatformIO Docs**: https://docs.platformio.org/
- **ESP-IDF Guide**: https://docs.espressif.com/projects/esp-idf/
- **Nuxt 4 Docs**: https://nuxt.com/docs
- **Tampermonkey**: https://www.tampermonkey.net/documentation.php
- **OpenFront.io**: https://openfront.io/ (game website)

## Getting Help

If you encounter issues not covered here:
1. Check `prompts/TROUBLESHOOTING.md` (if it exists)
2. Review error messages carefully
3. Search existing issues/documentation
4. Ask for help with specific error details

## Contributing

Before making changes:
1. Read [`prompts/GIT_WORKFLOW.md`](GIT_WORKFLOW.md)
2. Follow commit message conventions
3. Test all changes locally
4. Update documentation if needed
