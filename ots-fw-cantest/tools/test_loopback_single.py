#!/usr/bin/env python3
"""
Simple single-device loopback test
Goal: Verify RX works in loopback mode where device receives own TX
"""

import serial
import time
import sys

def test_single_loopback(port, board_name):
    print(f"\n{'='*70}")
    print(f"Testing loopback on {board_name} ({port})")
    print(f"{'='*70}\n")
    
    ser = serial.Serial(port, 115200, timeout=2)
    time.sleep(2)  # Wait for boot
    
    # Clear buffer
    ser.reset_input_buffer()
    
    # Read initial status
    print("1. Getting initial TWAI status...")
    ser.write(b't\n')  # Show statistics (includes TWAI status now)
    time.sleep(0.5)
    
    lines = []
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            lines.append(line)
            print(f"  {line}")
    
    # Parse initial stats
    initial_tx = 0
    initial_rx = 0
    for line in lines:
        if 'Messages TX:' in line:
            # Extract number from "║  Messages TX:     0║" format
            parts = line.split(':')
            if len(parts) > 1:
                num_str = parts[1].strip().replace('║', '').strip()
                try:
                    initial_tx = int(num_str)
                except:
                    pass
        if 'Messages RX:' in line:
            parts = line.split(':')
            if len(parts) > 1:
                num_str = parts[1].strip().replace('║', '').strip()
                try:
                    initial_rx = int(num_str)
                except:
                    pass
    
    print(f"\n2. Initial stats: TX={initial_tx}, RX={initial_rx}")
    
    # Enter controller simulator mode
    print("\n3. Entering controller simulator mode...")
    ser.write(b'c\n')
    time.sleep(0.5)
    
    # Clear initial mode banner
    while ser.in_waiting:
        ser.readline()
    
    # Send discovery query command
    print("\n4. Sending MODULE_QUERY (discovery) command...")
    ser.write(b'd\n')
    time.sleep(0.5)
    
    # Read logs for 2 seconds
    print("\n5. Reading logs for TX and RX activity...")
    start_time = time.time()
    while time.time() - start_time < 2:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"  {line}")
    
    # Exit simulator
    print("\n6. Exiting simulator mode...")
    ser.write(b'q\n')
    time.sleep(0.5)
    
    # Check final stats
    print("\n7. Getting final TWAI status...")
    ser.reset_input_buffer()
    ser.write(b't\n')
    time.sleep(0.5)
    
    final_lines = []
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            final_lines.append(line)
            print(f"  {line}")
    
    # Parse final stats
    final_tx = 0
    final_rx = 0
    msgs_to_rx = None
    twai_state = None
    
    for line in final_lines:
        if 'Messages TX:' in line:
            parts = line.split(':')
            if len(parts) > 1:
                num_str = parts[1].strip().replace('║', '').strip()
                try:
                    final_tx = int(num_str)
                except:
                    pass
        if 'Messages RX:' in line:
            parts = line.split(':')
            if len(parts) > 1:
                num_str = parts[1].strip().replace('║', '').strip()
                try:
                    final_rx = int(num_str)
                except:
                    pass
        if 'RX Queue:' in line and 'msgs waiting' in line:
            msgs_to_rx = line.split(':')[1].strip()
        if 'State:' in line and 'TWAI' in lines[max(0, final_lines.index(line) - 2)]:
            twai_state = line.split(':')[1].strip()
    
    # Analyze results
    print(f"\n{'='*70}")
    print("RESULTS:")
    print(f"{'='*70}")
    print(f"  TX count: {initial_tx} → {final_tx} (delta: {final_tx - initial_tx})")
    print(f"  RX count: {initial_rx} → {final_rx} (delta: {final_rx - initial_rx})")
    
    if twai_state:
        print(f"  TWAI State: {twai_state}")
    if msgs_to_rx is not None:
        print(f"  TWAI RX Queue: {msgs_to_rx}")
    
    print()
    
    # Verdict
    tx_delta = final_tx - initial_tx
    rx_delta = final_rx - initial_rx
    
    if rx_delta > 0:
        print("✓ SUCCESS: Loopback working! Device received own transmitted messages.")
        print("  → This proves RX hardware path is functional")
        print("  → Problem must be with inter-device communication")
        return True
    elif tx_delta > 0:
        print("✗ FAILURE: TX works but RX failed (even in loopback mode)")
        print("  → Device transmitted but didn't receive own message")
        print("  → This should NOT happen in loopback mode with NO_ACK")
        print()
        print("Possible causes:")
        print("  1. TWAI peripheral not actually in loopback mode")
        print("  2. Filter configuration blocks all messages")
        print("  3. GPIO RX pin not connected to TWAI controller")
        print("  4. Missing RX callback registration")
        return False
    else:
        print("✗ FAILURE: No TX or RX activity detected")
        print("  → Driver may not be working at all")
        print("  → Check if device entered simulator mode correctly")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 test_loopback_single.py <port> [board_name]")
        print()
        print("Examples:")
        print("  python3 test_loopback_single.py /dev/ttyACM1 'ESP32-S3'")
        print("  python3 test_loopback_single.py /dev/ttyUSB1 'ESP32-A1S'")
        sys.exit(1)
    
    port = sys.argv[1]
    board_name = sys.argv[2] if len(sys.argv) > 2 else port
    
    try:
        success = test_single_loopback(port, board_name)
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n✗ ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(2)
