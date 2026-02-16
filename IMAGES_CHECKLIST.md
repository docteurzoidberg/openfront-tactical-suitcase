# OTS Image Checklist

Track progress on adding images to documentation. See individual `images/README.md` files for detailed requirements and export instructions.

## Status Legend
- ✅ Added
- 📸 Have file, need to add
- 🎨 Need to create
- ⏳ Planned for later

---

## Root Project Images (`/images/`)

| Image | Status | File | Notes |
|-------|--------|------|-------|
| Suitcase assembly render | ✅ | `suitcase-assembly-render.png` | Added to repo |
| Device photo | 📸 | `device-photo.jpg` | WIP photos available |
| Architecture diagram | 🎨 | `architecture-diagram.png` | Create later |
| Simulator dashboard | 📸 | `simulator-dashboard.png` | Can screenshot |
| Userscript HUD | 📸 | `userscript-hud.png` | Can screenshot |

## Hardware Images (`/ots-hardware/kicad/images/`)

### Assembly & Layout
| Image | Status | File | Notes |
|-------|--------|------|-------|
| 10U rack layout diagram | 🎨 | `rack-layout.png` | Create diagram |
| Controller render | 📸 | `controller-render.png` | Export from KiCAD |
| Controller PCB | 📸 | `controller-pcb.png` | Export from KiCAD |

### Module CAD Renders (`/ots-hardware/cad/images/`) & PCB Images (`/ots-hardware/kicad/images/`)
| Module | CAD Render | PCB Image | Status |
|--------|------------|-----------|--------|
| Main Power (2U) | `main-power-render.png` | `main-power-pcb.png` | ✅ CAD done |
| Sound (2U) | `sound-render.png` | `sound-pcb.png` | 📸 Ready to export |
| Troops (4U) | `troops-render.png` | `troops-pcb.png` | ✅ CAD done |
| Alert (4U) | `alert-render.png` | `alert-pcb.png` | 📸 Ready to export |
| Nuke (4U) | `nuke-render.png` | `nuke-pcb.png` | 📸 Ready to export |
| Keypad (4U) | `keypad-render.png` | `keypad-pcb.png` | ✅ CAD done |

### Individual Module Specs
All module specs in `/ots-hardware/modules/*.md` now have image placeholders.

## Simulator Images (`/ots-simulator/images/`)

| Image | Status | File | Notes |
|-------|--------|------|-------|
| Dashboard overview | 📸 | `dashboard-overview.png` | Screenshot simulator |
| Main Power module UI | 📸 | `main-power-module.png` | Screenshot component |
| Alert module UI | 📸 | `alert-module.png` | Screenshot component |
| Nuke module UI | 📸 | `nuke-module.png` | Screenshot component |
| Troops module UI | 📸 | `troops-module.png` | Screenshot component |

## Userscript Images (`/ots-userscript/images/`)

| Image | Status | File | Notes |
|-------|--------|------|-------|
| Userscript HUD | 📸 | `userscript-hud.png` | Screenshot in-game |
| Logs tab | ⏳ | `logs-tab.png` | Optional detail |
| Hardware tab | ⏳ | `hardware-tab.png` | Optional detail |
| Sound tab | ⏳ | `sound-tab.png` | Optional detail |

---

## Quick Start Guide

### Priority Order (recommended)
1. **Root README hero images**: Suitcase render + device photo
2. **Hardware module table**: Export all 6 module renders + PCBs
3. **Software screenshots**: Simulator dashboard + userscript HUD
4. **Individual module detail**: Add to specific module docs
5. **Diagrams**: Architecture and flow diagrams (create when needed)

### Export Workflow

1. **KiCAD 3D Renders**:
   ```
   Open PCB → 3D Viewer → File → Export PNG (1920x1080, raytracing ON)
   Save to: ots-hardware/kicad/images/<module>-render.png
   ```

2. **PCB Layouts**:
   ```
   Open PCB → File → Plot → PDF → Convert to PNG
   Save to: ots-hardware/kicad/images/<module>-pcb.png
   ```

3. **Screenshots**:
   ```
   Simulator: npm run dev → localhost:3000 → Screenshot
   Userscript: Install → Join game → Open HUD → Screenshot
   ```

4. **Optimize Images**:
   ```bash
   # Compress PNGs
   pngquant --quality=80-95 *.png
   
   # Or use online tools
   tinypng.com
   ```

5. **Update Status**: Mark ✅ in this file as you add images

---

## Image Guidelines Summary

- **Format**: PNG (renders/UI), JPG (photos)
- **Size**: < 500KB per image (optimize!)
- **Resolution**: 1920px width minimum for hero images
- **Naming**: kebab-case, descriptive
- **Commit**: Include images in git (they're documentation)

## Next Steps

1. Export module renders from KiCAD (highest priority for GitHub README)
2. Take simulator screenshots
3. Screenshot userscript HUD in actual game
4. Add suitcase assembly render
5. Create architecture diagram when needed
