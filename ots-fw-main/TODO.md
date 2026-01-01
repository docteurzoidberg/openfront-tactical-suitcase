# OTS Firmware (ots-fw-main) — TODO

## Testing

- [x] testing websocket + protocol
  - Status: stable (WSS + userscript presence semantics)
  - Validated:
    - WSS server `/ws` and handshake detection for `clientType: "userscript"`
    - Userscript presence tracking is based on 0→1 / 1→0 transitions and also handles abrupt socket close (`close_fn`)
    - Serial-log “contract” validated via stdlib-only Python integration test
    - INFO heartbeats are intentionally dropped (not enqueued) to avoid event queue pressure
- [ ] testing output board
  - Initial test firmware created (`test-outputs`).
  - Next: verify with real MCP23017 @ 0x21 connected.
- [ ] testing input board
  - Initial test firmware created (`test-inputs`).
  - Next: verify with real MCP23017 @ 0x20 connected.
- [x] testing lcd driver
  - Test firmware exists (`test-lcd`) and LCD diagnostics helper exists (`tests/lcd_diag.py`).
- [ ] testing adc driver
  - Initial test firmware created (`test-adc`).
  - Next: verify ADS1015 readings + 1% threshold behavior.

## Automation

- [x] generic Python device tool + tests
  - `tools/ots_device_tool.py`
  - `tools/tests/userscript_connect_disconnect.py`
  - Wrapper entrypoint: `tools/test_ws_firmware.py`

## Features

- [ ] implementing can bus protocol to send event to future modules (audio)

## Follow-ups

- [ ] investigate persistent flash size mismatch warning (if still present on your board)
