# Releases

Download the latest firmware and userscript releases for OTS.

## Latest Releases

### Firmware

Latest firmware for ESP32-S3 main controller.

::: tip Installation
See the [Developer Guide](/developer/getting-started) for firmware flashing instructions.
:::

**Latest Release:** Coming soon

### Userscript

Browser extension to connect OpenFront.io game to your hardware.

::: tip Installation
See the [Userscript Installation Guide](/user/userscript-install) for setup instructions.
:::

**Latest Release:** Coming soon

## Release History

All releases are tagged and available on GitHub:

**[View All Releases on GitHub →](https://github.com/docteurzoidberg/openfront-tactical-suitcase/releases)**

## Release Format

Releases follow the date-based format: `YYYY-MM-DD.N`

Example: `2026-01-05.1` (first release on January 5, 2026)

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
