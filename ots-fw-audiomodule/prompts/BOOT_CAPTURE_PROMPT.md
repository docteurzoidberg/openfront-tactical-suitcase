# Boot Capture Tool - AI Assistant Prompt

## Tool Purpose

The `capture_boot.py` script captures complete ESP32-A1S boot sequences by monitoring serial output while resetting the device. This is essential for debugging boot issues, verifying initialization sequences, and capturing early logs that would otherwise be missed.

## When to Use This Tool

Use `capture_boot.py` when:
- Debugging boot crashes or hangs
- Verifying hardware initialization completed successfully
- Investigating "missed" early boot logs
- Comparing boot behavior before/after changes
- Need complete logs from bootloader onwards
- Testing firmware resilience (codec failures, SD card issues, etc.)

Do NOT use when:
- Just monitoring runtime logs (use `pio device monitor`)
- Interactive debugging (use monitor with manual input)
- Need WebSocket testing (use ots_device_tool.py from ots-fw-main)

## Quick Reference Commands

```bash
# Standard boot capture (auto-detect port, 10s)
./tools/capture_boot.py

# Save to file for analysis
./tools/capture_boot.py --output boot_logs.txt

# Longer capture (include runtime logs)
./tools/capture_boot.py --duration 20

# Specific port
./tools/capture_boot.py --port /dev/ttyUSB0

# Monitor without reset
./tools/capture_boot.py --no-reset --duration 30
```

## Expected Boot Sequence

A successful boot should show:

1. **ESP-IDF Bootloader** (ets Jul 29 2019...)
   - Chip info, flash mode, partition table
   - ~0-2 seconds

2. **Application Start** (I (xxx) app_init...)
   - Project name, version, compile time
   - ~2-3 seconds

3. **SD Card Init** (I (xxx) sdcard...)
   - Either mounts successfully OR fails gracefully
   - Should see: "SD card mount failed, continuing without SD"
   - ~3-4 seconds

4. **Hardware Init** (I (xxx) MAIN: Initializing hardware...)
   - GPIO, I2C, I2S initialization
   - AC101 codec attempt (may fail gracefully)
   - ~4-5 seconds

5. **Audio Mixer Init** (I (xxx) MIXER: Initializing...)
   - Mixer task starts
   - Hardware ready state set
   - ~5-6 seconds

6. **CAN Driver Init** (I (xxx) CAN_HANDLER...)
   - CAN initialization
   - Mock mode active (no physical CAN hardware)
   - ~6-7 seconds

7. **Runtime** (I (xxx) CAN_HANDLER: STATUS...)
   - Periodic status updates every 5 seconds
   - System stable and running

## Red Flags in Boot Logs

Watch for these issues:

**Critical (Boot Failure):**
- `Guru Meditation Error` - Crash/panic
- `panic'ed (LoadStoreError)` - Memory access violation
- Boot loop (repeated bootloader messages)
- Hang (no new logs after certain point)

**Warnings (May Be OK):**
- `AC101 codec init failed` - Expected if hardware not present
- `SD card mount failed` - Expected if card not inserted
- `Audio hardware not ready` - Expected without codec
- `CAN_DRV: Physical mode not implemented` - Expected in mock mode
- `gpio_pullup_en(78): GPIO number error` - Known GPIO39 limitation

## Integration Examples

### After Firmware Flash

```bash
cd ots-fw-audiomodule

# Build and flash
pio run -e esp32-a1s-espidf -t upload

# Capture boot immediately
./tools/capture_boot.py --output latest_boot.txt
```

### Compare Before/After Changes

```bash
# Before
git stash
./tools/capture_boot.py --output boot_before.txt

# After
git stash pop
pio run -e esp32-a1s-espidf -t upload
./tools/capture_boot.py --output boot_after.txt

# Compare
diff boot_before.txt boot_after.txt
```

### Verify Bug Fix

```bash
# Reproduce bug (if intermittent, run multiple times)
for i in {1..10}; do
  ./tools/capture_boot.py --output "boot_test_$i.txt"
  grep -q "Guru Meditation" "boot_test_$i.txt" && echo "CRASH in test $i"
  sleep 2
done
```

## Tool Architecture

The script follows the stdlib-only pattern from `ots-fw-main/tools/ots_device_tool.py`:

**Key Components:**
1. **Port Auto-Detection** - Finds ESP32 serial port automatically
2. **Serial Monitor** - Raw termios-based serial reading (no pyserial dependency)
3. **Device Reset** - Uses esptool.py to reset via DTR/RTS
4. **Log Capture** - Buffers and displays/saves serial output

**Why No pyserial?**
- Fewer dependencies (only esptool for reset)
- More control over serial port configuration
- Matches ots-fw-main tooling patterns
- Works identically on all Linux systems

## Common Issues and Solutions

### Issue: "esptool.py not found"

```bash
pip install esptool
```

Or check if it's installed but not in PATH:
```bash
python3 -m esptool --version
```

### Issue: "Permission denied: /dev/ttyUSB0"

```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Issue: "No serial ports found"

Check device connection:
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
dmesg | tail -20  # Look for USB device attach
```

### Issue: Port already in use

Kill other monitors:
```bash
pkill -f "pio.*monitor"
lsof /dev/ttyUSB0  # Find process holding port
```

## Script Modifications

If you need to modify the tool:

**Add new reset method:**
```python
def reset_device_via_rts(port: str) -> bool:
    """Alternative reset using direct RTS toggle."""
    import fcntl
    # ... implementation
```

**Change capture format:**
```python
# In SerialMonitor.monitor(), modify line output:
line = f"[{time.time():.3f}] {line}"  # Add timestamps
```

**Add log filtering:**
```python
# Skip certain log levels
if "DEBUG" in line:
    continue
```

## Testing the Tool

```bash
# Test without device (should fail gracefully)
./tools/capture_boot.py --port /dev/nonexistent

# Test with device but no reset
./tools/capture_boot.py --no-reset --duration 5

# Test file output
./tools/capture_boot.py --output /tmp/test.txt
cat /tmp/test.txt
```

## Related Files

- **Implementation**: `tools/capture_boot.py` (executable Python script)
- **Documentation**: `docs/BOOT_CAPTURE_TOOL.md` (user guide)
- **This file**: `prompts/BOOT_CAPTURE_PROMPT.md` (AI assistant reference)
- **Reference**: `../../ots-fw-main/tools/ots_device_tool.py` (similar pattern)

## When to Recommend This Tool

**User says:** "I can't see the early boot logs"
**Response:** Use `./tools/capture_boot.py` to reset the device while monitoring

**User says:** "Firmware crashes on boot"
**Response:** Capture full boot with `./tools/capture_boot.py --output crash.txt` and check for panic messages

**User says:** "Did my change break boot?"
**Response:** Capture before/after logs and compare

**User says:** "Is hardware initializing correctly?"
**Response:** Boot capture will show all initialization steps and any failures

## Code Snippet for AI Assistants

When suggesting boot capture in responses:

```bash
# Capture boot logs to verify initialization
cd ots-fw-audiomodule
./tools/capture_boot.py --output boot_logs.txt

# Check for errors
grep -E "ERROR|FAIL|Guru" boot_logs.txt
```

Or in test automation:

```python
# Example integration in Python test
import subprocess

def capture_boot() -> str:
    """Capture device boot logs."""
    result = subprocess.run(
        ["./tools/capture_boot.py", "--output", "boot.txt"],
        capture_output=True,
        timeout=15,
        cwd="/path/to/ots-fw-audiomodule"
    )
    with open("boot.txt") as f:
        return f.read()

# Use in tests
logs = capture_boot()
assert "Audio mixer initialized" in logs
assert "Guru Meditation" not in logs
```
