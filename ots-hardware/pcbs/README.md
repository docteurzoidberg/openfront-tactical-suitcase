# OTS PCB Documentation

This directory contains electrical design documentation for all printed circuit boards (PCBs) in the OTS system.

## PCB Index

### Core System Boards

| PCB | Description | KiCAD Project | Status |
|-----|-------------|---------------|--------|
| [Controller](controller.md) | Main ESP32-S3 controller board | `kicad/controller_rev2.zip` | Rev 2 |
| [I/O Expander Board](io-expander-board.md) | MCP23017 GPIO expansion boards | `kicad/moduleboard_rev1.zip` | Rev 1 |

### Module Boards

| PCB | Description | KiCAD Project | Status |
|-----|-------------|---------------|--------|
| [Module Board Template](module-board-template.md) | Reusable generic module frame | `kicad/board_module_template.zip` | Template |
| [Keypad](keypad.md) | 15-key mechanical keyboard PCB | `kicad/keyboard_rev1.zip` | Rev 1 |

## Documentation Structure

Each PCB document includes:
- **Overview**: Purpose and role in system
- **Specifications**: Physical size, layer count, power requirements
- **Schematic**: Block diagram and key circuits
- **Component Selection**: IC choices and rationale
- **Interfaces**: Connectors, bus connections, I/O
- **Power Design**: Regulators, power tree, current budget
- **Layout Considerations**: Routing notes, impedance control
- **BOM**: Bill of Materials (key components)
- **Fabrication Notes**: Special manufacturing requirements
- **Testing**: Test points and bring-up procedures

## Related Documentation

- **Module Designs**: [/ots-hardware/modules/](../modules/) - How PCBs integrate into physical modules
- **Hardware Spec**: [/ots-hardware/hardware-spec.md](../hardware-spec.md) - System-level architecture
- **CAD Files**: [/ots-hardware/cad/](../cad/) - Mechanical enclosure designs
- **KiCAD Projects**: [/ots-hardware/kicad/](../kicad/) - Actual PCB design files

## Design Guidelines

### Naming Conventions
- PCB files: `<name>_rev<N>.zip` (e.g., `controller_rev2.zip`)
- Documentation: `<name>.md` (kebab-case, e.g., `io-expander-board.md`)

### Version Control
- KiCAD projects stored as ZIP archives
- Major revisions increment rev number
- Document significant changes in PCB markdown file

### Manufacturing
- Default: 2-layer FR4, 1.6mm thickness
- Special requirements documented per-board
- JLCPCB/PCBWay compatible designs preferred
