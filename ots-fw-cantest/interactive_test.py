#!/usr/bin/env python3
import serial
import time
import sys
import select

port = "/dev/ttyACM0"
baudrate = 115200

print(f"Opening {port}...")
ser = serial.Serial(port, baudrate, timeout=0.1)
time.sleep(0.5)

print("Connected! Type commands (Ctrl+C to exit):")
print("=" * 50)

try:
    while True:
        # Check for data from board
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            print(data.decode('utf-8', errors='ignore'), end='')
            sys.stdout.flush()
        
        # Check for keyboard input (non-blocking)
        if select.select([sys.stdin], [], [], 0)[0]:
            cmd = sys.stdin.readline()
            ser.write(cmd.encode())
        
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\n\nClosing connection...")
    ser.close()
