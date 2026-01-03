# OTS Firmware Shared Components

This directory contains **shared ESP-IDF components** used across multiple OTS firmware projects:

- **ots-fw-main**: Main controller firmware (ESP32-S3)
- **ots-fw-audiomodule**: Audio module firmware (ESP32-A1S)
- **Future firmware projects**: Display modules, lighting modules, etc.

## Purpose

Shared components in this directory are **hardware-agnostic** or **protocol-level** abstractions that can be used by any firmware project in the OTS ecosystem.

## Component Types

### Protocol Components
- **can_driver**: Generic CAN bus (TWAI) driver with auto-detection and mock fallback

### Future Components
- Communication protocols (WebSocket client, MQTT, etc.)
- Common utilities (logging, config management)
- Shared data structures (message formats, etc.)

## Architecture

```
ots-fw-shared/
├── README.md              # This file
└── components/            # ESP-IDF components
    └── can_driver/        # Generic CAN bus driver
        ├── CMakeLists.txt
        ├── idf_component.yml
        ├── README.md
        ├── COMPONENT_PROMPT.md
        ├── CAN_PROTOCOL_ARCHITECTURE.md
        ├── include/
        │   └── can_driver.h
        └── can_driver.c
```

## Using Shared Components

### In fw-main (or any firmware project)

Add to your project's `CMakeLists.txt`:

```cmake
# Add shared components directory
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ots-fw-shared/components")
```

Then in your source component:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "../include"
    REQUIRES 
        can_driver  # From ots-fw-shared/components/
        # ...
)
```

### Adding New Shared Components

1. **Create component directory**:
   ```bash
   cd ots-fw-shared/components
   mkdir my_shared_component
   cd my_shared_component
   ```

2. **Create required files**:
   - `CMakeLists.txt` - ESP-IDF component build config
   - `idf_component.yml` - Dependencies
   - `README.md` - User documentation
   - `COMPONENT_PROMPT.md` - AI assistant guidance
   - `include/my_shared_component.h` - Public API
   - `my_shared_component.c` - Implementation

3. **Follow design principles**:
   - No firmware-specific logic
   - Generic, reusable abstractions
   - Well-documented public API
   - Minimal dependencies

## Design Principles

### ✅ Good Candidates for Shared Components

- **Generic hardware drivers**: CAN bus, SPI flash, external peripherals
- **Communication protocols**: WebSocket, MQTT, custom protocols
- **Utilities**: Logging wrappers, config parsers, data structures
- **Abstractions**: Generic interfaces that multiple firmwares need

### ❌ Not Suitable for Shared Components

- **Application logic**: Game state management, business rules
- **Firmware-specific code**: Module initialization, hardware pin mappings
- **Project dependencies**: Code that only one firmware needs

## Component Guidelines

1. **Independence**: Components should not depend on each other unless necessary
2. **Documentation**: All components must have COMPONENT_PROMPT.md
3. **Testing**: Provide usage examples and test code
4. **Versioning**: Use semantic versioning in `idf_component.yml`
5. **API Stability**: Avoid breaking changes; use deprecation warnings

## References

- Main firmware: `/ots-fw-main/`
- Audio firmware: `/ots-fw-audiomodule/`
- Component architecture: `/ots-fw-main/docs/COMPONENTS_ARCHITECTURE.md`
- CAN protocol: `/ots-fw-shared/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md`
