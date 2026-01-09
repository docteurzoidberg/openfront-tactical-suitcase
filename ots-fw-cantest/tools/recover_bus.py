#!/usr/bin/env python3
"""
Quick script to recover both CAN devices from BUS_OFF state.
Sends recovery commands to both devices.
"""

import sys
import time
import serial
from pathlib import Path

def recover_device(port: str, name: str):
    """Recover a single device from BUS_OFF."""
    print(f"\n=== Recovering {name} ({port}) ===")
    
    try:
        ser = serial.Serial(port, 115200, timeout=2)
        time.sleep(0.5)  # Let device settle
        
        # Clear any buffered output
        ser.read(ser.in_waiting)
        
        # Check current status first
        print("Checking current status...")
        ser.write(b't\n')
        time.sleep(0.5)
        output = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        
        if 'State: BUS_OFF' in output:
            print(f"  Device is in BUS_OFF state - initiating recovery...")
            
            # In cantest, 'r' command calls can_driver_recover() + can_driver_start()
            # But we need to add this command to the CLI first
            # For now, just report the status
            print(f"  ✗ Manual recovery needed - 'r' command not yet implemented in CLI")
            print(f"     Will be available after next firmware update")
            return False
            
        elif 'State: RUNNING' in output:
            print(f"  ✓ Device already in RUNNING state!")
            return True
        else:
            print(f"  ? Unknown state - check manually")
            print(output)
            return False
            
    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    finally:
        if 'ser' in locals():
            ser.close()

if __name__ == '__main__':
    s3_port = '/dev/ttyACM1'
    a1s_port = '/dev/ttyUSB1'
    
    print("╔══════════════════════════════════════════════════╗")
    print("║         CAN Bus Recovery Tool                    ║")
    print("╚══════════════════════════════════════════════════╝")
    print()
    print("NOTE: This tool checks device status.")
    print("Recovery CLI command needs to be added to firmware.")
    
    # Check both devices
    s3_ok = recover_device(s3_port, "ESP32-S3")
    a1s_ok = recover_device(a1s_port, "ESP32-A1S")
    
    print("\n=== Status Summary ===")
    print(f"ESP32-S3: {'✓ RUNNING' if s3_ok else '✗ BUS_OFF or ERROR'}")
    print(f"ESP32-A1S: {'✓ RUNNING' if a1s_ok else '✗ BUS_OFF or ERROR'}")
    
    if not (s3_ok and a1s_ok):
        print("\n⚠ Next step: Add 'r' (recovery) command to cantest CLI")
        print("   Command should call: can_driver_recover() then can_driver_start()")
        sys.exit(1)
    else:
        print("\n✓ Both devices ready! Run test_two_device.py")
        sys.exit(0)
