# Developer Getting Started

Set up your development environment to build and modify OTS.

## 📋 Prerequisites

### Required Tools

**System Requirements**:
- **OS**: Linux (recommended), macOS, or Windows (with WSL)
- **RAM**: 8GB minimum, 16GB recommended
- **Disk**: 10GB free space

**Core Tools**:
- **Git**: Version control ([git-scm.com](https://git-scm.com/))
- **Node.js**: v18+ ([nodejs.org](https://nodejs.org/))
- **npm**: v9+ (comes with Node.js)
- **Python**: 3.8+ ([python.org](https://www.python.org/))

**Firmware Development**:
- **PlatformIO**: ESP-IDF toolchain ([platformio.org](https://platformio.org/))
  - PlatformIO Core (CLI) or
  - VS Code + PlatformIO IDE extension
- **ESP-IDF**: v5.1+ (auto-installed by PlatformIO)

**Optional**:
- **VS Code**: Recommended IDE ([code.visualstudio.com](https://code.visualstudio.com/))
- **Serial Terminal**: For debugging (PuTTY, screen, or PlatformIO monitor)

### Check Your Installation

```bash
# Verify tools
git --version           # Should be 2.x+
node --version          # Should be v18+
npm --version           # Should be 9+
python3 --version       # Should be 3.8+
pio --version           # Should be 6.x+ (after installing PlatformIO)
```

## 🚀 Quick Setup (5 Minutes)

### 1. Clone Repository

```bash
# Clone the repo
git clone https://github.com/yourusername/ots.git
cd ots

# Repository structure:
# ots/
#   ├── ots-server/        # Nuxt dashboard
#   ├── ots-userscript/    # Browser extension
#   ├── ots-fw-main/       # Main controller firmware
#   ├── ots-fw-audiomodule # Audio module firmware
#   ├── ots-fw-cantest/    # CAN bus testing tool
#   ├── ots-shared/        # Shared TypeScript types
#   └── doc/               # Documentation
```

### 2. Install Dependencies

**Server (Nuxt Dashboard)**:
```bash
cd ots-server
npm install
```

**Userscript**:
```bash
cd ../ots-userscript
npm install
```

**Shared Types**:
```bash
cd ../ots-shared
npm install
npm run build  # Build shared types
```

### 3. First Build Test

**Test Server**:
```bash
cd ots-server
npm run dev
# Open http://localhost:3000
# Dashboard should load with hardware emulator
```

**Test Userscript**:
```bash
cd ../ots-userscript
npm run build
# Output: build/userscript.ots.user.js
# Install in browser to test
```

**Test Firmware** (requires PlatformIO):
```bash
cd ../ots-fw-main
pio run -e esp32-s3-dev
# Should compile successfully (even without hardware)
```

## 🔧 PlatformIO Setup

### Installation Options

**Option A: VS Code Extension (Recommended)**

1. Install [VS Code](https://code.visualstudio.com/)
2. Install PlatformIO IDE extension:
   - Open Extensions (Ctrl+Shift+X)
   - Search "PlatformIO IDE"
   - Click Install
3. Restart VS Code
4. PlatformIO home opens automatically

**Option B: Command Line**

```bash
# Install PlatformIO Core
pip install -U platformio

# Verify installation
pio --version
```

### First Firmware Build

```bash
cd ots-fw-main

# Build main firmware
pio run -e esp32-s3-dev

# This will:
# 1. Download ESP-IDF framework (first time only, ~1GB)
# 2. Install ESP32-S3 toolchain
# 3. Compile firmware
# 4. Output: .pio/build/esp32-s3-dev/firmware.bin
```

**First build takes 10-15 minutes** (downloads toolchain). Subsequent builds are much faster.

### PlatformIO Environments

The firmware has multiple build environments:

```bash
# Main firmware
pio run -e esp32-s3-dev              # Build only
pio run -e esp32-s3-dev -t upload    # Build + flash to device
pio run -e esp32-s3-dev -t monitor   # Build + flash + serial monitor

# Hardware tests (standalone test firmwares)
pio run -e test-i2c -t upload        # Test I2C bus
pio run -e test-outputs -t upload    # Test LED outputs
pio run -e test-inputs -t upload     # Test button inputs
pio run -e test-lcd -t upload        # Test LCD display
pio run -e test-websocket -t upload  # Test WebSocket only
```

See [Firmware Development](firmware-development.md) for details.

## 📂 Repository Overview

### Project Structure

```
ots/
├── doc/                    # Documentation (you are here!)
│
├── ots-server/            # 🌐 Nuxt Dashboard + WebSocket Server
│   ├── app/               # Vue 3 components
│   ├── server/routes/     # API endpoints + WebSocket handlers
│   └── package.json       # Node.js dependencies
│
├── ots-userscript/        # 🔧 Browser Extension
│   ├── src/               # TypeScript source
│   ├── build.mjs          # esbuild bundler
│   └── build/             # Compiled output (.user.js)
│
├── ots-fw-main/           # 🎛️ Main Controller Firmware (ESP32-S3)
│   ├── src/               # C source files
│   ├── include/           # Header files
│   ├── components/        # Hardware driver components
│   └── platformio.ini     # Build configuration
│
├── ots-fw-audiomodule/    # 🔊 Audio Module Firmware (ESP32-A1S)
│   ├── src/               # Audio playback + CAN bus
│   └── platformio.ini
│
├── ots-fw-cantest/        # 🧪 CAN Bus Testing Tool
│   ├── src/               # Protocol decoder + simulators
│   └── README.md          # Interactive CLI guide
│
├── ots-fw-shared/         # 📦 Shared Firmware Components
│   └── components/        # CAN drivers, discovery protocol
│
├── ots-shared/            # 🔗 Shared TypeScript Types
│   └── src/game.ts        # Protocol types (TS)
│
├── ots-hardware/          # 🔩 Hardware Specs
│   ├── modules/           # Module specifications
│   └── pcbs/              # Circuit board designs
│
└── prompts/               # 📝 AI Assistant Context
    └── protocol-context.md  # WebSocket protocol spec
```

See [Repository Overview](repository-overview.md) for detailed breakdown.

## 🛠️ Development Workflows

### Common Tasks

**Run development server** (dashboard):
```bash
cd ots-server
npm run dev  # Hot-reload on http://localhost:3000
```

**Build userscript with watch mode**:
```bash
cd ots-userscript
npm run watch  # Auto-rebuilds on file changes
```

**Flash firmware to device**:
```bash
cd ots-fw-main
pio run -e esp32-s3-dev -t upload
pio device monitor  # View serial output
```

**Run CAN bus tests**:
```bash
cd ots-fw-cantest
pio run -e esp32-s3-devkit -t upload
pio device monitor
# Press 'm' for monitor mode
```

### Development Loop

1. **Make changes** in your editor
2. **Test locally**:
   - Server: Browser auto-reloads
   - Userscript: Rebuild + reload browser page
   - Firmware: Flash to device + monitor serial
3. **Commit changes** following [git workflow](../standards/git-workflow.md)
4. **Push to repository**

## 🔌 Hardware Setup (Optional)

### Required Hardware

For firmware development with physical device:

- **ESP32-S3 DevKit** board
- **USB-C cable** (data + power)
- **MCP23017** I2C I/O expanders (2x)
- **LEDs** + resistors (for testing)
- **Buttons** (for testing)
- **Breadboard** + jumper wires

See [Hardware Guide](hardware/) for complete specs.

### Connections

**I2C Bus** (shared by all modules):
- **SDA**: GPIO8
- **SCL**: GPIO9
- **GND**: Common ground
- **VCC**: 3.3V or 5V (depending on module)

See [I2C Architecture](hardware/i2c-architecture.md) for wiring diagrams.

### Testing Without Hardware

You can develop most features without physical hardware:

- **Dashboard emulator** - Simulates all hardware modules
- **WebSocket testing** - Test protocol without device
- **Firmware compilation** - Build without flashing
- **Unit tests** - Test logic independently

## 🧪 Testing Your Setup

### Verify Server Works

```bash
cd ots-server
npm run dev
```

Open browser to `http://localhost:3000`:
- ✅ Dashboard loads
- ✅ Hardware modules visible
- ✅ No console errors

### Verify Userscript Builds

```bash
cd ots-userscript
npm run build
ls -lh build/userscript.ots.user.js
```

Output should be ~100KB JavaScript file.

### Verify Firmware Compiles

```bash
cd ots-fw-main
pio run -e esp32-s3-dev
```

Should complete with:
```
RAM:   [=         ]   X.X% (used XXXXX bytes)
Flash: [====      ]  XX.X% (used XXXXXX bytes)
SUCCESS
```

### Run Integration Test

1. **Start server** (`npm run dev` in ots-server)
2. **Open OpenFront.io** in browser
3. **Install userscript** (from build/)
4. **Configure WebSocket**: Point to `ws://localhost:3000/ws`
5. **Join game**: Start or join an OpenFront.io game
6. **Watch events**: Dashboard should show game events in real-time

## 📖 Next Steps

### Essential Reading

1. **[Repository Overview](repository-overview.md)** - Understand codebase structure
2. **[Architecture](architecture/)** - System design and protocols
3. **[Coding Standards](standards/coding-standards.md)** - Code style and conventions

### Development Guides

- **[Firmware Development](firmware-development.md)** - Build ESP32 firmware
- **[Server Development](server-development.md)** - Nuxt dashboard development
- **[Userscript Development](userscript-development.md)** - Browser extension development

### Common Workflows

- **[Adding Hardware Module](workflows/add-hardware-module.md)** - Create new module
- **[Protocol Changes](workflows/protocol-changes.md)** - Modify WebSocket messages
- **[Testing](testing/)** - Test strategies and tools

## 🆘 Troubleshooting

### PlatformIO Won't Install

**Issue**: `pip install platformio` fails

**Solutions**:
- Use Python 3.8+ (`python3 --version`)
- Try `pip3 install -U platformio`
- Install in virtual environment:
  ```bash
  python3 -m venv venv
  source venv/bin/activate
  pip install platformio
  ```

### Firmware Won't Compile

**Issue**: ESP-IDF errors during build

**Solutions**:
- Clear build cache: `pio run -e esp32-s3-dev -t clean`
- Delete `.pio` folder and rebuild
- Update PlatformIO: `pio upgrade`
- Check ESP-IDF version in `platformio.ini`

### Node Modules Issues

**Issue**: `npm install` fails or modules missing

**Solutions**:
- Delete `node_modules` and `package-lock.json`
- Run `npm install` again
- Clear npm cache: `npm cache clean --force`
- Use Node.js LTS version (v18 or v20)

### Port Access Denied

**Issue**: Can't access serial port for flashing

**Solutions**:
- **Linux**: Add user to dialout group:
  ```bash
  sudo usermod -a -G dialout $USER
  # Logout and login again
  ```
- **macOS**: Install CH340 USB drivers
- **Windows**: Install CP210x drivers

## 🤝 Getting Help

- **[Developer FAQ](FAQ.md)** - Common questions
- **[Troubleshooting](troubleshooting/)** - Debug guides
- **GitHub Issues** - Report bugs
- **Discord/Forum** - Community support

---

**Last Updated**: January 2026  
**Minimum Versions**: Node.js 18+, Python 3.8+, PlatformIO 6+
