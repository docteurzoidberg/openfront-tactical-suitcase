# Components Directory - DEPRECATED

**⚠️ This directory is deprecated. Shared components have been moved.**

## New Location

All shared firmware components have been moved to:

```
/ots-fw-shared/components/
```

## Components That Were Moved

- **can_driver** → `/ots-fw-shared/components/can_driver/`
  - Generic CAN bus (TWAI) driver
  - Used by fw-main and fw-audiomodule
  - Documentation: See `COMPONENT_PROMPT.md` in new location

## Why the Move?

The old `/components/` directory at repository root was ambiguous. The new structure is clearer:

- **`/ots-fw-shared/`** - Firmware components shared across multiple firmware projects
- **`/ots-fw-main/components/`** - Components specific to main controller firmware
- **`/ots-fw-audiomodule/components/`** - Components specific to audio module firmware

## For Developers

If you have code referencing the old location:

### ESP-IDF Projects (fw-main, fw-audiomodule)

Update your **main CMakeLists.txt**:

```cmake
# Add shared components directory
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ots-fw-shared/components")
```

Then your source CMakeLists.txt can reference by name:

```cmake
idf_component_register(
    SRCS "main.c"
    REQUIRES 
        can_driver  # Automatically found in ots-fw-shared/components/
)
```

### Documentation References

Update paths:
- OLD: `/components/can_driver/`
- NEW: `/ots-fw-shared/components/can_driver/`

## Cleanup

**This directory can be deleted** once all references are updated and builds are verified.

**Steps to remove:**
1. Verify fw-main builds: `cd ots-fw-main && pio run -e esp32-s3-dev`
2. Verify fw-audiomodule builds (if applicable)
3. Delete this directory: `rm -rf /components/`

## References

- Shared components README: `/ots-fw-shared/README.md`
- Component architecture: `/ots-fw-main/docs/COMPONENTS_ARCHITECTURE.md`
