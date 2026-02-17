# Documentation Plan - Keypad Module

This document outlines the documentation updates needed for the keypad module across user guides, developer guides, and the VitePress documentation site.

## Overview

Documentation is needed in three areas:
1. **User Documentation** - Installation, usage, userscript configuration
2. **Developer Documentation** - Architecture, API reference, integration
3. **Hardware Documentation** - PCB design, assembly, testing

**Important**: Key binding configuration is handled by the **userscript**, not firmware or dashboard.

## Files to Create/Update

### User Documentation (`/doc/user/`)

**New Files:**
- `keypad-module.md` - Complete user guide for keypad module
- `keypad-userscript-config.md` - How to configure key bindings in userscript
- `keypad-troubleshooting.md` - Common issues and solutions

**Updates:**
- `README.md` - Add keypad to module list
- `getting-started.md` - Mention keypad in setup
- `module-overview.md` - Add keypad section
- `userscript-guide.md` - Add keypad configuration section

### Developer Documentation (`/doc/developer/`)

**New Files:**
- `keypad-firmware-architecture.md` - Firmware design overview
- `keypad-can-protocol.md` - CAN message reference
- `keypad-integration-guide.md` - How to add keypad to custom builds

**Updates:**
- `module-system.md` - Add keypad as example
- `can-protocol.md` - Add keypad CAN messages (from plan #3)
- `websocket-protocol.md` - Add keypad WebSocket events (from plan #4)
- `userscript-architecture.md` - Add keypad mapping logic

### VitePress Site (`/ots-website/`)

**Updates:**
- `user/index.md` - Add keypad guide link
- `developer/index.md` - Add keypad architecture link
- `downloads.md` - Add keypad firmware download

---

## User Documentation Content

### 1. `/doc/user/keypad-module.md`

**Purpose**: Complete user guide for the keypad module

**Outline**:
```markdown
# Keypad Module User Guide

## Overview
- What is the keypad module?
- Features (15 keys, RGB LEDs, dual-mode)
- Use cases (game controls, standalone keyboard)

## Hardware
- Physical layout (7+7+1 key arrangement)
- Installation in suitcase (Column 1, Position 3)
- Connections (CAN bus, optional USB)
- LED indicators

## Operating Modes

### CAN Bus Mode (Integrated)
- Connected to main controller
- Key bindings configured in dashboard
- LED states controlled by game logic
- How to connect

### USB HID Mode (Standalone)
- Acts as USB keyboard
- User-programmable bindings
- How to switch modes

## Configuration

### Default Key Bindings
**Row 1 (Building Actions):**
- K1: Build City (1)
- K2: Build Factory (2)
- K3: Build Port (3)
- K4: Build Defense Post (4)
- K5: Build Missile Launcher (5)
- K6: Build SAM (6)
- K7: Build Warship (7)

**Row 2 (Game Controls):**
- K8: Zoom In (E)
- K9: Zoom Out (Q)
- K10: Decrease Attack Ratio (T)
- K11: Switch Missile Direction (U)
- K12: Increase Attack Ratio (Y)
- K13: Boat Attack (B)
- K14: Land Attack (G)

**Row 3 (View Control):**
- K15: Toggle View (Space)

**Note**: Nuke launches (8/9/0) are not mapped - use dedicated Nuke Module buttons instead.

### Key Binding (via Userscript)
- Configuration stored in userscript localStorage
- Access via Tampermonkey menu → "OTS Keypad Config" tab
- Real-time configuration without firmware reflash

### Customizing Bindings
1. Open OpenFront.io in browser with userscript installed
2. Click Tampermonkey icon → "OTS Keypad Config"
3. New tab opens with keypad configuration UI
4. Click key button to configure
5. Select game action from dropdown or enter custom DOM selector
6. Save changes to localStorage
7. Test by pressing physical key

### LED Indicators (Future Feature)
- Green: Action available
- Red: Action on cooldown
- Yellow: Charging/preparing
- Off: Action unavailable
- Blinking: Alert state

## Usage Tips
- Label your keycaps for easy identification
- Use consistent color schemes (green=go, red=stop)
- Configure most-used actions on top row
- Leave some keys unassigned for expansion

## Troubleshooting
- Key not responding → See troubleshooting guide
- LED not lighting → Check connections
- Configuration not saving → Factory reset
```

---

### 2. `/doc/user/keypad-userscript-config.md`

**Purpose**: Step-by-step configuration tutorial via userscript

**Outline**:
```markdown
# Keypad Configuration via Userscript

## Prerequisites
- Keypad module installed and connected to main controller
- OTS userscript installed in Tampermonkey
- OpenFront.io game accessible in browser
- Firmware WebSocket server running (port 3000)

## Step 1: Access Configuration UI
1. Open OpenFront.io in browser
2. Userscript connects to firmware WebSocket automatically
3. Click Tampermonkey icon in toolbar
4. Select "OTS Keypad Config" from menu
5. New tab/window opens with keypad configuration interface

## Step 2: Review Default Configuration
1. Visual keypad shows 15 keys (K1-K15)
2. Default bindings already configured:
   - K1 = "Build City" (keyboard 1)
   - K2 = "Build Factory" (keyboard 2)
   - K3 = "Build Port" (keyboard 3)
   - etc. (see full list above)
3. Click any key button to customize
4. Configuration panel opens with current binding
5. Select different action from dropdown or enter custom DOM selector
6. (Optional) Enter custom label for reference
7. Click "Save"
8. Configuration stored in browser localStorage

## Step 3: Configure Remaining Keys
Default mappings (pre-configured):
- K1: Build City
- K2: Build Factory
- K3: Build Port
- K4: Build Defense Post
- K5: Build Missile Launcher
- K6: Build SAM
- K7: Build Warship
- K8: Zoom In
- K9: Zoom Out
- K10: Decrease Attack Ratio
- K11: Switch Missile Direction
- K12: Increase Attack Ratio
- K13: Boat Attack
- K14: Land Attack
- K15: Toggle View

## Step 4: Test Configuration
1.Close configuration tab
2. Return to OpenFront.io game tab
3. Press physical key on keypad
4. Verify userscript receives event and triggers game action
5. Check console logs for debugging (F12)

## Step 5: Managing Configuration
Configuration persists in browser localStorage
- Stored per-browser, per-domain (OpenFront.io)
- Export config: Use userscript export button → JSON file
- Import config: Use userscript import button → Load JSON file
- Reset to defaults: Click "Reset All" button in config UI

## Advanced Configuration

### Custom Action Mapping
- Map any key to any game action DOM click
- Use browser console to identify game button selectors
- Store custom selectors in localStorage schema

### Multiple Browsers
- Configuration is per-browser
- Re-configure in each browser, or
- Export from one browser, import to another

### Backup & Restore
- Export configuration JSON before browser reset
- Store backups in safe location
- Import anytime to restore bindings
```

---

### 3. `/doc/user/keypad-troubleshooting.md`

**Purpose**: Troubleshooting common issues

**Outline**:
```markdown
# Keypad Troubleshooting

## Key Not Responding

### Symptom
Physical key press doesn't trigger action

### Possible Causes
1. Key not configured (unassigned in userscript)
2. CAN bus disconnected
3. WebSocket connection lost
4. Matrix wiring problem

### Solutions
1. Check key binding in userscript config tab
2. Verify CAN connection (dashboard status indicator)
3. Verify WebSocket connected (userscript console logs)
4. Test with cantest tool

## LED Not Lighting

### Symptom
LED stays off or wrong color

### Possible Causes
1. Power issue (5V supply)
2. SK6812 data line problem
3. Firmware LED driver issue
4. Wrong configuration

### Solutions
1. Check 5V rail voltage
2. Test LED with test firmware
3. Flash latest firmware
4. Reset LED configuration

## Configuration Not Persisting

### Symptom
Key bindings reset after browser restart

### Possible Causes
1. Browser localStorage cleared
2. Private/Incognito mode (no persistence)
3. Browser storage quota exceeded
4. Wrong domain/URL for game

### Solutions
1. Export configuration before closing browser
2. Use normal browser mode (not private)
3. Clear other site data to free space
4. Verify correct OpenFront.io URL

## CAN Bus Issues

### Symptom
"Keypad Disconnected" in dashboard

### Possible Causes
1. CAN bus not terminated (120Ω)
2. Wrong CAN speed (should be 500kbit/s)
3. Loose connections
4. Bus error (too many errors)

### Solutions
1. Add termination resistor
2. Verify CAN bitrate in firmware config
3. Check connectors
4. Monitor CAN errors with cantest

## USB Mode Not Working

### Symptom
USB keyboard not recognized

### Possible Causes
1. USB cable issue (data lines)
2. HID descriptor problem
3. Conflicting USB devices
4. Driver issue (Windows)

### Solutions
1. Use known-good USB-C cable
2. Flash latest firmware
3. Disconnect other keyboards
4. Install drivers (if needed)

## Factory Reset

### How to Reset
1. Power off keypad
2. Hold K1 + K15 keys
3. Power on while holding
4. Wait for all LEDs to flash
5. Release keys
6. Configuration erased, defaults restored

## Getting Help

- Check Discord #technical-support
- File GitHub issue with logs
- Email support@openfront.io
- See developer docs for advanced debugging
```

---

## Developer Documentation Content

### 1. `/doc/developer/keypad-firmware-architecture.md`

**Purpose**: Firmware design overview for developers

**Content**: Sync from `plans/01-keypad-firmware-architecture.md`
- Module architecture
- Data flow diagrams
- Task structure
- Memory layout
- GPIO configuration

---

### 2. `/doc/developer/keypad-can-protocol.md`

**Purpose**: CAN protocol reference

**Content**: Extract from `plans/03-can-protocol-specification.md`
- Message formats (KEY_EVENT, LED_SET, LED_BULK)
- CAN ID allocation
- C implementation examples
- Debugging tips

---

### 3. `/doc/developer/keypad-integration-guide.md`

**Purpose**: How to integrate keypad support into custom firmware

**Outline**:
```markdown
# Keypad Integration Guide

## Overview
How to add keypad support to custom OTS builds

## Prerequisites
- ots-fw-shared components installed
- CAN bus configured
- ESP-IDF 5.x

## Step 1: Add Shared Component
```cmake
list(APPEND EXTRA_COMPONENT_DIRS "../ots-fw-shared/components")
set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} PARENT_SCOPE)
```

## Step 2: Include Headers
```c
#include "can_driver.h"
#include "can_protocol_keypad.h"
```

## Step 3: Handle Key Events
See code examples in main controller plan...

## Step 4: Send LED Commands
See code examples...

## Testing
- Use cantest tool for CAN bus validation
- Test with physical keypad
- Verify LED updates
```

---

## VitePress Site Updates

### `/ots-website/user/index.md`

Add to module list:
```markdown
## Hardware Modules

- [Main Power Module](./main-power-module.md)
- [Troops Module](./troops-module.md)
- [Keypad Module](./keypad-module.md) ← NEW
- [Alert Module](./alert-module.md)
- [Nuke Module](./nuke-module.md)
- [Sound Module](./sound-module.md)
```

### `/ots-website/developer/index.md`

Add to architecture guides:
```markdown
## Firmware Architecture

- [Module System](./module-system.md)
- [CAN Bus Protocol](./can-protocol.md)
- [WebSocket Protocol](./websocket-protocol.md)
- [Keypad Firmware](./keypad-firmware-architecture.md) ← NEW
```

### `/ots-website/downloads.md`

Add firmware download:
```markdown
## Firmware

### Main Controller
- [ots-fw-main v1.0.0](...)

### Modules
- [ots-fw-audiomodule v1.0.0](...)
- [ots-fw-keypad v0.1.0](...)  ← NEW
```

---

## Documentation Writing Guidelines

### Tone & Style
- Friendly and approachable for users
- Technical and precise for developers
- Use active voice ("Click the button" not "The button should be clicked")
- Short sentences (max 20 words)

### Structure
- Use clear headings (H2, H3)
- Include code examples for developers
- Add screenshots for user guides (TODO: capture screenshots)
- Provide troubleshooting for common issues

### Formatting
- Use **bold** for UI elements ("Click **Edit** button")
- Use `code` for technical terms, filenames, commands
- Use > blockquotes for important notes
- Use tables for comparison data
- Use lists for steps/options

### Cross-References
- Link to related documentation
- Reference hardware specs when relevant
- Link to GitHub issues for known bugs
- Link to Discord for community support

---

## Screenshots Needed

### User Guide Screenshots
- [ ] Dashboard keypad module (idle state)
- [ ] Dashboard keypad module (edit mode)
- [ ] Key binding configuration modal
- [ ] Physical keypad with labeled keys
- [ ] LED states (green, red, yellow, off)
- [ ] CAN bus connection diagram
- [ ] USB connection diagram

### Developer Guide Screenshots
- [ ] PlatformIO build output
- [ ] Serial monitor logs (key events)
- [ ] Logic analyzer capture (CAN bus)
- [ ] LED timing diagram
- [ ] VitePress local dev server

---

## Implementation Steps

### Phase 1: User Documentation (Week 1)
1. ✅ Write `keypad-module.md` (overview + features)
2. ✅ Write `keypad-configuration.md` (tutorial)
3. ✅ Write `keypad-troubleshooting.md` (common issues)
4. ✅ Update user index with keypad links
5. ⏳ Capture screenshots (after hardware build)

### Phase 2: Developer Documentation (Week 2)
1. ✅ Copy `01-keypad-firmware-architecture.md` → `/doc/developer/`
2. ✅ Extract CAN protocol from plan #3 → `keypad-can-protocol.md`
3. ✅ Write `keypad-integration-guide.md`
4. ✅ Update developer index with keypad links
5. ✅ Update `can-protocol.md` with keypad sections

### Phase 3: VitePress Site (Week 2)
1. ✅ Sync markdown files to `ots-website/`
2. ✅ Update site navigation
3. ✅ Add download links (once firmware released)
4. ✅ Deploy to GitHub Pages
5. ✅ Verify all links work

### Phase 4: Review & Polish (Week 3)
1. ⏳ Technical review by team
2. ⏳ User testing feedback
3. ⏳ Fix typos and formatting
4. ⏳ Add missing screenshots
5. ⏳ Final deployment

---

## Related Documentation

- **Firmware Architecture**: `01-keypad-firmware-architecture.md`
- **CAN Protocol**: `03-can-protocol-specification.md`
- **WebSocket Protocol**: `04-websocket-protocol-integration.md`
- **Dashboard UI**: `05-dashboard-ui-integration.md`
- **VitePress Guide**: `/prompts/DOCUMENTATION_GUIDELINES.md`
