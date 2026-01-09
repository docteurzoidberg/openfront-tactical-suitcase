#!/usr/bin/env python3
"""
Test script to play all available sounds on audio module.
Sends PLAY_SOUND commands for each sound index with 1s pause between each.
"""

import serial
import time
import sys

# Sound mapping (indices 0-7 are game sounds)
SOUNDS = {
    0: "game_start.wav",
    1: "game_victory.wav", 
    2: "game_defeat.wav",
    3: "game_player_death.wav",
    4: "alert_nuke.wav",
    5: "alert_land.wav",
    6: "alert_naval.wav",
    7: "nuke_launch.wav",
}

def send_play_sound(ser, sound_index):
    """Send PLAY_SOUND command via cantest controller"""
    print(f"\n{'='*60}")
    print(f"Playing sound {sound_index}: {SOUNDS.get(sound_index, 'unknown')}")
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
    if 'SOUND_ACK' in response:
        if 'Status: OK' in response or 'Status: 0x00' in response:
            print("✅ Sound started successfully")
        elif 'Status: FILE_NOT_FOUND' in response:
            print("⚠️ File not found")
        elif 'Status: MIXER_FULL' in response:
            print("⚠️ Mixer full")
        else:
            print("⚠️ Unknown response")
        
        # Show response excerpt
        for line in response.split('\n'):
            if 'SOUND_ACK' in line or 'Queue ID' in line:
                print(f"    {line.strip()}")
    else:
        print("❌ No ACK received")
        print(f"Response: {response[:200]}")
    
    return 'SOUND_ACK' in response

def main():
    port = '/dev/ttyACM0'  # ESP32-S3 with cantest
    
    print("="*60)
    print("AUDIO MODULE - PLAY ALL SOUNDS TEST")
    print("="*60)
    print(f"Controller: {port}")
    print(f"Sounds to test: {len(SOUNDS)}")
    print(f"Pause between sounds: 1 second")
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
        
        # Test each sound
        results = {}
        for sound_idx in sorted(SOUNDS.keys()):
            success = send_play_sound(ser, sound_idx)
            results[sound_idx] = success
            
            # Pause before next sound
            if sound_idx < max(SOUNDS.keys()):
                print("Waiting 1 second...")
                time.sleep(1)
        
        # Summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        
        for idx in sorted(results.keys()):
            status = "✅ OK" if results[idx] else "❌ FAIL"
            print(f"Sound {idx} ({SOUNDS[idx]:25s}): {status}")
        
        success_count = sum(1 for v in results.values() if v)
        print(f"\nTotal: {success_count}/{len(SOUNDS)} sounds played successfully")
        
        if success_count == len(SOUNDS):
            print("✅ All sounds tested successfully!")
        else:
            print(f"⚠️ {len(SOUNDS) - success_count} sounds failed")
        
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
