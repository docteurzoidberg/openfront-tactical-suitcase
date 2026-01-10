# Downloads

Get the latest hardware files, firmware, and userscript for your OTS device.

## Quick Links (Hosted Here)

These links point to static files hosted directly by this site (VitePress `public/`).

- Firmware (latest): [/releases/latest/firmware/ots-fw-main.bin](/releases/latest/firmware/ots-fw-main.bin)
- Audio module firmware (latest): [/releases/latest/firmware/ots-fw-audiomodule.bin](/releases/latest/firmware/ots-fw-audiomodule.bin)
- Userscript (latest): [/releases/latest/userscript/userscript.user.js](/releases/latest/userscript/userscript.user.js)

## Release Archives (Hosted Here)

### 2026-01-10.1

- Main firmware: [/releases/2026-01-10.1/firmware/ots-fw-main-2026-01-10.1.bin](/releases/2026-01-10.1/firmware/ots-fw-main-2026-01-10.1.bin)
- Audio module firmware: [/releases/2026-01-10.1/firmware/ots-fw-audiomodule-2026-01-10.1.bin](/releases/2026-01-10.1/firmware/ots-fw-audiomodule-2026-01-10.1.bin)
- Userscript: [/releases/2026-01-10.1/userscript/userscript-2026-01-10.1.user.js](/releases/2026-01-10.1/userscript/userscript-2026-01-10.1.user.js)

## Hardware Files

### CAD Files

Mechanical design files for the suitcase enclosure and module mounting.

::: info Coming Soon
CAD files will be available in the next release.
:::

When available, they will be hosted under:

- `/releases/latest/hardware/cad/` (STEP, ZIP)
- `/releases/latest/hardware/fusion360/` (F3Z, ZIP)

### PCB Files (KiCad)

Complete KiCad projects for all OTS circuit boards.

::: info Coming Soon
PCB schematics and designs will be available soon.
:::

When available, they will be hosted under:

- `/releases/latest/hardware/kicad/` (ZIP)

**Planned boards:**
- Main Controller Board (ESP32-S3)
- I/O Expander Board (MCP23017)
- Power Distribution Board
- Module Connector Boards

## Notes

- Changelog + history: See the [Releases](/releases) page.
- Userscript install instructions: See the [Userscript Installation Guide](/user/userscript-install).

## Source Code

All source code is available on GitHub:

**[View Repository on GitHub →](https://github.com/docteurzoidberg/openfront-tactical-suitcase)**

### Repository Structure

- `ots-fw-main/` - Main ESP32-S3 firmware
- `ots-simulator/` - Nuxt dashboard and WebSocket simulator  
- `ots-userscript/` - Browser userscript
- `ots-hardware/` - Hardware specifications and PCB designs
- `doc/` - Documentation (this website)
