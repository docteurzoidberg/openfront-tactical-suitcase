```mdc
# TEST_PRODUCTION_LIVE_GAME — Phase 3.4 (prod main + prod audio)

This scenario runner validates **production main firmware** + **production audio firmware** together, while **mocking a live game/userscript** via WebSocket events.

It does **not** use `ots-fw-cantest` firmware on either board.

## What it covers

- Main firmware (WS server) accepts a userscript-style WSS client.
- Main firmware forwards `SOUND_PLAY` to CAN as `PLAY_SOUND`.
- Audio firmware responds with `SOUND_ACK` and `SOUND_FINISHED`.
- Audio firmware emits periodic `SOUND_STATUS (0x426)`.
- Main firmware discovers the audio module via CAN discovery.
- A small set of game events is replayed (GAME_START, TROOP_UPDATE, ALERT_ATOM, NUKE_*).

## Prerequisites

- Main controller flashed with `ots-fw-main` (production environment).
- Audio module flashed with `ots-fw-audiomodule` (production environment).
- Both boards wired on the same CAN bus and powered.
# TEST_PRODUCTION_LIVE_GAME — Phase 3.4 (prod main + prod audio)

This scenario runner validates production main firmware + production audio firmware together, while mocking a live game/userscript via WebSocket events.

It does not use `ots-fw-cantest` firmware on either board.

## What it covers

- Main firmware (WS server) accepts a userscript-style WSS client.
- Main firmware forwards `SOUND_PLAY` to CAN as `PLAY_SOUND`.
- Audio firmware responds with `SOUND_ACK` and `SOUND_FINISHED`.
- Audio firmware emits periodic `SOUND_STATUS (0x426)`.
- Main firmware discovers the audio module via CAN discovery.
- A mock game stream is replayed.

By default (`--profile full`), the stream includes every in-game WebSocket event type defined in `ots-shared`, plus sound events tied to phases/events.

## Prerequisites

- Main controller flashed with `ots-fw-main` (production env).
- Audio module flashed with `ots-fw-audiomodule` (production env).
- Both boards wired on the same CAN bus and powered.
- Serial ports accessible:
  - Main (example): `/dev/ttyACM0`
  - Audio (example): `/dev/ttyUSB0`

## Run

From repo root:

```bash
./ots-fw-cantest/tools/test_production_live_game.py \
  --main-port /dev/ttyACM0 \
  --audio-port /dev/ttyUSB0
```

If IP derivation fails (unusual serial log formats), pass the firmware host/IP:

```bash
./ots-fw-cantest/tools/test_production_live_game.py \
  --main-port /dev/ttyACM0 \
  --audio-port /dev/ttyUSB0 \
  --ws-host 192.168.1.50
```

## Options

### Logging

- Live raw serial streaming (`[TEST]`, `[MAIN]`, `[AUDIO]`) is **enabled by default**; disable with `--no-live-logs`.
- Verbose TEST progress logging is **enabled by default**; disable with `--quiet` / `--no-verbose`.
- ANSI colors are enabled when stdout is a TTY; disable with `--no-color`.

### Profiles

- `--profile full` (default): emits all in-game event types at least once:
  - `INFO`, `ERROR`
  - `GAME_SPAWNING`, `GAME_START`, `GAME_END`
  - `TROOP_UPDATE`
  - `ALERT_ATOM`, `ALERT_HYDRO`, `ALERT_MIRV`, `ALERT_LAND`, `ALERT_NAVAL`
  - `NUKE_LAUNCHED`, `NUKE_EXPLODED`, `NUKE_INTERCEPTED`
  - `HARDWARE_TEST` plus a command→response exercise for `HARDWARE_DIAGNOSTIC`
  - multiple `SOUND_PLAY` events tied to phases/events

- `--profile minimal`: a smaller “happy-path” stream.

### Sound Mode

- `--sound-mode embedded` (default): always includes `soundIndex=10000` so the audio module can play a deterministic embedded tone (no SD card required).
- `--sound-mode catalog`: uses the canonical `soundId → soundIndex` mapping for known IDs (`game_start`, `game_victory`, `game_defeat`, `game_player_death`). Unknown sound IDs fall back to `soundIndex=10000`.

## Expected pass criteria

The runner waits for (regex matches):

- Audio logs:
  - `CAN STATUS task started`
  - `STATUS: bits=0x..`
  - `Received MODULE_QUERY, announcing...`
  - `PLAY_SOUND: index=...`
  - `Sent ACK: ok=1 ...`
  - `Sound finished: queue_id=...`

- Main logs:
  - `Audio module vX.Y discovered ...`
  - `Received SOUND_PLAY event`
  - `Queued PLAY_SOUND: index=...`
  - `Received SOUND_ACK ...`
  - `Received SOUND_FINISHED ...`
  - `Atom alert! (unit=...)`
  - `Nuke launched: ... unit=...`

On failure, it prints the tail of both serial streams.

## Notes

- The stream is spaced out intentionally to avoid event-queue pressure on the main firmware.
- Serial readers use stdlib `termios` and open ports read-only; don’t run another monitor simultaneously.
