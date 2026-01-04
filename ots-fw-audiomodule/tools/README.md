# Audio Module Tools

This directory contains utility scripts for the ESP32-A1S Audio Module firmware.

## Available Tools

### capture_boot.py

Captures complete boot sequences from the ESP32-A1S device by monitoring serial output while resetting the device.

**Quick Start:**
```bash
# Basic usage (auto-detect port)
./tools/capture_boot.py

# Save to file
./tools/capture_boot.py --output boot_logs.txt

# Longer capture
./tools/capture_boot.py --duration 20
```

**Documentation:**
- [User Guide](../docs/BOOT_CAPTURE_TOOL.md) - Complete usage instructions
- [AI Prompt](../prompts/BOOT_CAPTURE_PROMPT.md) - AI assistant reference

**Purpose:**
Essential for debugging boot issues, verifying initialization sequences, and capturing early logs that would be missed by connecting to serial after the device has already started.

**Requirements:**
- Python 3.7+ (stdlib only, no external dependencies)
- esptool.py for device reset (auto-detected from PlatformIO installation)

## Tool Development Guidelines

When adding new tools to this directory:

1. **Follow stdlib-only pattern** - Avoid external dependencies when possible
2. **Make executable** - `chmod +x tools/your_tool.py`
3. **Add shebang** - Start with `#!/usr/bin/env python3`
4. **Document** - Create both user docs and AI prompts
5. **Match patterns** - Follow conventions from ots-fw-main/tools/

## Related Tools

- [ots-fw-main tools](../../ots-fw-main/tools/) - Main firmware testing tools
- `pio device monitor` - Interactive serial monitor
- `esptool.py` - ESP32 flashing and control
