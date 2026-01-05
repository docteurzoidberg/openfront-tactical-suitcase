# SD Card Audio Setup Guide

Complete guide for setting up audio files on SD card for the OTS Audio Module.

## Table of Contents

1. [SD Card Requirements](#sd-card-requirements)
2. [Audio Format Requirements](#audio-format-requirements)
3. [File Organization](#file-organization)
4. [Converting Web Audio Formats](#converting-web-audio-formats)
5. [Testing Your Setup](#testing-your-setup)
6. [Troubleshooting](#troubleshooting)

---

## SD Card Requirements

### Hardware

- **SD Card Type**: MicroSD or SD card (with adapter)
- **Capacity**: 128MB - 32GB recommended
- **Speed Class**: Class 4 or higher (Class 10 recommended for reliable playback)
- **File System**: FAT32 (required)

### Formatting the SD Card

**On Linux:**
```bash
# Find your SD card device (e.g., /dev/sdb1)
lsblk

# Unmount if mounted
sudo umount /dev/sdb1

# Format as FAT32
sudo mkfs.vfat -F 32 /dev/sdb1

# Set volume label (optional)
sudo fatlabel /dev/sdb1 "OTS_AUDIO"
```

**On macOS:**
```bash
# Using Disk Utility GUI:
# 1. Open Disk Utility
# 2. Select SD card
# 3. Click "Erase"
# 4. Format: MS-DOS (FAT32)
# 5. Scheme: Master Boot Record

# Or via command line:
diskutil list
diskutil eraseDisk FAT32 OTS_AUDIO MBRFormat /dev/diskN
```

**On Windows:**
```
1. Insert SD card
2. Open "This PC"
3. Right-click SD card drive
4. Select "Format..."
5. File system: FAT32
6. Allocation unit size: Default
7. Volume label: OTS_AUDIO (optional)
8. Click "Start"
```

---

## Audio Format Requirements

The audio module requires **specific audio file formats** for proper playback.

### Required Format

- **Container**: WAV (Waveform Audio File Format)
- **Encoding**: PCM (Pulse Code Modulation) - **uncompressed**
- **Sample Rate**: 44.1 kHz (44,100 Hz)
- **Bit Depth**: 16-bit
- **Channels**: Mono (1 channel) or Stereo (2 channels)
- **Byte Order**: Little-endian (standard for WAV)

### Why These Requirements?

- **PCM 16-bit**: Direct hardware compatibility with I2S DAC, no decoding overhead
- **44.1 kHz**: Standard CD quality, optimal for ESP32 I2S peripheral
- **WAV container**: Simple header format, easy to parse on embedded systems
- **Mono/Stereo**: Both supported, stereo mixes to mono automatically

### File Size Considerations

**Formula**: `Size (bytes) = Sample Rate × Bit Depth/8 × Channels × Duration (seconds) + 44 bytes (header)`

**Examples**:
- **1 second mono**: 44,100 × 2 × 1 = 88,200 bytes (~86 KB)
- **1 second stereo**: 44,100 × 2 × 2 = 176,400 bytes (~172 KB)
- **10 second mono**: ~860 KB
- **1 minute mono**: ~5.2 MB

**SD Card Capacity Guidelines**:
- 512 MB: ~80 minutes of mono audio
- 1 GB: ~160 minutes of mono audio
- 4 GB: ~640 minutes of mono audio

---

## File Organization

### Directory Structure

The audio module reads files from the **root directory** of the SD card.

```
/
├── track1.wav          # Quick-play file (command: "1")
├── track2.wav          # Quick-play file (command: "2")
├── hello.wav           # Quick-play file (command: "hello")
├── ping.wav            # Quick-play file (command: "ping")
├── game_start.wav      # CAN sound (index 0)
├── game_victory.wav    # CAN sound (index 1)
├── game_defeat.wav     # CAN sound (index 2)
├── game_player_death.wav  # CAN sound (index 3)
├── alert_nuke.wav      # Custom sounds
├── alarm.wav
└── notification.wav
```

### Reserved Filenames

Some filenames have **special meaning** and are mapped to quick-play commands:

| Filename | Console Command | Description |
|----------|----------------|-------------|
| `track1.wav` | `1` | Quick play slot 1 |
| `track2.wav` | `2` | Quick play slot 2 |
| `hello.wav` | `hello` | Hello sound |
| `ping.wav` | `ping` | Ping sound |

### CAN Sound Mapping

Files for CAN-triggered sounds (see `sound_config.c`):

| CAN Index | Filename | Description |
|-----------|----------|-------------|
| 0 | `game_start.wav` | Game start sound |
| 1 | `game_victory.wav` | Victory sound |
| 2 | `game_defeat.wav` | Defeat sound |
| 3 | `game_player_death.wav` | Player death |
| 4 | `alert_nuke.wav` | Nuclear alert |
| 5 | `alert_land.wav` | Land invasion alert |
| 6 | `alert_naval.wav` | Naval invasion alert |
| 7 | `nuke_launch.wav` | Nuke launch sound |

### Naming Conventions

**Best Practices**:
- ✅ Use lowercase filenames (Linux is case-sensitive)
- ✅ Use underscores instead of spaces: `game_start.wav` not `game start.wav`
- ✅ Keep filenames under 32 characters
- ✅ Use descriptive names: `victory.wav` not `snd001.wav`
- ❌ Avoid special characters: `!@#$%^&*()`
- ❌ Avoid Unicode characters

---

## Converting Web Audio Formats

Most audio from the web (YouTube, SoundCloud, etc.) is in **compressed formats** (MP3, M4A, OGG, WebM) and needs conversion.

### Tools Required

**FFmpeg** (cross-platform, command-line tool)

**Installation**:
```bash
# Linux (Ubuntu/Debian)
sudo apt install ffmpeg

# macOS (Homebrew)
brew install ffmpeg

# Windows (Chocolatey)
choco install ffmpeg

# Or download from: https://ffmpeg.org/download.html
```

**Audacity** (optional, GUI tool)
- Download: https://www.audacityteam.org/download/

---

### Converting with FFmpeg (Command Line)

FFmpeg is the fastest and most reliable tool for batch conversion.

#### Basic Conversion

**Convert any audio file to correct format**:
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 output.wav
```

**Parameters explained**:
- `-i input.mp3`: Input file (any format: MP3, M4A, OGG, WebM, FLAC, etc.)
- `-ar 44100`: Audio sample rate = 44.1 kHz
- `-ac 1`: Audio channels = 1 (mono)
- `-sample_fmt s16`: Sample format = 16-bit signed integer (PCM)
- `output.wav`: Output filename

#### Stereo to Mono Conversion

**Keep stereo** (if you want stereo output):
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 2 -sample_fmt s16 output.wav
```

**Mix stereo to mono** (recommended for space saving):
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 output.wav
```

#### Volume Adjustment

**Increase volume by 6 dB** (if source is too quiet):
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -af "volume=6dB" output.wav
```

**Decrease volume by 3 dB** (if source clips/distorts):
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -af "volume=-3dB" output.wav
```

**Normalize audio** (auto-adjust to optimal level):
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -af "loudnorm" output.wav
```

#### Trim/Cut Audio

**Extract first 5 seconds**:
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -t 5 output.wav
```

**Start at 2 seconds, duration 3 seconds**:
```bash
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -ss 2 -t 3 output.wav
```

#### Batch Conversion

**Convert all MP3 files in a directory**:

**Linux/macOS**:
```bash
for file in *.mp3; do
    ffmpeg -i "$file" -ar 44100 -ac 1 -sample_fmt s16 "${file%.mp3}.wav"
done
```

**Windows (PowerShell)**:
```powershell
Get-ChildItem *.mp3 | ForEach-Object {
    $output = $_.BaseName + ".wav"
    ffmpeg -i $_.Name -ar 44100 -ac 1 -sample_fmt s16 $output
}
```

---

### Converting with Audacity (GUI)

For users who prefer a graphical interface.

#### Step-by-Step Conversion

1. **Open Audacity**

2. **Import Audio File**
   - File → Open → Select your audio file
   - Supports: MP3, M4A, OGG, FLAC, WAV, and more

3. **Convert to Mono** (if stereo)
   - Click track dropdown menu (top-left of waveform)
   - Select "Split Stereo to Mono"
   - Delete one channel, or
   - Tracks → Mix → Mix Stereo Down to Mono

4. **Resample to 44.1 kHz**
   - Click track dropdown menu
   - Select "Resample..."
   - Enter: `44100` Hz
   - Click "OK"

5. **Adjust Volume** (optional)
   - Select all: Ctrl+A (Cmd+A on Mac)
   - Effect → Volume and Compression → Amplify...
   - Use "Normalize" option or adjust dB manually

6. **Trim Audio** (optional)
   - Select region with mouse
   - Edit → Remove Audio → Trim Audio

7. **Export as WAV**
   - File → Export → Export Audio...
   - Format: "WAV (Microsoft)"
   - Encoding: "Signed 16-bit PCM"
   - Sample Rate: 44100 Hz
   - Channels: Mono (1) or Stereo (2)
   - Click "Export"

---

### Common Source Formats

#### YouTube Audio

**Download with yt-dlp**:
```bash
# Install yt-dlp
pip install yt-dlp

# Download best audio, convert directly to WAV
yt-dlp -x --audio-format wav --audio-quality 0 \
    --postprocessor-args "-ar 44100 -ac 1 -sample_fmt s16" \
    "https://www.youtube.com/watch?v=VIDEO_ID"
```

#### Web Audio (MP3, M4A, OGG)

Most web audio just needs format conversion:
```bash
ffmpeg -i downloaded_audio.m4a -ar 44100 -ac 1 -sample_fmt s16 output.wav
```

#### Game Sound Effects

Many game sound effect sites (Freesound.org, Zapsplat.com) offer WAV downloads:
1. Download WAV file
2. Check format with: `ffmpeg -i sound.wav`
3. If not 44.1kHz/16-bit, convert:
```bash
ffmpeg -i sound.wav -ar 44100 -ac 1 -sample_fmt s16 sound_converted.wav
```

#### Text-to-Speech (TTS)

**Generate with espeak** (Linux):
```bash
espeak -v en -s 150 "Hello, OpenFront player" --stdout | \
    ffmpeg -i - -ar 44100 -ac 1 -sample_fmt s16 hello.wav
```

**Or use online TTS services**, then convert the output.

---

## Testing Your Setup

### 1. Verify SD Card Files

**Check file format with FFmpeg**:
```bash
ffmpeg -i track1.wav
```

**Look for**:
```
Stream #0:0: Audio: pcm_s16le, 44100 Hz, mono, s16, 1411 kb/s
                    ^^^^^^^^   ^^^^^      ^^^^  ^^^
                    16-bit PCM 44.1kHz    mono  correct!
```

**Or use `file` command**:
```bash
file track1.wav
# Should show: RIFF (little-endian) data, WAVE audio, Microsoft PCM, 16 bit, mono 44100 Hz
```

### 2. Test on Audio Module

**Insert SD card** into module (ensure power is off first).

**Connect to serial console**:
```bash
pio device monitor --raw
```

**Test commands**:
```
# List files
ls

# Play specific file
play track1.wav

# Play quick-play slots
1
2
hello
ping

# Check mixer status
status
```

**Expected output**:
```
I (12345) CONSOLE: Playing: /sdcard/track1.wav
I (12350) WAV_UTILS: Parsed WAV: 44100 Hz, 1 ch, 16 bits, 88200 bytes
I (12360) MIXER: Created file source 0: /sdcard/track1.wav vol=100%
I (12370) DECODER: Decoder task started for source 0
```

### 3. Verify Audio Output

- Connect speakers/headphones to audio output jack
- Play a test file: `play track1.wav`
- Listen for clear audio without distortion
- If audio is distorted, reduce source volume and reconvert

---

## Troubleshooting

### SD Card Not Detected

**Symptoms**:
```
E (1234) SDCARD: SD card init failed
```

**Solutions**:
1. Check SD card is fully inserted
2. Power cycle the module
3. Try different SD card (some cards incompatible)
4. Check SD card formatted as FAT32
5. Try re-formatting SD card

### File Not Found

**Symptoms**:
```
E (5678) PLAYER: Failed to open file: /sdcard/track1.wav
```

**Solutions**:
1. Check filename exactly matches (case-sensitive on Linux)
2. Ensure file is in root directory, not subfolder
3. Check filename uses 8.3 format or valid long filename
4. Try renaming file to all lowercase

### Audio Quality Issues

**Distorted/Clipping Audio**:
```bash
# Reduce volume by 6dB
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -af "volume=-6dB" output.wav
```

**Audio Too Quiet**:
```bash
# Normalize audio
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -af "loudnorm" output.wav
```

**Crackling/Popping**:
- Ensure sample rate is exactly 44100 Hz
- Check SD card speed (use Class 10)
- Try reducing audio volume at source

### Wrong Audio Format

**Symptoms**:
```
E (9012) WAV_UTILS: Invalid WAV format
E (9012) PLAYER: Not a valid WAV file
```

**Solution** - Verify format:
```bash
ffmpeg -i problem_file.wav

# Should show: pcm_s16le, 44100 Hz, mono
# If not, reconvert:
ffmpeg -i problem_file.wav -ar 44100 -ac 1 -sample_fmt s16 problem_file_fixed.wav
```

### File Too Large / Out of Memory

**Symptoms**:
```
E (3456) MIXER: Failed to allocate buffer for source
```

**Solutions**:
1. Reduce audio duration (trim with FFmpeg)
2. Convert stereo to mono (halves size)
3. Split long files into multiple shorter files
4. Recommended max: 30 seconds mono, 15 seconds stereo

---

## Quick Reference Card

### Essential FFmpeg Commands

```bash
# Basic conversion (mono)
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 output.wav

# Stereo conversion
ffmpeg -i input.mp3 -ar 44100 -ac 2 -sample_fmt s16 output.wav

# Normalize volume
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -af "loudnorm" output.wav

# Trim to 5 seconds
ffmpeg -i input.mp3 -ar 44100 -ac 1 -sample_fmt s16 -t 5 output.wav

# Check format
ffmpeg -i file.wav
```

### Console Commands

```bash
ls              # List files on SD card
play <file>     # Play specific file
1, 2            # Quick play track1.wav, track2.wav
hello, ping     # Quick play hello.wav, ping.wav
status          # Show mixer status
playing         # Show currently playing sources
stop            # Stop all playback
vol <0-100>     # Set master volume
```

---

## Additional Resources

- **FFmpeg Documentation**: https://ffmpeg.org/documentation.html
- **Audacity Manual**: https://manual.audacityteam.org/
- **Free Sound Effects**: https://freesound.org/
- **WAV Format Specification**: http://soundfile.sapp.org/doc/WaveFormat/

---

**Last Updated**: January 4, 2026  
**Firmware Version**: Compatible with ots-fw-audiomodule v1.0+
