# OTS Hardware - AI Assistant Context

This directory contains hardware specifications and design documents for the **Openfront Tactical Suitcase** physical controller device.

## Purpose

This directory serves as the **source of truth** for hardware specifications. Module specifications here are used to:
1. Guide firmware implementation in `ots-fw-main/`
2. Generate TypeScript types for `ots-shared/`
3. Create emulator logic for `ots-server/`
4. Build corresponding Vue UI components

## File Organization

- `README.md` - High-level hardware overview and architecture
- `hardware-spec.md` - Main controller and bus specification (I2C, CAN, power)
- `module-template.md` - Template for creating new module specifications
- `modules/` - Individual module specifications (one file per module)
- `copilot-project-context.md` - This file (AI assistant guidance)

## Key Concepts

### Module Specification Philosophy
Module specs are **intentionally concise** and focused on:
- **What** the module does (game state domain)
- **Physical components** (buttons, LEDs, displays, etc.)
- **Communication needs** (I2C, CAN, or custom protocols)
- **Game state mapping** (TypeScript types for inbound/outbound data)
- **Behavior** (high-level flow, not detailed implementation)
- **UI mockup** (Vue component interface)

Avoid over-specifying implementation details. The spec should guide development, not dictate every line of code.

### Modular Architecture
The device uses a modular design where functionality is split across separate daughter boards:
- Each module is a self-contained unit with specific game state domain
- Modules communicate with main controller via I2C/CAN bus
- Physical modules match software components in UI dashboard

## Workflow for Creating Modules

1. **Spec First**: Copy `module-template.md` to `modules/<module-name>.md`
2. **Define Module**: Fill out overview, components, communication, game state mapping
3. **Implement Firmware**: Create C code in `ots-fw-main/src/` (`.c` and `.h` files)
4. **Add to Build**: Update `ots-fw-main/src/CMakeLists.txt` SRCS list
5. **Implement UI**: Create Vue component in `ots-server/app/components/hardware/`
6. **Add Types**: Update `ots-shared/src/` with command/event types
7. **Test with Emulator**: Use ots-server emulator mode to test without hardware
8. **Build Hardware**: Assemble physical module and integrate

## Module Naming Conventions

- **Spec file**: `modules/module-name.md` (kebab-case)
- **Firmware class**: Not applicable (C, not C++)
- **Firmware files**: `module_name.h` / `module_name.c` (snake_case)
- **Vue component**: `ModuleName.vue` (PascalCase)
- **TypeScript types**: `ModuleNameCommand`, `ModuleNameEvent` (PascalCase)

---

**Important**: When evolving hardware specs, update this directory first, then update firmware and UI code to match.

## Current Modules

- **Spec file**: `modules/module-name.md` (kebab-case)
- **Firmware class**: `ModuleNameModule` (PascalCase)
- **Firmware files**: `module_name.h` / `module_name.cpp` (snake_case)
- **Vue component**: `ModuleName.vue` (PascalCase)
- **TypeScript types**: `ModuleNameCommand`, `ModuleNameEvent` (PascalCase)

---

**Important**: When evolving hardware specs, update this directory first, then update firmware and UI code to match.
