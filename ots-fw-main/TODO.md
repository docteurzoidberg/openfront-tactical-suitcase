# OTS Firmware (ots-fw-main) — TODO

## Active Tasks

### Hardware Testing

- [ ] Testing output board with physical MCP23017 @ 0x21
- [ ] Testing input board with physical MCP23017 @ 0x20
- [ ] Testing ADC driver with ADS1015 - verify 1% threshold behavior
- [ ] Testing CAN bus communication between main firmware and audio module
  - Validate PLAY_SOUND, STOP_SOUND, STATUS, ACK message exchange
  - Verify 500kbps CAN bus stability with real hardware

### Features

- [ ] CAN bus sound module integration
  - Protocol defined in `/ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md`
  - Shared component: `/ots-fw-shared/components/can_audiomodule/`

## Completed

- ✅ WebSocket + protocol testing (WSS, userscript presence)
- ✅ LCD driver testing (`test-lcd` + diagnostics)
- ✅ Python device tool (`ots_device_tool.py`)
- ✅ Automated test scripts
