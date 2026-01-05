# Development Environment Setup

Comprehensive guide for setting up a complete OTS development environment with all components, IDE configuration, and troubleshooting.

> 📝 **Quick Start?** See [Getting Started](getting-started.md) for a 10-minute setup guide.

## Overview

This guide covers detailed setup for all OTS components:

**Core Components:**
- **ots-userscript**: TypeScript Tampermonkey userscript (game bridge)
- **ots-simulator**: Nuxt 4 dashboard and WebSocket server
- **ots-shared**: Shared TypeScript protocol types

**Firmware Projects:**
- **ots-fw-main**: ESP32-S3 main controller firmware (C/ESP-IDF)
- **ots-fw-audiomodule**: ESP32-A1S audio playback module
- **ots-fw-cantest**: CAN bus testing/debugging tool
- **ots-fw-shared**: Shared firmware components (CAN driver, discovery protocol)

**Documentation & Hardware:**
- **ots-website**: VitePress documentation site (GitHub Pages)
- **ots-hardware**: Hardware specifications and PCB designs
- **doc/**: User and developer documentation source

## Component Setup

### Userscript (`ots-userscript`)

#### Install Dependencies
```bash
cd ots-userscript
npm install
```

#### Build
```bash
npm run build
# Output: build/userscript.ots.user.js
```

#### Install in Browser
1. Open Tampermonkey dashboard
2. Click **"+"** to create new script
3. Copy contents from `build/userscript.ots.user.js`
4. Paste into Tampermonkey editor
5. Save (Ctrl+S or Cmd+S)

**Or drag-and-drop:**
- Drag `build/userscript.ots.user.js` into browser window
- Tampermonkey will prompt to install

#### Development Workflow
```bash
# Watch mode (auto-rebuild on changes)
npm run watch

# Edit src/main.user.ts
# Save file → Auto-rebuild
# Refresh browser page to test
```

### Server (`ots-simulator`)

#### Install Dependencies
```bash
cd ots-simulator
npm install
```

#### Configure Environment (Optional)
```bash
# Create .env file if needed
cat > .env << EOF
# Server configuration
PORT=3000
NODE_ENV=development
EOF
```

#### Start Development Server
```bash
npm run dev
# Server starts at http://localhost:3000
```

#### Build for Production
```bash
npm run build
# Output: .output/ directory

# Test production build
node .output/server/index.mjs
```

#### Verify Dashboard
Open browser to http://localhost:3000:
- Should see "Game Dashboard" header
- UI WS status pill (green = connected)
- USERSCRIPT status pill (initially red)
- Hardware module emulators
- Recent events list (empty initially)

### Firmware (`ots-fw-main`)

#### Configure WiFi
```bash
# Edit include/config.h
vim include/config.h

# Update these lines:
# #define WIFI_SSID "YourWiFiName"
# #define WIFI_PASSWORD "YourWiFiPassword"
# #define WS_SERVER_HOST "192.168.1.100"  // Your PC's IP
```

#### Build
```bash
# Clean build (recommended first time)
pio run -e esp32-s3-dev -t clean
pio run -e esp32-s3-dev

# Build takes 2-5 minutes on first run
# Output: .pio/build/esp32-s3-dev/firmware.bin
```

#### Flash to Device
```bash
# Connect ESP32-S3 dev board via USB

# Check device detected
pio device list
# Should show /dev/ttyUSB0 (Linux) or /dev/cu.usbserial-* (macOS)

# Upload firmware
pio run -e esp32-s3-dev -t upload

# Upload and monitor serial output
pio run -e esp32-s3-dev -t upload && pio device monitor
```

#### Monitor Serial Output
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

### Documentation Site (`ots-website`)

#### Install Dependencies
```bash
cd ots-website
npm install
```

#### Development Server
```bash
npm run dev
# Visit http://localhost:5173
```

#### Sync Documentation
```bash
# Sync docs from ../doc/ folder
npm run sync
```

#### Build for Production
```bash
npm run build
# Output: .vitepress/dist/

# Preview production build
npm run preview
```

## Testing the Complete System

### Start All Components

**Terminal 1 - Server:**
```bash
cd ots-simulator
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

### Verify WebSocket Connections

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

### Test Event Flow

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
    "ots-simulator"
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

### Recommended Extensions

- **PlatformIO IDE** (`platformio.platformio-ide`)
- **ESLint** (`dbaeumer.vscode-eslint`)
- **Volar** (`Vue.volar`) - Vue 3 support
- **GitHub Copilot** (`github.copilot`) - AI assistance
- **GitLens** (`eamonn.gitlens`) - Git integration
- **Prettier** (`esbenp.prettier-vscode`) - Code formatter

## Environment Variables

### Server Environment

Create `ots-simulator/.env`:
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
pio run -e esp32-s3-dev -t upload --upload-port /dev/ttyUSB0
```

**Problem: Flash fails**
```bash
# Hold BOOT button on ESP32-S3 during upload
# Release after "Connecting..." appears

# Or use esptool directly
esptool.py --port /dev/ttyUSB0 erase_flash
pio run -e esp32-s3-dev -t upload
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
pio run -e esp32-s3-dev -t clean

# Update PlatformIO
pio upgrade

# Reinstall platform
pio platform uninstall espressif32
pio platform install espressif32
```

## Next Steps

Once your environment is set up:
1. Read [Git Workflow](git-workflow.md) for contribution guidelines
2. Review [Repository Overview](repository-overview.md) for codebase structure
3. Explore component-specific documentation:
   - `ots-userscript/copilot-project-context.md`
   - `ots-simulator/copilot-project-context.md`
   - `ots-fw-main/copilot-project-context.md`

## Resources

- **PlatformIO Docs**: https://docs.platformio.org/
- **ESP-IDF Guide**: https://docs.espressif.com/projects/esp-idf/
- **Nuxt 4 Docs**: https://nuxt.com/docs
- **Tampermonkey**: https://www.tampermonkey.net/documentation.php
- **OpenFront.io**: https://openfront.io/ (game website)

---

**Last Updated**: January 2026  
**See Also**: [Getting Started](getting-started.md) | [Troubleshooting](troubleshooting.md) | [Repository Overview](repository-overview.md)
