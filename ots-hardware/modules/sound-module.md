# Sound Module (8U)

## Overview

The Sound Module provides **audio feedback** for important game events (alerts, launches, outcomes, etc.).

Unlike other OTS modules, it contains a **dedicated ESP32** with its own firmware and audio circuitry. The main controller (ESP32-S3 running `ots-fw-main`) communicates with the Sound Module over **CAN bus** to request playback of sound assets stored on an SD card.

## Module Specifications

- **Size**: 8U (Standard height - Half module)
- **Module Type**: Output-only (sound)
- **Bus**: CAN (module is not an MCP23017/I2C GPIO expander)
- **Power**: From suitcase bus (12V + 5V)

## Hardware Components

### Core
- **ESP32-A1S (AI Thinker ESP32 Audio Kit v2.2)** running its own firmware (`ots-fw-audiomodule`)
- SD card slot (FAT32) containing audio files
- Audio output stage + **3Ω speaker**

### Controls
- **Volume potentiometer** (analog, local-only)
- **On/Off switch** that disables the entire module (no sound)

Notes:
- The on/off switch is assumed to be a **hard power cut** to the module. When OFF, the module will not respond on CAN.
- Volume is assumed to be **local-only** via the pot; the main controller does not need to set volume unless you want remote override.

## CAN Communication (High Level)

### Physical / Link Layer

- **CAN**: CAN 2.0 (classic frames, 8-byte payload)
- **Bitrate**: 500 kbps (recommended default)
- **IDs**: Standard 11-bit identifiers
- **Bus behavior**:
  - Sound module may be OFF (hard switch), meaning it is silent on the bus.
  - Main controller must treat sound as best-effort and never block waiting for responses.

### Message IDs

All frames are 8 bytes.

- `0x420` **PLAY_SOUND** (main → sound)
- `0x421` **STOP_SOUND** (main → sound)
- `0x422` **SOUND_STATUS** (sound → main)
- `0x423` **SOUND_ACK** (sound → main, optional)

### Payload format

All multi-byte integers are **little-endian**.

#### `0x420` PLAY_SOUND (main → sound)

| Byte | Name | Type | Notes |
|---:|---|---|---|
| 0 | `cmd` | u8 | `0x01` |
| 1 | `flags` | u8 | bit0=`interrupt`, bit1=`highPriority`, bit2=`loop` |
| 2-3 | `soundIndex` | u16 | Index of sound asset on SD (see naming below) |
| 4 | `volumeOverride` | u8 | `0..100`, or `0xFF` = ignore (use pot) |
| 5 | `reserved` | u8 | set `0` |
| 6-7 | `requestId` | u16 | Optional correlation (0 allowed) |

#### `0x421` STOP_SOUND (main → sound)

| Byte | Name | Type | Notes |
|---:|---|---|---|
| 0 | `cmd` | u8 | `0x02` |
| 1 | `flags` | u8 | bit0=`stopAll` |
| 2-3 | `soundIndex` | u16 | `0xFFFF` = stop any/current |
| 4-5 | `reserved` | u16 | set `0` |
| 6-7 | `requestId` | u16 | Optional correlation |

#### `0x422` SOUND_STATUS (sound → main)

Heartbeat/status frame (recommended at ~1 Hz when powered).

| Byte | Name | Type | Notes |
|---:|---|---|---|
| 0 | `cmd` | u8 | `0x80` |
| 1 | `stateBits` | u8 | bit0=`ready`, bit1=`sdMounted`, bit2=`playing`, bit3=`mutedBySwitch`, bit4=`error` |
| 2-3 | `currentSoundIndex` | u16 | `0xFFFF` if none |
| 4 | `lastErrorCode` | u8 | `0` if none |
| 5 | `currentVolume` | u8 | `0..100` (from pot), or `0xFF` unknown |
| 6-7 | `uptimeSeconds` | u16 | wraps naturally |

#### `0x423` SOUND_ACK (sound → main, optional)

| Byte | Name | Type | Notes |
|---:|---|---|---|
| 0 | `cmd` | u8 | `0x81` |
| 1 | `ok` | u8 | `1` ok, `0` failed |
| 2-3 | `soundIndex` | u16 | Echo requested index |
| 4 | `errorCode` | u8 | `0` if ok |
| 5 | `reserved` | u8 | set `0` |
| 6-7 | `requestId` | u16 | Echo from request |

### SD card naming convention (recommended)

To keep CAN payloads small, the Sound Module uses `soundIndex` to choose a file.

- Directory: `/sounds/`
- File naming: 4-digit index + extension
  - MP3 preferred: `/sounds/0020.mp3`
  - WAV fallback: `/sounds/0020.wav`

The Sound Module should try MP3 first and fall back to WAV if MP3 decode is not available.

For the canonical mapping of `soundId` → `soundIndex`, see `prompts/protocol-context.md` (Sound Catalog).

## Game / Protocol Mapping (Planned)

### Trigger source
- `ots-userscript` emits events indicating a sound should play.
- `ots-server` relays those events.
- `ots-fw-main` receives them and issues a CAN request to the Sound Module.
- In emulator mode, `ots-server` may simulate this behavior (and optionally play a local audio clip).

### Recommended event modeling
The cleanest approach is usually a **single generic event** like:

- `SOUND_PLAY`

With event data such as:

```ts
// Proposed (not yet in protocol)
export type SoundPlayEventData = {
  soundId: string;          // e.g. "alert.atom", "nuke.launch.atom"
  variant?: string;         // optional: "1" | "2" | "long" | etc.
  priority?: number;        // optional: 0..N for mixing/queue policy
  requestId?: string;       // optional correlation
}
```

Alternatively, if you prefer explicit events (e.g. `SOUND_ALERT_ATOM`, `SOUND_NUKE_LAUNCHED`, ...), we can document that instead—generic is usually easier to extend.

## Module Behavior (Intended)

### On Sound Trigger
1. A sound-trigger event arrives from the userscript (via server).
2. Main controller maps the event to a sound asset identifier.
3. Main controller sends a CAN "play" command.
4. Sound module plays the corresponding file from SD.

### Suggested playback policies (TBD)
- **Queue vs interrupt**: should new sounds interrupt current playback?
- **Priority**: should alerts interrupt informational sounds?
- **Dedup**: should repeated events collapse (e.g. rapid ALERT spam)?

## UI Component Mockup (Emulator)

Vue component path (planned): `ots-server/app/components/hardware/SoundModule.vue`

### Props
```ts
interface SoundModuleProps {
  connected: boolean
  lastSoundId?: string
  muted: boolean // derived from module on/off or emulator toggle
}
```

### Emits
```ts
// Likely none for the physical module.
// (Optional) emulator-only controls could emit play-test events.
```

## Firmware Notes

### Main controller (`ots-fw-main`)
- Needs a CAN driver and a small adapter layer to translate received game events into CAN "play" requests.

### Sound module firmware (separate)
- Runs on its own ESP32.
- Receives CAN messages.
- Mounts SD card.
- Plays sound assets over DAC/I2S + amplifier.

## Open Questions

1. **Audio format details**: MP3 preferred; WAV fallback. Confirm exact constraints:
  - MP3 bitrates/sample rates expected?
  - Any required directory layout / naming on SD?
2. **CAN protocol**:
   - CAN IDs for play/stop/status?
   - Payload: `soundId` index, hash, or fixed numeric IDs?
   - Ack required, or fire-and-forget?
3. **Playback policy**: queue vs interrupt, priority rules.

