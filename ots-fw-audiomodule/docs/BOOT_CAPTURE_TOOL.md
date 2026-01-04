# Boot Log Capture Tool

## Overview

`capture_boot.py` is a Python tool for capturing complete boot sequences from the ESP32-A1S Audio Module firmware. It monitors the serial port while resetting the device to capture all logs from the very beginning of the boot process.

## Purpose

When debugging firmware issues or verifying boot behavior, you need to see logs from the earliest boot stages (bootloader, app start, hardware initialization). Simply connecting to serial after the device has already booted misses critical early messages.

This tool solves that by:
1. Opening the serial port first
2. Resetting the device via esptool.py
3. Capturing all output from the moment the device starts
4. Optionally saving logs to a file

## Requirements

- Python 3.7 or later (uses stdlib only)
- esptool.py (for device reset)
  ```bash
  pip install esptool
  ```
- Serial port access (user must be in `dialout` group on Linux)

## Installation

The tool is already in the repository:
```
ots-fw-audiomodule/
  tools/
    capture_boot.py  # Executable Python script
```

No additional installation needed - just run it directly.

## Usage

### Basic Usage (Auto-detect)

```bash
# From repository root
cd ots-fw-audiomodule
./tools/capture_boot.py
```

This will:
- Auto-detect the ESP32 serial port (usually /dev/ttyUSB0 or /dev/ttyACM0)
- Reset the device
- Capture 10 seconds of boot logs
- Display logs in terminal

### Specify Port

```bash
./tools/capture_boot.py --port /dev/ttyUSB0
```

### Save to File

```bash
./tools/capture_boot.py --output boot_logs.txt
```

This displays logs in terminal AND saves to `boot_logs.txt`.

### Longer Capture Duration

```bash
./tools/capture_boot.py --duration 15
```

Useful if you need to capture beyond the initial boot (e.g., CAN initialization, first status messages).

### Monitor Without Reset

```bash
./tools/capture_boot.py --no-reset --duration 30
```

Just monitor the serial port without resetting. Useful for capturing runtime logs.

### Combine Options

```bash
./tools/capture_boot.py --port /dev/ttyUSB0 --duration 20 --output full_boot.txt
```

## Command-Line Options

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--port` | `-p` | Auto-detect | Serial port path |
| `--baud` | `-b` | 115200 | Baud rate |
| `--duration` | `-d` | 10.0 | Capture duration (seconds) |
| `--output` | `-o` | None | Save logs to file |
| `--no-reset` | | False | Skip device reset |

## Examples

### Example 1: Quick Boot Check

```bash
./tools/capture_boot.py
```

Output:
```
Using serial port: /dev/ttyUSB0
Baud rate: 115200
Capture duration: 10.0s
Opening serial port...
Resetting device...
Device reset successful

============================================================
Capturing serial output for 10.0s...
============================================================

ets Jul 29 2019 12:21:46

rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
...
I (359) MAIN: === ESP32-A1S Audio Module Starting ===
...
I (499) MIXER: Initializing audio mixer (max 4 sources)
I (509) MIXER: Mixer task started
W (509) MIXER: Audio hardware not ready - I2S output disabled
...

============================================================
Capture complete
============================================================
```

### Example 2: Debug Boot Issue

When investigating a boot crash or hang:

```bash
./tools/capture_boot.py --duration 30 --output debug_boot.txt
```

Then analyze `debug_boot.txt` to find the crash point or last successful initialization.

### Example 3: Compare Before/After Changes

```bash
# Before firmware change
./tools/capture_boot.py --output boot_before.txt

# Flash new firmware
pio run -e esp32-a1s-espidf -t upload

# After firmware change
./tools/capture_boot.py --output boot_after.txt

# Compare
diff boot_before.txt boot_after.txt
```

### Example 4: Continuous Monitoring

For debugging intermittent issues:

```bash
# Monitor without reset, capture 5 minutes
./tools/capture_boot.py --no-reset --duration 300 --output runtime_logs.txt
```

## Integration with Development Workflow

### 1. Verify Boot After Flash

```bash
# Build and flash
pio run -e esp32-a1s-espidf -t upload

# Immediately capture boot logs
./tools/capture_boot.py
```

### 2. Automated Testing

Use in test scripts to verify firmware boots correctly:

```python
import subprocess

# Flash firmware
subprocess.run(["pio", "run", "-e", "esp32-a1s-espidf", "-t", "upload"])

# Capture boot
result = subprocess.run(
    ["./tools/capture_boot.py", "--output", "test_boot.txt"],
    timeout=15
)

# Check for critical errors
with open("test_boot.txt") as f:
    logs = f.read()
    assert "Guru Meditation Error" not in logs
    assert "Audio mixer initialized" in logs
```

### 3. CI/CD Integration

Add to GitHub Actions or similar:

```yaml
- name: Flash and verify boot
  run: |
    cd ots-fw-audiomodule
    pio run -e esp32-a1s-espidf -t upload
    ./tools/capture_boot.py --output boot_logs.txt
    
- name: Upload boot logs
  uses: actions/upload-artifact@v2
  with:
    name: boot-logs
    path: ots-fw-audiomodule/boot_logs.txt
```

## Troubleshooting

### Port Permission Denied

```
ERROR: Permission denied: /dev/ttyUSB0
```

Solution:
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Log out and back in, or:
newgrp dialout
```

### esptool Not Found

```
WARNING: esptool.py not found, reset may not work
```

Solution:
```bash
pip install esptool
```

### No Serial Ports Found

```
ERROR: No serial ports found
```

Check:
1. Device is plugged in via USB
2. USB cable supports data (not charge-only)
3. Device drivers installed (should be automatic on Linux)

Check available ports:
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
```

### Port Already in Use

```
ERROR: Serial port in use: /dev/ttyUSB0
```

Close other terminal programs:
```bash
# Kill all pio monitor processes
pkill -f "pio.*monitor"

# Or find and kill specific process
lsof /dev/ttyUSB0
kill <PID>
```

## Implementation Notes

### Why This Approach?

We use Python's stdlib-only approach (no pyserial dependency) for:
- **Portability**: Works on any Linux system with Python 3
- **Simplicity**: No pip dependencies beyond esptool
- **Reliability**: Direct termios control for stable serial I/O
- **Consistency**: Matches the pattern used in ots-fw-main tools

### How Reset Works

The script uses `esptool.py --chip esp32 --port <PORT> run` which:
1. Connects to the ESP32 bootloader
2. Sends a reset command via DTR/RTS lines
3. Immediately exits, leaving the device running

This is gentler than:
- Hard power cycle (requires external hardware)
- Manual reset button (requires physical access)
- Other reset methods that may interfere with serial monitoring

### Serial Port Configuration

The script configures the port in **raw mode**:
- No echo, no line buffering
- 8 data bits, no parity, 1 stop bit (8N1)
- Hardware flow control disabled
- Captures exact output from firmware

This ensures we see logs exactly as the firmware sends them, with no terminal processing.

## Related Tools

- **pio device monitor**: Interactive serial monitor (no boot capture)
- **ots_device_tool.py** (ots-fw-main): Full device testing framework with WebSocket support
- **esptool.py**: Low-level ESP32 flashing and control

## See Also

- [PROJECT_PROMPT.md](../PROJECT_PROMPT.md) - Firmware architecture overview
- [README.md](../README.md) - Build and flash instructions
- [ots-fw-main tools](../../ots-fw-main/tools/) - Main firmware testing tools
