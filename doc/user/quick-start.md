# Quick Start Guide

Get your OTS device up and running in 10 minutes!

## 📦 What You'll Need

- Your OTS device
- Power supply (USB-C cable + adapter)
- Computer with web browser (Chrome/Firefox recommended)
- WiFi network (2.4GHz or 5GHz)
- Active OpenFront.io game account

## ⚡ Step 1: Power On (1 minute)

1. **Connect power** - Plug USB-C cable into your OTS device
2. **Watch the LINK LED** on the Main Power module:
   - **Blinking** = Device is starting up
   - **Off** = Waiting for WiFi connection
3. **Device is ready** when LEDs stabilize

## 📡 Step 2: Connect to WiFi (3 minutes)

When your device first powers on, it creates its own WiFi network:

1. **Look for WiFi network**: `OTS-XXXXXX` (where XXXXXX is unique to your device)
2. **Connect** to this network from your computer/phone
3. **Open browser** and go to: `http://192.168.4.1`
4. **Enter your WiFi credentials**:
   - Network name (SSID)
   - Password
   - Click "Connect"
5. **Wait 30 seconds** for device to connect
6. **LINK LED turns solid green** = Connected successfully!

**Can't find the WiFi network?** See [WiFi Setup Guide](wifi-setup.md) for troubleshooting.

## 🌐 Step 3: Install Userscript (3 minutes)

The userscript bridges your browser game to the hardware:

### Install Tampermonkey

1. Install [Tampermonkey browser extension](https://www.tampermonkey.net/):
   - **Chrome**: [Chrome Web Store](https://chrome.google.com/webstore/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo)
   - **Firefox**: [Firefox Add-ons](https://addons.mozilla.org/en-US/firefox/addon/tampermonkey/)

### Install OTS Userscript

2. **Download userscript**: Get `userscript.ots.user.js` from:
   - Your device setup package, OR
   - Build from source (see [Developer Guide](../developer/userscript-development.md))

3. **Install**:
   - Open the `.user.js` file in your browser, OR
   - Drag and drop the file into browser window
   - Tampermonkey will prompt to install
   - Click "Install"

4. **Configure connection**:
   - The userscript opens automatically on OpenFront.io
   - Click the **⚙ gear icon** in the HUD
   - Enter your device IP address: `ws://[device-ip]:3000/ws`
   - Click "Save"

**Find your device IP**: Check your router's admin panel or use network scanner app.

## 🎮 Step 4: Start Playing! (1 minute)

1. **Open OpenFront.io** in your browser
2. **Start or join a game**
3. **Watch the HUD** appear (top-right corner)
4. **Connection status** should show:
   - **WS: OPEN** (green) = Connected to device
   - **GAME: CONNECTED** (green) = In active game

### Test Your Device

Try these quick tests:

**Nuke Module:**
- Press **ATOM button** → Button LED should blink (10 seconds)
- Watch userscript HUD show "NUKE_LAUNCHED" event

**Alert Module:**
- Wait for incoming nuke in game
- Watch **ATOM/HYDRO/MIRV LED** blink on device
- Watch userscript HUD show "ALERT_*" event

**Troops Module:**
- LCD displays: `[Current] / [Max]` troops
- Move slider → Percentage updates
- Command sent to game

## ✅ You're Ready!

Your OTS device is now connected and operational. Here's what happens automatically:

- **Button presses** → Send commands to game
- **Game events** → Light up LEDs
- **Troop counts** → Update LCD display
- **Incoming threats** → Trigger alerts

## 📖 Next Steps

- **[Device Guide](device-guide.md)** - Learn about each module
- **[Userscript Interface](userscript-interface.md)** - Explore HUD features
- **[Troubleshooting](troubleshooting.md)** - Fix common issues

## 🆘 Having Issues?

### Device Won't Connect to WiFi
→ See [WiFi Setup Guide](wifi-setup.md)

### Userscript Not Working
→ Check [Userscript Installation](userscript-install.md)

### LEDs Not Responding
→ See [Troubleshooting Guide](troubleshooting.md)

### Still Stuck?
→ Check [FAQ](FAQ.md) or contact support

---

**Last Updated**: January 2026  
**Firmware Version**: 0.1.0+
