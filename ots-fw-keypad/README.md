# OTS Keypad Module Firmware

Firmware for the 15-key RGB mechanical keyboard module with dual-mode operation (CAN bus + USB HID).

## Hardware

- **MCU**: M5Stack Stamp S3 (ESP32-S3)
- **Keys**: 15× Cherry MX compatible (3×7 matrix)
- **RGB LEDs**: 15× SK6812-MINI-E
- **CAN**: TJA1050 transceiver
- **USB**: USB-C connector

## Quick Start

### Build with PlatformIO

```bash
# Build
pio run -e m5stack-stamps3-espidf

# Flash
pio run -e m5stack-stamps3-espidf -t upload

# Monitor
pio device monitor
```

### Build with ESP-IDF

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Build, flash, and monitor
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Status

**Current**: Skeleton project with basic initialization

See [PROJECT_PROMPT.md](PROJECT_PROMPT.md) for planned features and implementation roadmap.

## Documentation

- **Feature List**: [PROJECT_PROMPT.md](PROJECT_PROMPT.md)
- **Hardware Module**: [/ots-hardware/modules/keypad-module.md](../ots-hardware/modules/keypad-module.md)
- **PCB Design**: [/ots-hardware/pcbs/keypad.md](../ots-hardware/pcbs/keypad.md)
- **CAN Protocol**: [/prompts/CANBUS_MESSAGE_SPEC.md](../prompts/CANBUS_MESSAGE_SPEC.md)

## Directory Structure

```
ots-fw-keypad/
  ├── CMakeLists.txt              # ESP-IDF project
  ├── platformio.ini              # PlatformIO config
  ├── sdkconfig.defaults          # ESP-IDF defaults
  ├── partitions_no_ota.csv       # Partition table
  ├── PROJECT_PROMPT.md           # Feature roadmap
  ├── README.md                   # This file
  └── src/
      ├── CMakeLists.txt          # Component definition
      └── main.c                  # Entry point (skeleton)
```

## License

Part of the OpenFront Tactical Suitcase (OTS) project.
