# OTS Firmware Main — Automated Tests Prompt

## Goal
Provide an automated, repeatable way to validate **ots-fw-main** runtime behavior on real hardware (ESP32‑S3) with minimal manual steps.

This prompt defines:
- What to test
- How to run tests locally
- What the firmware must log (serial) so tests can assert behavior
- The test harness implementation constraints

## Scope (What We Test)
These are **integration tests** against a real device, not unit tests.

Primary target behaviors:
1. **Userscript presence detection**
   - A client connecting to `/ws` and sending `{"type":"handshake","clientType":"userscript"}` is detected.
   - Firmware transitions to “userscript connected” state.

2. **Userscript disconnect handling**
   - When the userscript socket closes, firmware transitions back to “no userscript” state.
   - This transition must happen even if other sockets were previously connected (main firmware now defines *connected* as “userscript present”).

3. **System status LCD state changes**
   - On userscript connect: System status module should report connected/lobby.
   - On userscript disconnect: System status module should revert to waiting.

4. **RGB status state changes (indirectly via logs)**
   - Tests do not read the LED electrically; they validate the firmware *decision* path via logs.

## Non-Goals
- Verifying actual WS2812 electrical output.
- End-to-end OpenFront gameplay parsing.
- OTA.
- I2C/LCD hardware electrical validation (assumed already stable).

## Required Firmware Log Contract (Serial)
The tests rely on stable substrings/regexes in serial logs.

On userscript connect, firmware should emit *at least one* of:
- `OTS_WS_SERVER: Identified userscript client` (handshake path)
- `OTS_WS_SERVER: Identified userscript client via INFO` (fallback)

And then the status layer should emit:
- `SysStatus: WebSocket connected - showing lobby screen`

On userscript disconnect, firmware should emit:
- `OTS_WS_SERVER: Last userscript disconnected; userscript_clients=0`

And then:
- `SysStatus: WebSocket disconnected - showing waiting screen`

These lines are treated as the authoritative “state transition” indicators.

## Test Harness Implementation Constraints
- Prefer **stdlib-only Python** (no external dependencies) so the repo is easy to run anywhere.
- The harness must:
  - Open the serial port directly (e.g., `/dev/ttyACM1`) and parse logs in real time.
  - Create a **WSS** WebSocket connection to the device, send masked frames (client frames MUST be masked).
  - Simulate the userscript messages:
    - `handshake` with `clientType: userscript`
    - INFO `userscript-connected`
  - Close the socket and validate the disconnect transition.
- Avoid running `pio device monitor` simultaneously (it will compete for the serial port).

## Test Cases

### TC1 — Userscript Connect
**Steps**
1. (Optional) flash firmware env `esp32-s3-dev-factory`.
2. Start serial capture.
3. Connect WSS to `wss://<device-ip>:443/ws`.
4. Send userscript handshake + INFO fallback.

**Asserts**
- See `Identified userscript client` (handshake OR INFO) within 5s.
- See `SysStatus: WebSocket connected - showing lobby screen` within 5s.

### TC2 — Userscript Disconnect
**Steps**
1. From the same connection, send a close frame and/or close the socket.

**Asserts**
- See `Last userscript disconnected; userscript_clients=0` within 5s.
- See `SysStatus: WebSocket disconnected - showing waiting screen` within 5s.

### TC3 — Abrupt Disconnect (Optional)
Same as TC2 but without sending a close frame (drop the TCP socket).

## How To Run
From `ots-fw-main/`:

- Run tests (no flash):
  - `python3 tools/test_ws_firmware.py --host <device-ip> --serial-port /dev/ttyACM1 --insecure`

- Flash + run:
  - `python3 tools/test_ws_firmware.py --upload --env esp32-s3-dev-factory --host <device-ip> --serial-port /dev/ttyACM1 --insecure`

## Expected Outcome
- Script prints PASS/FAIL per test case.
- On failure, script prints the last N log lines captured and which assertion timed out.
