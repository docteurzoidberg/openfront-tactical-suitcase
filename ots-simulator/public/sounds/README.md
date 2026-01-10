# Local Sound Assets (Dashboard / Emulator)

This folder is for **local development sound files** (for the dashboard/emulator).

It is separate from the Sound Module SD card, but uses the same `soundIndex` convention so everything stays aligned.

## Naming convention

- Use a 4-digit index prefix matching the Sound Module `soundIndex`.
- Prefer MP3, with WAV as fallback.

Examples:
- `0001-game_start.mp3` (preferred)
- `0001-game_start.wav` (fallback)

## Current sound catalog

- `0001-game_start.*` → `SOUND_PLAY { soundId: "game_start" }`
- `0002-game_player_death.*` → `SOUND_PLAY { soundId: "game_player_death" }`
- `0003-game_victory.*` → `SOUND_PLAY { soundId: "game_victory" }`
- `0004-game_defeat.*` → `SOUND_PLAY { soundId: "game_defeat" }`
- `0100-audio-ready.*` → `SOUND_PLAY { soundId: "audio-ready" }` (audio module boot-ready)

See the canonical mapping table in `prompts/WEBSOCKET_MESSAGE_SPEC.md` (Sound Catalog).
