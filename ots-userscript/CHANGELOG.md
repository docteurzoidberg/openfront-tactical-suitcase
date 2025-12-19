# Changelog

All notable changes to the OTS Userscript will be documented in this file.

## [0.1.0] - 2024-12-19

### Added
- Initial release with nuke, boat, and land attack tracking
- WebSocket connection to OTS server/firmware
- Real-time troop monitoring with intelligent change detection
- Attack ratio (troop deployment percentage) monitoring from game DOM
- Bidirectional attack ratio synchronization (game ↔ dashboard)
- `set-attack-ratio` command handler for dashboard-to-game control
- Automatic troop count division correction (game reports 10x actual values)
- HUD window with draggable, resizable interface
- Event logging and connection status indicators

### Features
- **Troop Monitor**: Polls at 100ms but only sends WebSocket updates when values change
- **Attack Ratio Tracking**: Reads directly from game's `#attack-ratio` input element
- **Change Detection**: Monitors input/change events on slider for immediate updates
- **Initial State Sync**: Sends troop data immediately when game starts
- **Command Handler**: Dashboard can update game's attack ratio remotely

### Technical Details
- Built with TypeScript and esbuild
- Uses Tampermonkey/Greasemonkey APIs for persistent configuration
- WebSocket protocol with JSON message format
- Event-driven architecture with change detection (99%+ efficiency vs blind polling)

