#!/usr/bin/env python3
"""
Test embedded sounds via CAN protocol using cantest controller.
Tests only the sound indices that are known to work (embedded in firmware).
"""

import serial
import time
import sys

# Known working embedded sounds (from previous test)
EMBEDDED_SOUNDS = {
    0: "Embedded sound 0",
    1: "Embedded sound 1", 
    2: "Embedded sound 2",
    3: "Embedded sound 3",
}

def send_play_sound(ser, sound_index):
    """Send PLAY_SOUND command via cantest controller"""
    print(f"\n{'='*60}")
    print(f"Playing embedded sound {sound_index}")
    print('='*60)
    
    # Clear input buffer
    ser.read(ser.in_waiting)
    
    # Send play command: p <index>
    cmd = f"p {sound_index}\n"
    ser.write(cmd.encode())
    
    # Wait for response
    time.sleep(0.5)
    
    # Read response
    response = ""
    start = time.time()
    while time.time() - start < 2:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            response += chunk
            
            # Check for SOUND_ACK
            if 'SOUND_ACK' in chunk or '0x423' in chunk:
                break
        time.sleep(0.1)
    
    # Parse response for queue ID
    queue_id = None
    success = False
    for line in response.split('\n'):
        if 'SOUND_ACK' in line:
            print(f"  {line.strip()}")
        if 'Queue ID:' in line and 'Queue ID: 0' not in line:
            # Extract queue ID
            try:
                queue_id = int(line.split('Queue ID:')[1].split()[0])
                success = True
            except:
                pass
        if 'Status: SUCCESS' in line or 'Status: 0x00' in line:
            success = True
            print(f"  ✅ Sound playing")
    
    if not success:
        print(f"  ⚠️ No successful ACK")
    
    return success, queue_id

def main():
    port = '/dev/ttyACM0'  # ESP32-S3 with cantest
    
    print("="*60)
    print("EMBEDDED SOUNDS TEST via CAN Protocol")
    print("="*60)
    print(f"Controller: {port} (cantest)")
    print(f"Audio module: /dev/ttyUSB1 (fw-audiomodule)")
    print(f"Testing embedded sounds: {list(EMBEDDED_SOUNDS.keys())}")
    print(f"Pause between sounds: 2 seconds")
    print("="*60)
    
    try:
        # Open serial connection
        ser = serial.Serial(port, 115200, timeout=2)
        time.sleep(0.5)
        
        # Clear buffer
        ser.read(ser.in_waiting)
        
        # Ensure controller mode is active
        print("\nEnsuring controller mode...")
        ser.write(b'c\n')
        time.sleep(1)
        ser.read(ser.in_waiting)
        
        # Test each embedded sound
        results = {}
        queue_ids = {}
        
        for sound_idx in sorted(EMBEDDED_SOUNDS.keys()):
            success, queue_id = send_play_sound(ser, sound_idx)
            results[sound_idx] = success
            queue_ids[sound_idx] = queue_id
            
            # Pause before next sound
            if sound_idx < max(EMBEDDED_SOUNDS.keys()):
                print(f"Waiting 2 seconds before next sound...")
                time.sleep(2)
        
        # Summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        
        for idx in sorted(results.keys()):
            status = "✅ PLAYING" if results[idx] else "❌ FAILED"
            qid = f"(queue_id={queue_ids[idx]})" if queue_ids[idx] else ""
            print(f"Sound {idx}: {status} {qid}")
        
        success_count = sum(1 for v in results.values() if v)
        print(f"\nTotal: {success_count}/{len(EMBEDDED_SOUNDS)} sounds played")
        
        if success_count == len(EMBEDDED_SOUNDS):
            print("✅ All embedded sounds working via CAN!")
        else:
            print(f"⚠️ {len(EMBEDDED_SOUNDS) - success_count} sounds failed")
        
        ser.close()
        
    except serial.SerialException as e:
        print(f"\n❌ Serial error: {e}")
        print(f"Make sure {port} is available and cantest is running")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n⚠️ Test interrupted by user")
        sys.exit(1)

if __name__ == '__main__':
    main()
