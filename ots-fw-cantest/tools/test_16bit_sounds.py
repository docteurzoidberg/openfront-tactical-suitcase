#!/usr/bin/env python3
"""Test 16-bit sound indices (embedded tones 10000-10002 and quack 10100)"""

import serial
import time
import sys

# Sound indices
SOUNDS = {
    10000: ("Tone 1 - 1s 440Hz", 2),
    10001: ("Tone 2 - 2s 880Hz", 3),
    10002: ("Tone 3 - 5s 220Hz", 6),
    10100: ("Quack (embedded WAV)", 2),
}

def send_raw_can_via_cantest(ser, can_id, data):
    """Send raw CAN frame through cantest (not implemented in current firmware)"""
    # Cantest doesn't support raw CAN frame injection
    # We need to use Python-CAN library directly or update firmware
    pass

def send_play_sound_16bit_direct():
    """Send PLAY_SOUND with 16-bit indices using python-can"""
    try:
        import can
    except ImportError:
        print("ERROR: python-can not installed")
        print("Install with: pip install python-can")
        sys.exit(1)
    
    # Try to find CAN interface
    # This won't work without a CAN interface on the host
    print("ERROR: Direct CAN not available - host doesn't have CAN interface")
    print("The ESP32s have the CAN bus between them only")
    sys.exit(1)

def main():
    print("="*60)
    print("Testing 16-bit Sound Indices via CAN")
    print("="*60)
    print("\nPROBLEM: cantest 'p' command only supports 8-bit indices (0-255)")
    print("Embedded tones are at indices 10000-10002 (16-bit)")
    print("\nSOLUTIONS:")
    print("1. Update cantest firmware to support 16-bit indices")
    print("2. Use serial console on audio module directly")
    print("3. Use python-can with CAN interface hardware")
    print("\nRECOMMENDATION: Test via audio module serial console")
    print("="*60)
    
    # Show the serial console commands
    print("\nTesting via AUDIO MODULE serial console (/dev/ttyUSB0):")
    print("-" * 60)
    for idx, (desc, wait) in sorted(SOUNDS.items()):
        print(f"\nSound {idx}: {desc}")
        print(f"  Command: tone{idx-9999}")  # tone1, tone2, tone3
        if idx == 10100:
            print(f"  Command: test_quack")
    
    print("\n" + "="*60)
    print("Run these commands in the audio module serial console")
    print("to test embedded sounds directly (no CAN needed)")
    print("="*60)

if __name__ == '__main__':
    main()
