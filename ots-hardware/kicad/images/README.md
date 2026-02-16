# PCB Images (KiCAD)

This directory contains PCB layouts, board renders, and schematics exported from KiCAD.

**Note**: Mechanical CAD renders of module assemblies are in `/ots-hardware/cad/images/`.

## Expected Images

### Controller PCB
- `controller-render.png` - Main ESP32-S3 controller board 3D render (from KiCAD)
- `controller-pcb.png` - Controller PCB layout/schematic

### Module PCBs
- `main-power-pcb.png` - Main Power PCB layout
- `sound-pcb.png` - Sound Module PCB layout
- `troops-pcb.png` - Troops Module PCB layout
- `alert-pcb.png` - Alert Module PCB layout
- `nuke-pcb.png` - Nuke Control Panel PCB layout
- `keypad-pcb.png` - Keypad Module PCB layout

### Schematics (Optional)
- `*.schematic.png` - Individual module schematics exported from KiCAD

## Export Tips

### From KiCAD 3D Viewer
1. Open PCB in KiCAD
2. View → 3D Viewer
3. File → Export → PNG
4. Resolution: 1920x1080 or higher
5. Enable raytracing for best quality

### PCB Layouts
1. File → Plot
2. Format: PDF or SVG
3. Convert to PNG (300 DPI minimum)
4. Include silkscreen and component layers

### Naming Convention
- Use module name from specs (kebab-case)
- Suffix with `-render` for 3D, `-pcb` for layouts, `-schematic` for schematics
