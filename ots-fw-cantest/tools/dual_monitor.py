#!/usr/bin/env python3
"""
Dual CAN Board Monitor
Monitors both ESP32 boards simultaneously and displays their output side-by-side
"""

import argparse
import serial
import threading
import sys
import time
from queue import Queue

# Output queues
audio_queue = Queue()
ctrl_queue = Queue()

def read_serial(port_name, port, queue, prefix):
    """Read from serial port and add to queue with prefix"""
    try:
        while True:
            if port.in_waiting > 0:
                line = port.readline().decode('utf-8', errors='replace').rstrip()
                queue.put(f"[{prefix:5s}] {line}")
    except Exception as e:
        queue.put(f"[{prefix:5s}] ERROR: {e}")

def display_output():
    """Display output from both queues"""
    while True:
        # Check audio queue
        if not audio_queue.empty():
            print(audio_queue.get())
            
        # Check ctrl queue  
        if not ctrl_queue.empty():
            print(ctrl_queue.get())
            
        time.sleep(0.01)

def send_command(port, command):
    """Send a command to a serial port"""
    port.write(f"{command}\n".encode())
    port.flush()

def main():
    parser = argparse.ArgumentParser(description='Dual CAN Board Monitor (interactive)')
    parser.add_argument('--audio', default='/dev/ttyUSB0', help='Audio device serial port')
    parser.add_argument('--controller', default='/dev/ttyACM0', help='Controller device serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Serial baud rate')
    args = parser.parse_args()

    audio_port = args.audio
    ctrl_port = args.controller
    baud_rate = args.baud

    print("=== Dual CAN Board Monitor ===")
    print(f"Audio Module: {audio_port}")
    print(f"Controller:   {ctrl_port}")
    print("")

    audio_serial = None
    ctrl_serial = None
    
    try:
        # Open serial ports
        audio_serial = serial.Serial(audio_port, baud_rate, timeout=1)
        ctrl_serial = serial.Serial(ctrl_port, baud_rate, timeout=1)
        
        print("✓ Both serial ports opened")
        print("")
        print("Commands:")
        print("  aa - Start audio module simulator")
        print("  ac - Start controller simulator")
        print("  ad - Send MODULE_QUERY from controller")
        print("  at - Show statistics (audio)")
        print("  ct - Show statistics (controller)")
        print("  am - Monitor mode (audio)")
        print("  cm - Monitor mode (controller)")
        print("  q  - Quit")
        print("")
        
        # Start reader threads
        audio_thread = threading.Thread(
            target=read_serial, 
            args=(audio_port, audio_serial, audio_queue, "AUDIO"),
            daemon=True
        )
        ctrl_thread = threading.Thread(
            target=read_serial,
            args=(ctrl_port, ctrl_serial, ctrl_queue, "CTRL"),
            daemon=True
        )
        
        audio_thread.start()
        ctrl_thread.start()
        
        # Start display thread
        display_thread = threading.Thread(target=display_output, daemon=True)
        display_thread.start()
        
        print("Monitoring started. Type commands below:\n")
        
        # Main command loop
        while True:
            try:
                cmd = input("> ").strip().lower()
                
                if cmd == 'q':
                    break
                elif cmd == 'aa':
                    print("→ Audio: Starting audio module simulator...")
                    send_command(audio_serial, 'a')
                elif cmd == 'ac':
                    print("→ Controller: Starting controller simulator...")
                    send_command(ctrl_serial, 'c')
                elif cmd == 'ad':
                    print("→ Controller: Sending MODULE_QUERY...")
                    send_command(ctrl_serial, 'd')
                elif cmd == 'at':
                    print("→ Audio: Requesting statistics...")
                    send_command(audio_serial, 't')
                elif cmd == 'ct':
                    print("→ Controller: Requesting statistics...")
                    send_command(ctrl_serial, 't')
                elif cmd == 'am':
                    print("→ Audio: Entering monitor mode...")
                    send_command(audio_serial, 'm')
                elif cmd == 'cm':
                    print("→ Controller: Entering monitor mode...")
                    send_command(ctrl_serial, 'm')
                else:
                    print(f"Unknown command: {cmd}")
                    
            except KeyboardInterrupt:
                break
                
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        return 1
    finally:
        print("\nClosing connections...")
        if audio_serial:
            audio_serial.close()
        if ctrl_serial:
            ctrl_serial.close()
        
    return 0

if __name__ == '__main__':
    sys.exit(main())
