#!/usr/bin/env python3
"""
Fixed discovery test - checks BOTH device queues
"""

import sys
import time
import serial
from pathlib import Path

def main():
    ctrl_port = '/dev/ttyACM0'
    audio_port = '/dev/ttyUSB0'
    
    print("=== Fixed Discovery Test ===\n")
    
    # Open connections
    ctrl = serial.Serial(ctrl_port, 115200, timeout=1)
    audio = serial.Serial(audio_port, 115200, timeout=1)
    
    # Reset both
    print("Resetting devices...")
    ctrl.dtr = False
    audio.dtr = False
    time.sleep(0.1)
    ctrl.dtr = True
    audio.dtr = True
    time.sleep(5)
    
    # Clear boot messages
    ctrl.read_all()
    audio.read_all()
    
    # Enter audio simulator mode
    print("Entering audio simulator mode...")
    audio.write(b'a\n')
    time.sleep(1)
    audio_resp = audio.read_all().decode('utf-8', errors='replace')
    if 'AUDIO MODULE SIMULATOR' in audio_resp:
        print("✓ Audio simulator active")
    else:
        print("✗ Failed to enter audio mode")
        return
    
    # Enter controller simulator mode
    print("Entering controller simulator mode...")
    ctrl.write(b'c\n')
    time.sleep(1)
    ctrl_resp = ctrl.read_all().decode('utf-8', errors='replace')
    if 'CONTROLLER SIMULATOR' in ctrl_resp:
        print("✓ Controller simulator active")
    else:
        print("✗ Failed to enter controller mode")
        return
    
    # Now send discovery
    print("\n=== Sending MODULE_QUERY ===\n")
    ctrl.write(b'd\n')
    
    # Wait and capture output from BOTH devices
    time.sleep(2)
    
    ctrl_output = ctrl.read_all().decode('utf-8', errors='replace')
    audio_output = audio.read_all().decode('utf-8', errors='replace')
    
    print("Controller Output:")
    print(ctrl_output)
    print("\n" + "="*60 + "\n")
    print("Audio Output:")
    print(audio_output)
    print("\n" + "="*60 + "\n")
    
    # Check results
    results = {
        'ctrl_tx_query': '→ TX: 0x411' in ctrl_output,
        'audio_rx_query': '← RX:' in audio_output and '0x411' in audio_output,
        'audio_tx_announce': '→ TX: MODULE_ANNOUNCE' in audio_output,
        'ctrl_rx_announce': '← RX:' in ctrl_output and '0x410' in ctrl_output
    }
    
    print("\n=== Results ===")
    print(f"Controller sent MODULE_QUERY (0x411): {'✓' if results['ctrl_tx_query'] else '✗'}")
    print(f"Audio received MODULE_QUERY (0x411): {'✓' if results['audio_rx_query'] else '✗'}")
    print(f"Audio sent MODULE_ANNOUNCE (0x410): {'✓' if results['audio_tx_announce'] else '✗'}")
    print(f"Controller received MODULE_ANNOUNCE (0x410): {'✓' if results['ctrl_rx_announce'] else '✗'}")
    
    all_passed = all(results.values())
    
    if all_passed:
        print("\n✓✓✓ ALL TESTS PASSED ✓✓✓")
    else:
        print("\n✗✗✗ TESTS FAILED ✗✗✗")
        print("\nThis confirms:")
        if not results['audio_rx_query']:
            print("  ❌ Audio module is NOT receiving CAN messages")
            print("  ❌ This is a PHYSICAL LAYER issue (hardware wiring)")
        
    ctrl.close()
    audio.close()

if __name__ == '__main__':
    main()
