#!/usr/bin/env python3
"""
Simple test - just watch if main controller sends MODULE_QUERY on boot
No mode switching needed - just monitor the CAN bus
"""

import serial
import time
import sys

def main():
    print("=== Simple Discovery Test ===")
    print("Goal: Verify main controller sends MODULE_QUERY (0x411) on boot")
    print()
    
    # Connect to cantest (just monitor, no mode switching)
    print("Connecting to /dev/ttyUSB0 (cantest monitor)...")
    try:
        ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
        time.sleep(0.5)
        print("✓ Connected")
    except Exception as e:
        print(f"✗ Failed: {e}")
        return 1
    
    print()
    print("Watching CAN bus for MODULE_QUERY (0x411)...")
    print("Press Ctrl+C when done")
    print("-" * 60)
    
    try:
        start_time = time.time()
        seen_query = False
        
        while time.time() - start_time < 30:  # 30 second timeout
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        # Print all CAN traffic
                        if '0x' in line or 'CAN' in line or 'TX:' in line or 'RX:' in line:
                            print(line[:120])
                        
                        # Check for MODULE_QUERY
                        if '0x411' in line or 'MODULE_QUERY' in line:
                            print("\n✓✓✓ MODULE_QUERY DETECTED! ✓✓✓\n")
                            seen_query = True
                except:
                    pass
            
            time.sleep(0.01)
        
        print("-" * 60)
        if seen_query:
            print("\n✓ SUCCESS: Main controller sent MODULE_QUERY")
            return 0
        else:
            print("\n✗ FAILED: No MODULE_QUERY seen in 30 seconds")
            return 1
            
    except KeyboardInterrupt:
        print("\n\nStopped by user")
        return 0
    finally:
        ser.close()

if __name__ == '__main__':
    sys.exit(main())
