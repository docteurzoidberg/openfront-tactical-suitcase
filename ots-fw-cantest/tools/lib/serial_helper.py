#!/usr/bin/env python3
"""High-level serial helper utilities for cantest firmware.

Provides convenience functions built on top of device.py:
- Quick connection (auto-detect + open)
- Mode switching for cantest firmware
- Command sending with response wait

These helpers make scripts shorter and cleaner.
"""

import sys
import time
from typing import Optional

# Handle imports for both module use and direct execution
try:
    from lib.device import ESP32Device
except ImportError:
    from device import ESP32Device


# ==============================================================================
# Quick Connection
# ==============================================================================

def quick_connect(port: Optional[str] = None, 
                  capture_boot: bool = False,
                  baudrate: int = 115200) -> Optional[ESP32Device]:
    """Connect to ESP32 device in one call.
    
    Args:
        port: Serial port (None = auto-detect)
        capture_boot: Capture boot log on connection
        baudrate: Baud rate (default: 115200)
    
    Returns:
        Connected ESP32Device, or None if failed
    
    Example:
        device = quick_connect()
        if device:
            device.write('h')
            print(device.read())
            device.close()
    """
    # Auto-detect port if not specified
    if port is None:
        port = ESP32Device.auto_detect_port()
        if port is None:
            print("Error: No ESP32 device found", file=sys.stderr)
            return None
    
    # Create and open device
    device = ESP32Device(port, baudrate=baudrate)
    if not device.open(capture_boot_log=capture_boot):
        print(f"Error: Failed to open {port}", file=sys.stderr)
        return None
    
    return device


# ==============================================================================
# Mode Switching (cantest firmware specific)
# ==============================================================================

def enter_monitor_mode(device: ESP32Device, timeout: float = 1.0) -> bool:
    """Enter CAN monitor mode.
    
    Sends 'm' command and waits for confirmation.
    
    Args:
        device: Connected ESP32Device
        timeout: Wait timeout in seconds
    
    Returns:
        True if mode entered successfully
    """
    if not device.is_open:
        return False
    
    # Clear any pending data
    device.flush_input()
    
    # Send mode command
    device.write('m')
    time.sleep(0.1)
    
    # Read response
    response = device.read(timeout=timeout)
    
    # Check for monitor mode confirmation
    # (cantest firmware typically echoes mode or shows prompt)
    return 'monitor' in response.lower() or len(response) > 0


def enter_audio_mode(device: ESP32Device, timeout: float = 1.0) -> bool:
    """Enter audio module simulator mode.
    
    Sends 'a' command and waits for confirmation.
    
    Args:
        device: Connected ESP32Device
        timeout: Wait timeout in seconds
    
    Returns:
        True if mode entered successfully
    """
    if not device.is_open:
        return False
    
    device.flush_input()
    device.write('a')
    time.sleep(0.1)
    
    response = device.read(timeout=timeout)
    return 'audio' in response.lower() or len(response) > 0


def enter_controller_mode(device: ESP32Device, timeout: float = 1.0) -> bool:
    """Enter controller simulator mode.
    
    Sends 'c' command and waits for confirmation.
    
    Args:
        device: Connected ESP32Device
        timeout: Wait timeout in seconds
    
    Returns:
        True if mode entered successfully
    """
    if not device.is_open:
        return False
    
    device.flush_input()
    device.write('c')
    time.sleep(0.1)
    
    response = device.read(timeout=timeout)
    return 'controller' in response.lower() or len(response) > 0


def exit_to_menu(device: ESP32Device, timeout: float = 1.0) -> bool:
    """Exit current mode back to main menu.
    
    Sends 'q' command (quit) to return to menu.
    
    Args:
        device: Connected ESP32Device
        timeout: Wait timeout in seconds
    
    Returns:
        True if exited successfully
    """
    if not device.is_open:
        return False
    
    device.flush_input()
    device.write('q')
    time.sleep(0.2)
    
    response = device.read(timeout=timeout)
    # Menu typically shows available commands
    return len(response) > 0


def switch_mode(device: ESP32Device, target_mode: str, timeout: float = 2.0) -> bool:
    """Switch to target mode from any current mode.
    
    Args:
        device: Connected ESP32Device
        target_mode: Target mode ('m'=monitor, 'a'=audio, 'c'=controller)
        timeout: Total timeout for mode switch
    
    Returns:
        True if switched successfully
    """
    if not device.is_open:
        return False
    
    # Exit current mode first
    exit_to_menu(device, timeout=timeout/2)
    time.sleep(0.1)
    
    # Enter target mode
    if target_mode == 'm':
        return enter_monitor_mode(device, timeout=timeout/2)
    elif target_mode == 'a':
        return enter_audio_mode(device, timeout=timeout/2)
    elif target_mode == 'c':
        return enter_controller_mode(device, timeout=timeout/2)
    else:
        print(f"Error: Unknown mode '{target_mode}'", file=sys.stderr)
        return False


# ==============================================================================
# Command Sending with Wait
# ==============================================================================

def send_and_wait(device: ESP32Device, 
                  command: str, 
                  wait_time: float = 0.2,
                  read_timeout: float = 1.0) -> str:
    """Send command and wait for response.
    
    Args:
        device: Connected ESP32Device
        command: Command string to send
        wait_time: Time to wait before reading (seconds)
        read_timeout: Read timeout (seconds)
    
    Returns:
        Response string
    
    Example:
        response = send_and_wait(device, 'h', wait_time=0.2)
        print(response)
    """
    if not device.is_open:
        return ""
    
    device.write(command)
    time.sleep(wait_time)
    return device.read(timeout=read_timeout)


# ==============================================================================
# Test/Demo Code
# ==============================================================================

if __name__ == '__main__':
    print("Serial Helper Test")
    print("=" * 60)
    
    # Test quick connect
    print("\n1. Testing quick_connect()...")
    device = quick_connect()
    
    if device:
        print(f"   ✓ Connected to {device.port}")
        
        # Test mode switching
        print("\n2. Testing mode switching...")
        
        print("   Entering monitor mode...")
        if enter_monitor_mode(device):
            print("   ✓ Monitor mode entered")
        else:
            print("   ✗ Failed to enter monitor mode")
        
        time.sleep(0.5)
        
        print("   Exiting to menu...")
        if exit_to_menu(device):
            print("   ✓ Exited to menu")
        else:
            print("   ✗ Failed to exit")
        
        time.sleep(0.5)
        
        # Test send_and_wait
        print("\n3. Testing send_and_wait()...")
        response = send_and_wait(device, 'h', wait_time=0.2)
        if response:
            print(f"   ✓ Got response ({len(response)} chars)")
            print(f"   First 100 chars: {response[:100]}...")
        else:
            print("   ✗ No response")
        
        device.close()
        print("\n✓ Test complete")
    else:
        print("   ✗ Connection failed")
