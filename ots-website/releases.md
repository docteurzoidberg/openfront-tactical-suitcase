# Releases

Download the latest firmware and userscript releases for OTS.

## Latest Releases

### Firmware

Latest firmware for ESP32-S3 main controller.

::: tip Installation
See the [Developer Guide](/developer/getting-started) for firmware flashing instructions.
:::

**Latest (hosted on this site):**

- [/releases/latest/firmware/ots-fw-main.bin](/releases/latest/firmware/ots-fw-main.bin)
- [/releases/latest/firmware/ots-fw-audiomodule.bin](/releases/latest/firmware/ots-fw-audiomodule.bin)

### Userscript

Browser extension to connect OpenFront.io game to your hardware.

::: tip Installation
See the [Userscript Installation Guide](/user/userscript-install) for setup instructions.
:::

**Latest (hosted on this site):**

- [/releases/latest/userscript/userscript.user.js](/releases/latest/userscript/userscript.user.js)

### Hardware (KiCad / CAD)

**Latest (hosted on this site):**

- KiCad: `/releases/latest/hardware/kicad/`
- Fusion360: `/releases/latest/hardware/fusion360/`
- CAD (STEP, etc): `/releases/latest/hardware/cad/`

## Release History

All releases are tagged and available on GitHub:

**[View All Releases on GitHub →](https://github.com/docteurzoidberg/openfront-tactical-suitcase/releases)**

::: tip When to use GitHub Releases vs this site
For firmware and small artifacts, hosting in `ots-website/public/releases/` is simple and gives stable URLs.

For very large files (Fusion360 exports, big CAD bundles) or many historical versions, GitHub Releases is usually better (keeps the docs repo lighter).
:::

## Release Format

Releases follow the date-based format: `YYYY-MM-DD.N`

Example: `2026-01-05.1` (first release on January 5, 2026)

## Static File Layout (this website)

This site can host release artifacts as static files. Put files under:

- `ots-website/public/releases/latest/...` for stable “latest” links
- `ots-website/public/releases/<tag>/...` for pinned releases

Example URLs:

- `/releases/latest/firmware/ots-fw-main.bin`
- `/releases/2026-01-09.1/firmware/ots-fw-main-2026-01-09.1.bin`
- `/releases/latest/userscript/userscript.ots.user.js`

## Automated Releases

The project uses `release.sh` for unified versioning across all components. See [RELEASE.md](https://github.com/docteurzoidberg/openfront-tactical-suitcase/blob/main/prompts/RELEASE.md) for details.

## What's Included

Each release includes:

- **Firmware Binary** (`ots-fw-main.bin`) - Flash to ESP32-S3
- **Userscript** (`userscript.ots.user.js`) - Install in Tampermonkey
- **Release Notes** - Changelog and known issues

## Beta/Development Builds

Development builds are available from the `main` branch but are not recommended for production use.

To build from source, see the [Developer Guide](/developer/getting-started).
