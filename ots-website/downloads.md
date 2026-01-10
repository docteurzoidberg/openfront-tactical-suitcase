# Downloads

Get the latest hardware files, firmware, and userscript for your OTS device.

## Quick Links (Hosted Here)

These links point to static files hosted directly by this site (VitePress `public/`).

- Firmware (latest): [/releases/latest/firmware/ots-fw-main.bin](/releases/latest/firmware/ots-fw-main.bin)
- Audio module firmware (latest): [/releases/latest/firmware/ots-fw-audiomodule.bin](/releases/latest/firmware/ots-fw-audiomodule.bin)
- Userscript (latest): [/releases/latest/userscript/userscript.user.js](/releases/latest/userscript/userscript.user.js)

::: tip Where to put files
Drop release artifacts under `ots-website/public/releases/...`.

Example:
- `ots-website/public/releases/latest/firmware/ots-fw-main.bin`
- `ots-website/public/releases/latest/userscript/userscript.user.js`
:::

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

## Firmware

Latest firmware releases for ESP32-S3 devices.

- Hosted on this site: [/releases/latest/firmware/ots-fw-main.bin](/releases/latest/firmware/ots-fw-main.bin)
- Changelog + history: See the [Releases](/releases) page.

## Userscript

Built Tampermonkey userscript to connect OpenFront.io game to your OTS device.

- Hosted on this site: [/releases/latest/userscript/userscript.ots.user.js](/releases/latest/userscript/userscript.ots.user.js)
- Hosted on this site: [/releases/latest/userscript/userscript.user.js](/releases/latest/userscript/userscript.user.js)
- Instructions: See the [Userscript Installation Guide](/user/userscript-install).

::: warning Large files
GitHub Pages works great for firmware and small ZIPs. For very large files (Fusion360 exports, big CAD bundles) or lots of historical releases, consider attaching artifacts to GitHub Releases and linking them here.
:::

## Source Code

All source code is available on GitHub:

**[View Repository on GitHub →](https://github.com/docteurzoidberg/openfront-tactical-suitcase)**

### Repository Structure

- `ots-fw-main/` - Main ESP32-S3 firmware
- `ots-simulator/` - Nuxt dashboard and WebSocket simulator  
- `ots-userscript/` - Browser userscript
- `ots-hardware/` - Hardware specifications and PCB designs
- `doc/` - Documentation (this website)
