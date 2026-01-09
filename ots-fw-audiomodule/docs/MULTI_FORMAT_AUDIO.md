# Multi-Format Audio Support

**Version**: 1.0  
**Date**: January 8, 2026  
**Status**: Implemented

## Overview

The audio module firmware now supports WAV files with different bit depths and sample rates. The system automatically converts all audio to a fixed output format (16-bit 44.1kHz stereo) for playback through the ES8388 codec.

## Supported Input Formats

### Bit Depth
- **8-bit PCM** (unsigned, 0-255)
- **16-bit PCM** (signed, -32768 to 32767)

### Sample Rates
- **8 kHz** - Voice quality
- **11.025 kHz** - Low quality music
- **16 kHz** - Wideband voice
- **22.05 kHz** - Standard quality music
- **32 kHz** - Broadcast quality
- **44.1 kHz** - CD quality (no conversion needed)
- **48 kHz** - Professional audio
- Any other standard rate

### Channels
- **Mono** (1 channel)
- **Stereo** (2 channels)

## Output Format (Fixed)

All audio is converted to:
- **Bit Depth**: 16-bit signed PCM
- **Sample Rate**: 44.1 kHz
- **Channels**: Stereo (mono is duplicated to both channels)
- **Codec**: ES8388 I2S output

## Conversion Process

### 1. Bit Depth Conversion (8-bit → 16-bit)

```c
// Formula: (sample << 8) - 32768
// Converts unsigned 8-bit (0-255) to signed 16-bit (-32768 to 32767)
out_16bit[i] = ((int16_t)in_8bit[i] << 8) - 32768;
```

**Example**:
- 8-bit input: 0 (silence) → 16-bit output: -32768
- 8-bit input: 128 (mid-level) → 16-bit output: 0
- 8-bit input: 255 (max) → 16-bit output: 32512

### 2. Sample Rate Conversion (Any Rate → 44.1kHz)

Uses **linear interpolation** for resampling:

```c
// Calculate ratio
float ratio = (float)in_rate / 44100.0;

// For each output sample
float in_pos = out_sample_idx * ratio;
int in_idx1 = (int)in_pos;
int in_idx2 = in_idx1 + 1;
float frac = in_pos - in_idx1;

// Interpolate
out_sample = in_sample1 + (in_sample2 - in_sample1) * frac;
```

**Example** (22.05 kHz → 44.1 kHz):
- Ratio: 22050 / 44100 = 0.5
- Output samples are interpolated between every 2 input samples
- Effectively doubles the sample rate

## Architecture

### Decoder Task Flow

```
┌─────────────┐
│  WAV File   │
│  SD Card /  │
│   Memory    │
└──────┬──────┘
       │
       ▼
┌─────────────────────────┐
│  Parse WAV Header       │
│  - Detect bit depth     │
│  - Detect sample rate   │
│  - Detect channels      │
└──────┬──────────────────┘
       │
       ▼
┌─────────────────────────┐
│  Read Chunk (512 samples)│
└──────┬──────────────────┘
       │
       ▼
    ┌──────────────────┐
    │ Need Conversion? │
    └──────┬───────────┘
           │
      ┌────┴────┐
      │         │
   YES│         │NO
      ▼         ▼
┌─────────┐  ┌──────────┐
│ Convert │  │  Direct  │
│ 8→16bit │  │   Pass   │
└────┬────┘  └────┬─────┘
     │            │
     └─────┬──────┘
           │
           ▼
    ┌──────────────────┐
    │ Need Resampling? │
    └──────┬───────────┘
           │
      ┌────┴────┐
      │         │
   YES│         │NO
      ▼         ▼
┌─────────┐  ┌──────────┐
│Resample │  │  Direct  │
│to 44.1kHz│  │   Pass   │
└────┬────┘  └────┬─────┘
     │            │
     └─────┬──────┘
           │
           ▼
┌─────────────────────────┐
│  Stream Buffer (Mixer)  │
│  (Always 16-bit 44.1kHz)│
└─────────────────────────┘
```

### Memory Usage

Each decoder task uses:
- **Read buffer**: 1024 bytes (512 samples × 2 bytes)
- **Convert buffer**: 2048 bytes (1024 samples × 2 bytes)
- **Total per decoder**: ~3KB RAM

With 4 simultaneous sources: **~12KB RAM overhead**

### CPU Usage

Estimated CPU overhead per source:
- **8-bit conversion**: ~2-5% (simple shift operation)
- **Resampling**: ~10-20% (linear interpolation)
- **Total**: ~15-25% per source with full conversion

For 4 sources with mixed formats: **40-80% CPU usage estimate**

## Performance Characteristics

### Quality Trade-offs

| Input Format | Output Quality | Storage Efficiency | CPU Usage |
|--------------|----------------|-------------------|-----------|
| 16-bit 44.1kHz | Excellent (no loss) | Low | Minimal |
| 16-bit 22.05kHz | Good (resampled) | 2× better | Medium |
| 8-bit 44.1kHz | Fair (upsampled) | 2× better | Low |
| 8-bit 22.05kHz | Fair (both) | 4× better | Medium |
| 8-bit 8kHz | Poor (voice only) | 11× better | High |

### Storage Capacity Examples

With **2.75MB** available flash space:

| Format | Duration | Use Case |
|--------|----------|----------|
| 16-bit 44.1kHz stereo | 16.3 seconds | High-quality music/effects |
| 16-bit 22.05kHz mono | 65.4 seconds | Standard quality sounds |
| 8-bit 22.05kHz mono | 130.8 seconds | Game sounds/effects |
| 8-bit 8kHz mono | 360 seconds (6 min) | Voice/alerts |

## Usage

### Embedded Sounds

For embedded samples in firmware:
- Use **16-bit 44.1kHz** for high-quality sounds (no conversion overhead)
- Use **8-bit 22.05kHz** for space-constrained scenarios
- Use **8-bit 8kHz** for long voice clips or alerts

No code changes needed - format is detected automatically from WAV header.

### SD Card Sounds

Users can provide any standard PCM WAV file:
1. Format: 8 or 16-bit PCM, any sample rate, mono or stereo
2. Filename: `/sdcard/sounds/XXXX.wav` (0-9999)
3. Playback: Automatic format detection and conversion

**Example**:
```bash
# User replaces SD card sound with custom WAV
cp my_custom_sound.wav /sdcard/sounds/0042.wav

# Firmware will automatically:
# 1. Detect format (e.g., 8-bit 22.05kHz mono)
# 2. Convert to 16-bit
# 3. Resample to 44.1kHz
# 4. Play through codec
```

## Limitations

1. **PCM Only**: Only uncompressed PCM WAV files supported (no MP3, OGG, etc.)
2. **Fixed Output**: Output is always 44.1kHz (cannot change I2S/codec rate)
3. **Conversion Quality**: Uses simple linear interpolation (not professional-grade)
4. **CPU Overhead**: Multiple conversions running simultaneously may affect performance
5. **Buffer Size**: Very large sample rate differences may cause buffer overflow

## Testing

### Test Files

Create test WAV files for validation:

```bash
# 8-bit 8kHz mono (voice quality)
ffmpeg -i input.wav -ar 8000 -ac 1 -sample_fmt u8 test_8bit_8khz_mono.wav

# 8-bit 22.05kHz mono (game sounds)
ffmpeg -i input.wav -ar 22050 -ac 1 -sample_fmt u8 test_8bit_22khz_mono.wav

# 16-bit 22.05kHz mono (standard quality)
ffmpeg -i input.wav -ar 22050 -ac 1 -sample_fmt s16 test_16bit_22khz_mono.wav

# 16-bit 44.1kHz stereo (CD quality, no conversion)
ffmpeg -i input.wav -ar 44100 -ac 2 -sample_fmt s16 test_16bit_44khz_stereo.wav
```

### Verification

Check logs during playback:

```
I (12345) DECODER: Source 0: 22050Hz 1ch 16bit
I (12346) DECODER: Source 0: Resampling from 22050Hz to 44100Hz
I (12450) DECODER: Source 1: 8000Hz 1ch 8bit
I (12451) DECODER: Source 1: 8-bit to 16-bit conversion enabled
I (12452) DECODER: Source 1: Resampling from 8000Hz to 44100Hz
```

## Implementation Files

- **`src/wav_utils.h`**: Conversion function declarations
- **`src/wav_utils.c`**: Conversion function implementations
  - `wav_convert_8bit_to_16bit()`: Bit depth conversion
  - `wav_resample_linear()`: Sample rate conversion
- **`src/audio_decoder.c`**: Decoder task with format detection and conversion
- **`src/audio_decoder.h`**: Decoder parameters and interface

## Future Enhancements

### Short Term
- [ ] Add cubic interpolation for better resampling quality
- [ ] Optimize conversion routines with SIMD instructions
- [ ] Add format statistics (conversion time, quality metrics)

### Medium Term
- [ ] Support for 24-bit PCM
- [ ] Support for 32-bit float PCM
- [ ] Dynamic sample rate switching (configure codec on-the-fly)

### Long Term
- [ ] Hardware resampling using ESP32 I2S peripheral
- [ ] Compressed audio formats (MP3, OGG, AAC)
- [ ] Real-time pitch shifting and time stretching

## Troubleshooting

### Audio Sounds Wrong
- **Symptom**: Distorted, wrong pitch, or garbled audio
- **Cause**: Incorrect WAV header or unsupported format
- **Solution**: Verify WAV file with `ffmpeg -i file.wav` or `soxi file.wav`

### High CPU Usage
- **Symptom**: System sluggish, delayed responses
- **Cause**: Too many simultaneous conversions
- **Solution**: Use native 44.1kHz files for frequently played sounds

### Clicks/Pops in Audio
- **Symptom**: Audible artifacts during playback
- **Cause**: Buffer underflow due to slow conversion
- **Solution**: Reduce number of simultaneous sources or use simpler formats

### SD Card Sounds Not Playing
- **Symptom**: "Failed to open" or "Invalid WAV file" errors
- **Cause**: Incompatible file format or missing SD card
- **Solution**: Check SD card mounted, verify WAV format with tools

## References

- **CAN Bus Specification**: `/prompts/CANBUS_MESSAGE_SPEC.md`
- **Audio Module Firmware**: `/ots-fw-audiomodule/README.md`
- **WAV File Format**: RIFF WAVE specification
- **ESP32 I2S**: ESP-IDF I2S driver documentation
- **Linear Interpolation**: Basic digital signal processing technique

---

**Status**: ✅ Implemented and compiled  
**Firmware Size**: 1,249,455 bytes (29.8% of 4MB flash)  
**RAM Overhead**: ~12KB for 4 simultaneous sources  
**Tested**: Compilation successful, runtime testing pending
