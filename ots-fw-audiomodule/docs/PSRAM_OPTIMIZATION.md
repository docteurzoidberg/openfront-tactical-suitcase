# PSRAM Optimization - Documentation

**Date:** January 4, 2026  
**Version:** 1.0  
**ESP32-A1S Board:** 8MB PSRAM @ 40MHz

## Overview

This document describes the PSRAM optimization implemented to ensure intelligent use of the ESP32-A1S board's 8MB PSRAM for audio buffer allocation.

## Hardware Context

- **Board:** ESP32-A1S (AC101 Audio)
- **PSRAM:** 8MB SPIRAM @ 40MHz (Quad mode)
- **Configuration:** 
  - `CONFIG_SPIRAM_USE_MALLOC=y` (automatic allocation)
  - `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` (16KB threshold)
  - `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768` (32KB reserved)

## Problem Statement

Before optimization, audio buffers were allocated using standard `xStreamBufferCreate()` and `malloc()`, which may or may not use PSRAM depending on allocation size. This led to:

1. **Unpredictable allocation**: No guarantee buffers were in PSRAM
2. **Internal RAM waste**: Potential 216KB+ of buffers in precious internal RAM
3. **No visibility**: No way to track where buffers were allocated
4. **No control**: Cannot force PSRAM allocation for specific buffers

## Audio Buffer Requirements

The audio mixer manages multiple buffers simultaneously:

| Buffer Type | Size | Count | Total | Critical? |
|-------------|------|-------|-------|-----------|
| Mix buffer | 16KB | 1 | 16KB | Yes - DMA target |
| Stream buffers | 16KB | 1-8 | 16-128KB | No - decoder staging |
| Test tone buffer | ~88KB | 0-1 | 0-88KB | No - test only |
| **Total** | - | - | **32-216KB** | - |

## Optimization Strategy

### 1. Explicit PSRAM Allocation

Changed from implicit allocation to explicit PSRAM requests:

**Before:**
```c
// Let ESP-IDF decide (may use internal RAM)
xStreamBufferCreate(buffer_size, 1);
test_tone_buffer = malloc(buffer_size);
```

**After:**
```c
// Force PSRAM with fallback
uint8_t *storage = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (!storage) {
    storage = malloc(buffer_size);  // Fallback to internal RAM
}
xStreamBufferCreateStatic(buffer_size, 1, storage, &static_stream_buffer);
```

### 2. Allocation Tracking

Added struct fields to track allocation source:

```c
typedef struct {
    // ... existing fields ...
    
    StreamBufferHandle_t buffer;
    bool buffer_in_psram;        // Track if buffer is in PSRAM
    uint8_t *buffer_storage;     // Storage pointer for cleanup
    
    // ... rest of struct ...
} audio_source_t;
```

### 3. Visibility & Monitoring

Enhanced `sysinfo` command to show PSRAM utilization:

```
PSRAM Utilization:
  Usage: 12.5% (1048576 / 8388608 bytes)
  Audio buffers: Mixer + 3 source streams
```

Calculation:
```c
size_t psram_used = esp_psram_get_size() - heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
float psram_usage = (float)psram_used / (float)esp_psram_get_size() * 100.0f;
```

## Implementation Details

### Modified Files

1. **audio_mixer.c** (3 changes):
   - Struct: Added `buffer_in_psram` and `buffer_storage` tracking fields
   - Buffer allocation: Changed to `xStreamBufferCreateStatic()` with PSRAM storage
   - Cleanup: Free buffer_storage on source deletion

2. **audio_test_tone.c** (1 change):
   - Changed `malloc()` to `heap_caps_malloc(MALLOC_CAP_SPIRAM)`
   - Added fallback to internal RAM
   - Added allocation source logging

3. **audio_console.c** (1 change):
   - Enhanced `sysinfo` command with PSRAM utilization stats

### Code Changes

#### audio_mixer.c - Struct Definition

```c
typedef struct {
    bool active;
    audio_source_state_t state;
    char filepath[128];
    uint8_t volume;
    bool loop;
    
    // Stream buffer for PCM data (allocated in PSRAM)
    StreamBufferHandle_t buffer;
    bool buffer_in_psram;        // Track if buffer is in PSRAM
    uint8_t *buffer_storage;     // Storage pointer for cleanup
    
    // Decoder task and params
    TaskHandle_t decoder_task;
    // ... rest of struct
} audio_source_t;
```

#### audio_mixer.c - Buffer Allocation

```c
// Allocate buffer storage in PSRAM with fallback
size_t buffer_size = 16 * 1024;
uint8_t *buffer_storage = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
bool in_psram = (buffer_storage != NULL);

if (!buffer_storage) {
    ESP_LOGW(TAG, "PSRAM allocation failed, using internal RAM");
    buffer_storage = malloc(buffer_size);
    if (!buffer_storage) {
        ESP_LOGE(TAG, "Failed to allocate buffer storage");
        return -1;
    }
}

// Create static stream buffer with our storage
StaticStreamBuffer_t static_stream_buffer;
src->buffer = xStreamBufferCreateStatic(buffer_size, 1, 
                                        buffer_storage, &static_stream_buffer);
src->buffer_storage = buffer_storage;
src->buffer_in_psram = in_psram;

ESP_LOGI(TAG, "Allocated %zu bytes buffer from %s for source %d",
         buffer_size, in_psram ? "PSRAM" : "internal RAM", idx);
```

#### audio_mixer.c - Cleanup

```c
void audio_mixer_stop_source(int idx) {
    audio_source_t *src = &sources[idx];
    
    // ... existing cleanup ...
    
    // Free buffer storage if allocated
    if (src->buffer_storage) {
        free(src->buffer_storage);
        src->buffer_storage = NULL;
    }
    
    src->buffer_in_psram = false;
}
```

#### audio_test_tone.c - Test Tone Buffer

```c
// Try to allocate from PSRAM first
test_tone_buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

if (test_tone_buffer) {
    ESP_LOGI(TAG, "Test tone buffer allocated from PSRAM (%zu bytes)", buffer_size);
} else {
    // Fallback to internal RAM
    ESP_LOGW(TAG, "PSRAM allocation failed, using internal RAM for test tone");
    test_tone_buffer = malloc(buffer_size);
    if (!test_tone_buffer) {
        ESP_LOGE(TAG, "Failed to allocate test tone buffer");
        return ESP_ERR_NO_MEM;
    }
}
```

#### audio_console.c - PSRAM Monitoring

```c
static int cmd_sysinfo(int argc, char **argv) {
    // ... existing system info ...
    
    // PSRAM utilization
    if (esp_psram_get_size() > 0) {
        size_t psram_total = esp_psram_get_size();
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t psram_used = psram_total - psram_free;
        float psram_usage = (float)psram_used / (float)psram_total * 100.0f;
        
        printf("\nPSRAM Utilization:\n");
        printf("  Usage: %.1f%% (%lu / %lu bytes)\n", 
               psram_usage, (unsigned long)psram_used, (unsigned long)psram_total);
        
        int active_count = 0;
        for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
            if (audio_mixer_is_source_active(i)) active_count++;
        }
        printf("  Audio buffers: Mixer + %d source streams\n", active_count);
    }
    
    return 0;
}
```

## Testing & Validation

### Build Validation

```bash
$ pio run -e esp32-a1s-espidf
...
RAM:   [          ]   4.4% (used 14344 bytes from 327680 bytes)
Flash: [===       ]  27.7% (used 1163519 bytes from 4194304 bytes)
====================================== [SUCCESS] Took 5.58 seconds ======================================
```

**Result:** Firmware builds successfully with PSRAM optimizations.

### Runtime Testing Commands

```bash
# Check PSRAM availability
> sysinfo
PSRAM Utilization:
  Usage: X.X% (used / total bytes)
  Audio buffers: Mixer + N source streams

# Play multiple sources (test buffer allocation)
> play /sdcard/test1.wav
> play /sdcard/test2.wav
> play /sdcard/test3.wav

# Check PSRAM usage increase
> sysinfo
PSRAM Utilization:
  Usage: Y.Y% (higher than before)
  Audio buffers: Mixer + 3 source streams

# Generate test tone (88KB buffer)
> tone1 440 3
[audio_test_tone] Test tone buffer allocated from PSRAM (88200 bytes)

# Stop all and verify PSRAM freed
> stop
> sysinfo
PSRAM Utilization:
  Usage: Z.Z% (back to baseline)
  Audio buffers: Mixer + 0 source streams
```

### Expected Behavior

1. **Initial state**: Low PSRAM usage (~1-2%)
2. **After loading 3 WAV files**: ~48KB allocated (3 × 16KB buffers)
3. **After tone generation**: +88KB for test tone
4. **After stop**: PSRAM freed, back to baseline

## Benefits

### 1. Predictable Memory Usage
- All large audio buffers guaranteed in PSRAM (if available)
- Internal RAM preserved for critical tasks
- Fallback ensures operation even if PSRAM exhausted

### 2. Visibility
- `sysinfo` shows real-time PSRAM utilization
- Logs indicate allocation source per buffer
- Can track memory issues during development

### 3. Scalability
- Can support up to 8 simultaneous audio sources
- Total buffer usage: 16KB + (8 × 16KB) = 144KB in PSRAM
- Leaves 8MB - 144KB = 7.9MB PSRAM free for other uses

### 4. Robustness
- Fallback to internal RAM prevents allocation failures
- Proper cleanup prevents memory leaks
- Tracking enables debugging

## Performance Considerations

### PSRAM vs Internal RAM

| Aspect | PSRAM (40MHz) | Internal RAM |
|--------|---------------|--------------|
| Speed | ~10MB/s | ~40MB/s |
| Latency | Higher | Lower |
| Size | 8MB | 320KB |
| Use Case | Large buffers | DMA, critical ops |

**Decision:** Audio stream buffers are perfect for PSRAM because:
- Not DMA targets (copied to mix buffer for DMA)
- Sequential access pattern (not random)
- Large size (16KB each)
- Non-critical timing (decoder staging)

The mix buffer remains in internal RAM (or cache-aligned) for optimal DMA performance.

## Monitoring & Debugging

### Console Commands

```bash
# Check PSRAM status
> sysinfo

# Check active audio sources
> status

# Verify buffer allocation logs (requires serial monitoring)
[audio_mixer] Allocated 16384 bytes buffer from PSRAM for source 0
[audio_test_tone] Test tone buffer allocated from PSRAM (88200 bytes)
```

### Serial Logs

Enable verbose logging to see allocation details:
```c
// In audio_mixer.c
ESP_LOGI(TAG, "Allocated %zu bytes buffer from %s for source %d",
         buffer_size, in_psram ? "PSRAM" : "internal RAM", idx);

// In audio_test_tone.c
ESP_LOGI(TAG, "Test tone buffer allocated from PSRAM (%zu bytes)", buffer_size);
ESP_LOGW(TAG, "PSRAM allocation failed, using internal RAM for test tone");
```

## Future Enhancements

### 1. Dynamic Buffer Sizing
- Adjust buffer sizes based on bitrate/sample rate
- Higher quality = larger buffers (all in PSRAM)

### 2. Buffer Pool
- Pre-allocate buffer pool in PSRAM on startup
- Reuse buffers for new sources (avoid alloc/free cycles)

### 3. Heap Fragmentation Tracking
- Monitor PSRAM fragmentation over time
- Implement periodic defragmentation if needed

### 4. PSRAM Caching
- Enable PSRAM cache for frequently accessed data
- Trade-off: uses internal RAM for cache

## Troubleshooting

### Issue: PSRAM Allocation Fails

**Symptoms:**
```
[audio_mixer] PSRAM allocation failed, using internal RAM
```

**Causes:**
1. PSRAM exhausted (8MB used)
2. PSRAM disabled in sdkconfig
3. Heap fragmentation

**Solutions:**
1. Check `sysinfo` for PSRAM usage
2. Verify `CONFIG_SPIRAM=y` in sdkconfig
3. Stop unused audio sources to free buffers
4. Reboot device to defragment

### Issue: Unexpected Internal RAM Usage

**Symptoms:**
```
[audio_mixer] Allocated 16384 bytes buffer from internal RAM for source 0
```

**Debug:**
1. Check PSRAM availability: `sysinfo`
2. Verify PSRAM size: `heap_caps_get_total_size(MALLOC_CAP_SPIRAM)`
3. Check fragmentation: `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)`

### Issue: Memory Leaks

**Symptoms:**
- PSRAM usage increases over time
- Never returns to baseline after `stop`

**Debug:**
1. Check cleanup code in `audio_mixer_stop_source()`
2. Verify `buffer_storage` is freed
3. Use ESP-IDF heap tracing:
   ```c
   heap_trace_start(HEAP_TRACE_LEAKS);
   // ... run test ...
   heap_trace_stop();
   heap_trace_dump();
   ```

## References

- **ESP-IDF PSRAM Guide:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/external-ram.html
- **Heap Memory API:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html
- **Stream Buffers:** https://www.freertos.org/RTOS-stream-buffer-example.html

## Changelog

### v1.0 - January 4, 2026
- Initial implementation
- Explicit PSRAM allocation for all audio buffers
- Added allocation tracking (buffer_in_psram, buffer_storage)
- Enhanced sysinfo with PSRAM utilization
- Documented optimization strategy and testing

---

**Maintainers:** Keep this document updated when modifying buffer allocation strategy or adding new audio features that use large buffers.
