# Module CAD Renders

This directory contains mechanical CAD renders of OTS hardware modules. These are 3D renders of the physical module assemblies (enclosures, front panels, mechanical components).

**Note**: PCB-related images (PCB layouts, schematics) are in `/ots-hardware/kicad/images/`.

## Expected Images

### Module 3D Renders (Mechanical Assembly)
- `main-power-render.png` - Main Power Module (2U) mechanical render
- `sound-render.png` - Sound Module (2U) mechanical render  
- `troops-render.png` - Troops Module (4U) mechanical render with LCD/slider
- `alert-render.png` - Alert Module (4U) mechanical render with LED matrix
- `nuke-render.png` - Nuke Module (4U) mechanical render with buttons/LEDs
- `keypad-render.png` - Keypad Module (4U) mechanical render with 15-key keyboard

### Full Assembly
- `rack-layout.png` - 10U rack configuration diagram (optional)
- `full-assembly.png` - Complete suitcase assembly (optional)

## Export Guidelines

### From FreeCAD / Fusion 360 / SolidWorks
1. Open module assembly file
2. Set realistic view angle (slight perspective, front-facing)
3. Enable high-quality rendering:
   - Raytracing/realistic materials
   - Shadows and ambient occlusion
   - Anti-aliasing enabled
4. Export as PNG: 1920x1080 or higher resolution
5. Transparent or white background preferred

### Recommended View Settings
- **Angle**: 30-45° from front, slightly elevated
- **Lighting**: Overhead + front lighting for component visibility
- **Background**: White or transparent
- **Quality**: High DPI (300+ for print, 150+ for web)

### Naming Convention
- Use module name from hardware specs (kebab-case)
- Suffix with `-render` for mechanical 3D renders
- Example: `main-power-render.png`, `nuke-render.png`

## Usage

These renders appear in:
- `/ots-hardware/README.md` - Module comparison table (CAD column)
- `/ots-hardware/modules/*.md` - Individual module specification pages
- Root `/README.md` - Project overview (hero images)
