# Sound Module Prompt (CAN-controlled audio peripheral)

Use this prompt when implementing or modifying Sound Module integration in `ots-fw-main`.

## What this module is

- A **separate ESP32-based board** with audio circuitry + SD card.
- The Sound Module has its **own firmware** and plays audio files from SD.
- The main controller (`ots-fw-main` on ESP32-S3) sends **CAN bus** requests to play sounds.
- The module includes:
  - 3Ω speaker
  - volume potentiometer (local)
  - hardware on/off switch (module can be totally absent from CAN)

Known hardware choice:
- **ESP32-A1S (AI Thinker ESP32 Audio Kit v2.2)**
- Firmware repo/subproject: `ots-fw-audiomodule`

## Key constraints

- Treat the Sound Module as **best-effort**: it may be powered off.
- The main controller should never block gameplay logic waiting for sound.
- Prefer a **single mapping table** (event → soundId) and keep it small.

## Integration target behavior (main controller)

1. Receive a "play sound" request from the game side.
   - In the current architecture, this likely arrives as a `GameEvent` from `ots-server`.
   - We will likely introduce a protocol event later (e.g. `SOUND_PLAY`), but do not invent multiple redundant message types.
2. Map the request to a stable `soundId` (string or numeric).
3. Send a CAN frame to the Sound Module.
4. Optionally track delivery via ack/heartbeat, but do not require it.

## CAN design (TBD — ask for details)

Recommended defaults (can be revised once the CAN bus is finalized):

- CAN 2.0 classic, 500 kbps, standard 11-bit IDs
- IDs:
  - `0x420` PLAY_SOUND (main → sound)
  - `0x421` STOP_SOUND (main → sound)
  - `0x422` SOUND_STATUS (sound → main)
  - `0x423` SOUND_ACK (sound → main, optional)

Payloads are 8 bytes, little-endian multi-byte integers.

**PLAY_SOUND (`0x420`)**:
- `cmd=0x01`, `flags` (interrupt/highPriority/loop), `soundIndex (u16)`, `volumeOverride (u8, 0xFF=ignore)`, `requestId (u16)`

**STOP_SOUND (`0x421`)**:
- `cmd=0x02`, `flags` (stopAll), `soundIndex (u16, 0xFFFF=any)`, `requestId (u16)`

Playback policy is still TBD (queue vs interrupt, priority rules). Treat `flags` as hints.

### Suggested pragmatic protocol (example)

- `CAN_ID_PLAY_SOUND`: payload
  - `uint16 sound_index`
  - `uint8 flags` (bit0=interrupt, bit1=high_priority)
  - `uint8 reserved`
- `CAN_ID_STATUS`: payload
  - `uint8 ready`
  - `uint8 playing`
  - `uint16 current_sound_index`

(Adjust to your actual hardware/firmware constraints.)

## Event mapping strategy

Keep mapping centralized (single table), e.g.:

- `ALERT_ATOM` → `sound_index = 10`
- `NUKE_LAUNCHED` → `sound_index = 20`
- `GAME_END` with `victory=true` → `sound_index = 30`

Avoid scattering mapping logic across multiple modules.

## Emulator expectations (server)

In `ots-server` emulator mode:
- It should be able to show "Sound requested: X" in the UI.
- Actual audio playback in the browser is optional and can be added later.

## Failure modes to handle

- Sound Module absent/off: CAN TX may succeed but no responses.
- SD card missing: module should signal error (if status frames exist).
- Rapid event bursts: implement dedup or simple throttling on main controller side if needed.

## Open questions to ask the user

1. Confirm **event name(s)** and payload shape used by the userscript (generic `SOUND_PLAY` vs many explicit events).
2. Confirm **sound asset list** (names) and how they map to IDs.
3. Confirm **CAN protocol** (IDs, payloads, bitrate, ack policy).
4. Confirm **audio format** constraints:
  - MP3 preferred; WAV fallback.
  - Any specific codec/bitrate/sample-rate limits for MP3?
5. Confirm whether the main controller should ever request **volume** or **mute**, or if volume is strictly local.
