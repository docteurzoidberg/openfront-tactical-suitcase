# Component Reorganization - Summary

**Date**: January 3, 2026

## Changes Made

Reorganized shared firmware components from root `/components/` to `/ots-fw-shared/components/` for better project structure.

## New Directory Structure

### Before
```
ots/
├── components/
│   └── can_driver/          # Lonely, ambiguous location
└── ots-fw-main/
    └── components/
        ├── mcp23017_driver/
        ├── ads1015_driver/
        └── ...
```

### After
```
ots/
├── ots-fw-shared/                    # NEW: Shared firmware code
│   ├── README.md                     # Documentation
│   └── components/
│       └── can_driver/               # Moved from /components/
│           ├── CMakeLists.txt
│           ├── idf_component.yml
│           ├── README.md
│           ├── COMPONENT_PROMPT.md
│           ├── CAN_PROTOCOL_ARCHITECTURE.md
│           ├── include/can_driver.h
│           └── can_driver.c
│
├── components/                       # DEPRECATED (to be deleted)
│   ├── README.md                     # Deprecation notice
│   └── can_driver/                   # OLD location (can delete)
│
└── ots-fw-main/
    ├── CMakeLists.txt                # UPDATED: Added EXTRA_COMPONENT_DIRS
    └── components/
        ├── mcp23017_driver/          # Project-specific components
        ├── ads1015_driver/
        └── ...
```

## Files Created/Modified

### Created
1. `/ots-fw-shared/README.md` - Documentation for shared components
2. `/ots-fw-shared/components/can_driver/` - Copied from `/components/can_driver/`
3. `/components/README.md` - Deprecation notice

### Modified
1. `/ots-fw-main/CMakeLists.txt` - Added `EXTRA_COMPONENT_DIRS` to find shared components
2. `/ots-fw-main/docs/COMPONENTS_ARCHITECTURE.md` - Updated all references
3. `/ots-fw-main/copilot-project-context.md` - Updated component location
4. `/ots-fw-main/docs/CAN_MULTI_MODULE_ROADMAP.md` - Updated references
5. `/.github/copilot-instructions.md` - Updated repository structure

## Component Location Guidelines

### `/ots-fw-shared/components/`
**Use for**: Components shared across **multiple** firmware projects
- CAN driver (used by fw-main and fw-audiomodule)
- Future: WebSocket client, MQTT client, common protocols
- Future: Shared utilities, data structures

**Characteristics**:
- No project-specific dependencies
- Generic, reusable abstractions
- Can be used by fw-main, fw-audiomodule, and future firmwares

### `/ots-fw-main/components/`
**Use for**: Components specific to **main controller** firmware only
- MCP23017 driver (I/O expander for buttons/LEDs)
- ADS1015 driver (ADC for slider)
- HD44780 LCD driver
- WS2812 RGB LED driver
- ESP HTTP server utilities

**Characteristics**:
- Used only by fw-main
- May have fw-main-specific configurations
- Not intended for other firmware projects

## Build System Integration

### Main Firmware (ots-fw-main)

**CMakeLists.txt** now includes:
```cmake
# Add shared components directory
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ots-fw-shared/components")
```

This tells ESP-IDF to search for components in the shared directory.

**Source CMakeLists.txt** references by name:
```cmake
idf_component_register(
    SRCS "main.c" "sound_module.c"
    REQUIRES 
        can_driver  # Automatically found in ots-fw-shared/components/
        mcp23017_driver  # Found in ots-fw-main/components/
)
```

### Audio Firmware (ots-fw-audiomodule)

**TODO**: Update when needed:
```cmake
# In ots-fw-audiomodule/CMakeLists.txt
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ots-fw-shared/components")
```

## Benefits

1. **Clearer Organization**: No confusion about what's shared vs project-specific
2. **Explicit Sharing**: `/ots-fw-shared/` makes it obvious these are for multiple projects
3. **Scalability**: Easy to add more shared components as needed
4. **No Root Clutter**: Root `/components/` was lonely and ambiguous
5. **Better Documentation**: Each area has its own README explaining its purpose

## Future Components

### Candidates for ots-fw-shared/components/
- WebSocket client library (if both firmwares need it)
- MQTT client (for IoT features)
- Configuration management utilities
- Logging wrappers
- Shared message formats
- Network utilities

### Stay in fw-main/components/
- Hardware drivers for specific fw-main peripherals
- Firmware-specific utilities
- OTA update logic (unless audio module needs it too)

## Migration Checklist

- [x] Create `/ots-fw-shared/` directory structure
- [x] Copy `can_driver` to new location
- [x] Update fw-main CMakeLists.txt with EXTRA_COMPONENT_DIRS
- [x] Update all documentation references
- [x] Add deprecation notice to old location
- [x] Verify build system finds components correctly
- [ ] Delete old `/components/can_driver/` after verification (manual step)
- [ ] Update fw-audiomodule when it uses can_driver (future)

## Cleanup

Once verified that everything works:

```bash
cd /home/drzoid/dev/openfront/ots
rm -rf components/
```

**Verification before cleanup**:
```bash
cd ots-fw-main
pio run -e esp32-s3-dev  # Should build successfully
```

## Documentation Updates

All documentation now references the new location:
- Component architecture docs
- CAN protocol roadmap
- Project context files
- GitHub Copilot instructions

Search and replace pattern used:
- OLD: `/components/can_driver/`
- NEW: `/ots-fw-shared/components/can_driver/`

## References

- Shared components: `/ots-fw-shared/README.md`
- Component architecture: `/ots-fw-main/docs/COMPONENTS_ARCHITECTURE.md`
- CAN driver: `/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md`
- CAN protocol: `/ots-fw-shared/components/can_driver/CAN_PROTOCOL_ARCHITECTURE.md`
