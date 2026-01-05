# Userscript Installation Guide

Install the OTS userscript to connect your browser to your hardware device.

## 📋 Prerequisites

- OTS device connected to WiFi ([WiFi Setup](wifi-setup.md))
- Device IP address (from router or serial console)
- Modern web browser (Chrome, Firefox, Edge, Safari)
- OpenFront.io account

## 🔧 Step 1: Install Tampermonkey

The userscript requires Tampermonkey browser extension to run.

### Chrome / Edge / Brave
1. Open [Chrome Web Store](https://chrome.google.com/webstore/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo)
2. Click **"Add to Chrome"**
3. Confirm installation
4. Tampermonkey icon appears in toolbar

### Firefox
1. Open [Firefox Add-ons](https://addons.mozilla.org/en-US/firefox/addon/tampermonkey/)
2. Click **"Add to Firefox"**
3. Confirm installation
4. Tampermonkey icon appears in toolbar

### Safari
1. Open [App Store](https://apps.apple.com/us/app/tampermonkey/id1482490089)
2. Install Tampermonkey for Safari
3. Enable in Safari Extensions preferences

## 📦 Step 2: Get OTS Userscript

### Option A: Pre-built Release (Recommended)

1. **Download** the latest userscript:
   - From your device setup package: `userscript.ots.user.js`
   - Or from releases: [GitHub Releases](https://github.com/yourusername/ots/releases)

2. **Install**:
   - Simply **open** the `.user.js` file in your browser, OR
   - **Drag and drop** the file into browser window
   - Tampermonkey automatically detects and prompts installation

3. **Confirm installation**:
   - Click **"Install"** in Tampermonkey popup
   - Script is now active

### Option B: Build from Source

For developers or latest features:

```bash
cd ots-userscript
npm install
npm run build
```

Output: `build/userscript.ots.user.js`

Follow Option A instructions to install.

## ⚙️ Step 3: Configure Connection

### Find Your Device IP

**Method 1: Router Admin Panel**
- Check connected devices
- Look for "OTS-XXXXXX" or "ots-fw-main"
- Note the IP (e.g., `192.168.1.100`)

**Method 2: mDNS** (if supported)
- Try: `ots-fw-main.local`

**Method 3: Serial Console**
- Connect USB cable
- Device logs IP on connection

### Configure WebSocket URL

1. **Open OpenFront.io** in your browser
2. **Wait for HUD** to appear (top-right corner)
3. **Click ⚙ gear icon** (settings button)
4. **Enter WebSocket URL**:
   ```
   ws://[device-ip]:3000/ws
   ```
   
   Examples:
   - `ws://192.168.1.100:3000/ws`
   - `ws://ots-fw-main.local:3000/ws`

5. **Click "Save"**
6. **Connection status** updates:
   - **WS: CONNECTING** (amber) → Attempting connection
   - **WS: OPEN** (green) → Connected successfully!

## ✅ Verify Installation

### Check Connection Status

The HUD header shows two status pills:

**WS (WebSocket)**:
- 🔴 **DISCONNECTED** - Can't reach device
- 🟡 **CONNECTING** - Attempting connection
- 🟢 **OPEN** - Connected to device
- 🔴 **ERROR** - Connection error

**GAME (Game State)**:
- 🔴 **DISCONNECTED** - Not in game
- 🟢 **CONNECTED** - In active game

### Test Basic Functionality

1. **Start or join** an OpenFront.io game
2. **Open Logs tab** in HUD
3. **Watch for events**:
   - `INFO` - Connection heartbeats
   - `GAME_START` - Game begins
   - `TROOPS` updates - Troop counts

4. **Test hardware** (if device connected):
   - Press nuke button → See `NUKE_LAUNCHED` event
   - Incoming threat → See `ALERT_*` event

## 🐛 Troubleshooting

### HUD Doesn't Appear

**Symptoms**: No userscript interface on OpenFront.io

**Solutions**:
1. **Check Tampermonkey**:
   - Click Tampermonkey icon
   - Verify script is **enabled**
   - Check if running on current page

2. **Verify page match**:
   - Script only runs on `https://openfront.io/*`
   - Reload page (Ctrl+R / Cmd+R)

3. **Check browser console** (F12):
   - Look for script errors
   - Check if Tampermonkey blocked by extensions

4. **Reinstall script**:
   - Delete current script in Tampermonkey dashboard
   - Reinstall from `.user.js` file

### Can't Connect to Device

**Symptoms**: WS status stays "DISCONNECTED" or "ERROR"

**Solutions**:
1. **Check device IP**:
   - Verify IP address is correct
   - Try pinging device: `ping [device-ip]`

2. **Check WebSocket URL format**:
   - Must start with `ws://` (not `http://` or `wss://`)
   - Must include port `:3000`
   - Must end with `/ws`
   - Example: `ws://192.168.1.100:3000/ws`

3. **Check network**:
   - Computer and device on same network
   - No VPN blocking local connections
   - Firewall allowing port 3000

4. **Check device**:
   - LINK LED is solid green (connected to WiFi)
   - Device is powered on
   - Firmware is running (check serial logs)

### Connection Drops Frequently

**Symptoms**: WS status flashes OPEN → DISCONNECTED repeatedly

**Solutions**:
1. **Check WiFi signal**: Move device closer to router
2. **Check power**: Use quality USB adapter (2A minimum)
3. **Check browser**: Close unnecessary tabs/extensions
4. **Check firmware**: Update to latest version

### Events Not Showing

**Symptoms**: Connected but no events in Logs tab

**Solutions**:
1. **Join active game**: Userscript only works in-game
2. **Check filters**: Ensure log filters are enabled
3. **Wait for events**: Some events only trigger on actions
4. **Test manually**: Press device buttons to trigger events

### Script Conflicts

**Symptoms**: Other OpenFront.io extensions interfere

**Solutions**:
1. **Disable conflicting scripts** temporarily
2. **Check Tampermonkey priorities**
3. **Report compatibility issues**

## 🔐 Security & Privacy

### What the Userscript Does
- ✅ Monitors OpenFront.io game state (read-only)
- ✅ Sends game events to your local device
- ✅ Receives commands from your local device
- ❌ Does NOT modify game behavior
- ❌ Does NOT send data to external servers
- ❌ Does NOT access other websites

### Network Security
- **Local only**: WebSocket connects to your LAN device
- **No cloud**: All data stays on your network
- **Open source**: Code is auditable

### Permissions
- Tampermonkey grants: `@match https://openfront.io/*`
- GM storage: `GM_getValue`, `GM_setValue` (settings only)
- No external network requests

## 🔄 Updating Userscript

### Check for Updates

1. **Manual check**:
   - Download latest `.user.js` file
   - Install over existing script (auto-updates)

2. **Tampermonkey auto-update**:
   - Checks daily for updates (if configured)
   - Prompts to install new version

### Version Information

Check current version:
- Click Tampermonkey icon → Dashboard
- Find "OTS Userscript"
- Version shown below name

## 📱 Mobile Support

### Android
- Install **Kiwi Browser** (supports Chrome extensions)
- Install Tampermonkey from Chrome Web Store
- Follow desktop instructions

### iOS
- Install **Safari** + Tampermonkey extension
- Limited support (iOS restrictions)
- Desktop recommended for best experience

## 📖 Next Steps

- **[Userscript Interface](userscript-interface.md)** - Learn the HUD features
- **[Device Guide](device-guide.md)** - Understand hardware modules
- **[Troubleshooting](troubleshooting.md)** - Fix common issues

---

**Last Updated**: January 2026  
**Userscript Version**: 2025-12-31.1+
