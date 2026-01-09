#!/usr/bin/env python3
"""
Test embedded tones via CAN protocol using cantest controller.
Tests sound indices 10000-10002 (embedded test tones) and 10100 (quack).
"""

import serial
import time
import sys

# Embedded tones (compiled into firmware, no SD card needed)
EMBEDDED_TONES = {
    10000: "Test tone 1 (1s, 440Hz)",
    10001: "Test tone 2 (2s, 880Hz)",
    10002: "Test tone 3 (5s, 220Hz)",
    10100: "Quack sound (embedded WAV)",
}

def send_play_sound(ser, sound_index, wait_time=1):
    """Send PLAY_SOUND command via cantest controller"""
    description = EMBEDDED_TONES.get(sound_index, f"Sound {sound_index}")
    print(f"\n{'='*60}")
    print(f"Playing: {description}")
    print(f"Sound ID: {sound_index}")
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
    
    # Parse response
    queue_id = None
    success = False
    status = "UNKNOWN"
    
    for line in response.split('\n'):
        if 'SOUND_ACK' in line:
            print(f"  {line.strip()}")
        
        if 'Status: SUCCESS' in line or 'Status: 0x00' in line:
            success = True
            status = "SUCCESS"
        elif 'Status: FILE_NOT_FOUND' in line:
            status = "FILE_NOT_FOUND"
        elif 'Status: MIXER_FULL' in line:
            status = "MIXER_FULL"
        
        if 'Queue ID:' in line:
            try:
                qid_str = line.split('Queue ID:')[1].split()[0]
                queue_id = int(qid_str)
                if queue_id > 0:
                    success = True
            except:
                pass
    
    if success:
        print(f"  ✅ Playing (queue_id={queue_id})")
    else:
        print(f"  ⚠️ Status: {status}")
    
    # Wait for sound to finish
    if wait_time > 0:
        print(f"  Waiting {wait_time}s for sound to finish...")
        time.sleep(wait_time)
    
    return success, queue_id

def main():
    port = '/dev/ttyACM0'  # ESP32-S3 with cantest
    
    print("="*60)
    print("EMBEDDED TONES TEST via CAN Protocol")
    print("="*60)
    print(f"Controller: {port} (cantest)")
    print(f"Audio module: /dev/ttyUSB1 (fw-audiomodule)")
    print(f"\nTesting {len(EMBEDDED_TONES)} embedded sounds:")
    for idx, desc in sorted(EMBEDDED_TONES.items()):
        print(f"  {idx}: {desc}")
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
        
        # Test each embedded tone
        results = {}
        queue_ids = {}
        
        for sound_idx in sorted(EMBEDDED_TONES.keys()):
            # Determine wait time based on tone duration
            if sound_idx == 10002:
                wait_time = 5  # 5s tone
            elif sound_idx == 10001:
                wait_time = 2  # 2s tone
            else:
                wait_time = 1  # 1s tones and quack
            
            success, queue_id = send_play_sound(ser, sound_idx, wait_time)
            results[sound_idx] = success
            queue_ids[sound_idx] = queue_id
        
        # Summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        
        for idx in sorted(results.keys()):
            status = "✅ PLAYED" if results[idx] else "❌ FAILED"
            qid = f"(queue_id={queue_ids[idx]})" if queue_ids[idx] else ""
            desc = EMBEDDED_TONES[idx]
            print(f"{idx}: {status} {qid}")
            print(f"       {desc}")
        
        success_count = sum(1 for v in results.values() if v)
        print(f"\nTotal: {success_count}/{len(EMBEDDED_TONES)} tones played")
        
        if success_count == len(EMBEDDED_TONES):
            print("\n✅ All embedded tones working via CAN!")
            print("   No SD card required - sounds are compiled into firmware")
        else:
            print(f"\n⚠️ {len(EMBEDDED_TONES) - success_count} tones failed")
        
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
