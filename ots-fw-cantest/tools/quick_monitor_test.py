#!/usr/bin/env python3
"""Quick test: Put cantest in monitor mode and watch for CAN traffic"""

import serial
import time
import sys

PORT = "/dev/ttyACM0"

print("=== Quick Monitor Test ===")
print(f"Port: {PORT}")
print()

try:
    ser = serial.Serial(PORT, 115200, timeout=0.1)
    time.sleep(0.5)
    
    # Flush input
    ser.reset_input_buffer()
    
    # Enter monitor mode
    print("Entering monitor mode...")
    ser.write(b'm\n')
    time.sleep(1)
    
    # Read initial banner
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"  {line}")
    
    print()
    print("Monitoring CAN bus for 10 seconds...")
    print("(Any messages received will appear below)")
    print("-" * 70)
    
    start = time.time()
    msg_count = 0
    
    while time.time() - start < 10:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
                if '0x' in line and ('RX:' in line or 'TX:' in line):
                    msg_count += 1
        time.sleep(0.01)
    
    print("-" * 70)
    print(f"Test complete. Messages seen: {msg_count}")
    
    if msg_count == 0:
        print()
        print("No CAN messages received.")
        print("This means either:")
        print("  1. No other device is sending on the bus")
        print("  2. CAN wiring issue (CANH/CANL not connected)")
        print("  3. No termination resistors")
    
    # Exit monitor mode
    ser.write(b'q\n')
    ser.close()

except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
