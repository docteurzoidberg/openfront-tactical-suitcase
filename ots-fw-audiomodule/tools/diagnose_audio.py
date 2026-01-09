#!/usr/bin/env python3
"""
Audio Diagnostic Tool for ESP32-A1S AudioKit
Reads ES8388 codec registers to diagnose audio issues
"""

import serial
import time
import sys

def send_command(ser, cmd):
    """Send command and read response"""
    ser.write(f'{cmd}\n'.encode())
    time.sleep(0.2)
    output = ""
    while ser.in_waiting:
        output += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        time.sleep(0.05)
    return output

def main():
    port = '/dev/ttyUSB0'
    print(f"ES8388 Audio Diagnostic Tool")
    print(f"Port: {port}")
    print("="*60)
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(1)
        ser.read(ser.in_waiting)  # Clear buffer
        
        # Get system info
        print("\n📊 System Information:")
        print("-"*60)
        output = send_command(ser, 'sysinfo')
        print(output)
        
        # Get audio status
        print("\n🔊 Audio Mixer Status:")
        print("-"*60)
        output = send_command(ser, 'status')
        print(output)
        
        # Play test tone to check audio path
        print("\n🎵 Testing Audio Path (tone1):")
        print("-"*60)
        output = send_command(ser, 'tone1')
        print(output)
        time.sleep(2)
        
        print("\n✅ Diagnostic complete")
        print("\nISSUES DETECTED:")
        print("  1. Sound only in LEFT speaker (LOUT channel)")
        print("  2. Static noise during playback")
        print("\nLIKELY CAUSES:")
        print("  • I2S stereo channel routing issue")
        print("  • ADC input path not fully disabled (feedback)")
        print("  • Amplifier power or gain settings")
        print("  • ES8388 power management configuration")
        
        print("\nRECOMMENDED FIXES:")
        print("  1. Check DACCONTROL16 register (output mixer routing)")
        print("  2. Verify ADC is fully powered down (ADCPOWER register)")
        print("  3. Check I2S stereo channel configuration")
        print("  4. Review amplifier enable/mute pins")
        
        ser.close()
        
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
