# OTS Firmware (ots-fw-main) — TODO

## Testing

- [ ] testing websocket + protocol  
  - Status: in progress
  - Validated so far via `pio run -e test-websocket` (TEST_WEBSOCKET):
    - WiFi credentials provisioning via `config.h` (NVS-backed credentials not validated / not working yet).
    - WiFi connect + DHCP IP acquisition via `network_manager`.
    - WebSocket server bring-up after IP is acquired (`ws_server_start()`), with `/ws` endpoint.
    - WS vs WSS server mode (TLS on/off via `WS_USE_TLS`), including self-signed cert flow for WSS.
    - Connection lifecycle callbacks fire (client connect/disconnect).
    - Userscript presence detection works via either:
      - handshake `{ "type": "handshake", "clientType": "userscript" }`, or
      - INFO event message `"userscript-connected"`.
    - Protocol envelope parsing for inbound messages (`type` + `payload`) through `ws_protocol_parse()`:
      - `handshake` recognized and tracked
      - `event` recognized and routed into `event_dispatcher`
      - INFO events are intentionally dropped (not enqueued) to avoid event queue pressure.
    - Protocol envelope generation for outbound events via `ws_server_send_event()`:
      - Sends `INFO` and `HARDWARE_TEST` events (smoke test that clients receive valid JSON).
    - RGB status LED indicates connection states (manual cycle test + live status during WiFi/WS/userscript/game events).
- [ ] testing output board
-  - Initial test firmware created (`test-outputs`).
- [ ] testing input board
-  - Initial test firmware created (`test-inputs`).
- [ ] testing lcd driver
-  - Initial test firmware created (`test-lcd`).
- [ ] testing adc driver
-  - Initial test firmware created (`test-adc`).

## Features

- [ ] implementing can bus protocol to send event to future modules (audio)
- [ ] support improv wifi credentials provisioning
