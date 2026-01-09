#!/usr/bin/env python3
"""ESP32 device serial communication handler for cantest firmware.

This module provides the foundation for all CAN test tools. It handles:
- Serial connection management (open/close)
- Hardware reset via DTR/RTS pins
- Boot log capture (until firmware ready)
- Command send/receive
- Context manager support
- Port auto-detection

All test tools depend on this module.
"""

import sys
import time
import serial
import serial.tools.list_ports
from typing import Optional, List
from dataclasses import dataclass


@dataclass
class DeviceInfo:
    """Information about a detected ESP32 device."""
    port: str
    description: str
    hwid: str
    vid: Optional[int] = None
    pid: Optional[int] = None


class ESP32Device:
    """ESP32 device serial handler with reset and boot log capture.
    
    Usage:
        # Basic usage
        device = ESP32Device('/dev/ttyUSB0')
        if device.open():
            device.write('h')  # Send help command
            response = device.read(timeout=1.0)
            print(response)
            device.close()
        
        # Context manager (recommended)
        with ESP32Device('/dev/ttyUSB0') as device:
            if device.is_open:
                device.write('m')  # Enter monitor mode
                data = device.read(timeout=5.0)
    
    Attributes:
        port: Serial port path (e.g., '/dev/ttyUSB0')
        baudrate: Serial baud rate (default: 115200)
        timeout: Default read timeout in seconds
        boot_log: List of boot log lines (if reset performed)
    """
    
    # ESP32 USB VID/PID for auto-detection
    ESP32_VID_PID = [
        (0x10C4, 0xEA60),  # Silicon Labs CP2102
        (0x1A86, 0x7523),  # CH340
        (0x0403, 0x6001),  # FTDI FT232
    ]
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 1.0):
        """Initialize ESP32 device handler.
        
        Args:
            port: Serial port path
            baudrate: Baud rate (default: 115200)
            timeout: Default read timeout in seconds
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial: Optional[serial.Serial] = None
        self.boot_log: List[str] = []
        self._is_open = False
    
    @property
    def is_open(self) -> bool:
        """Check if device is connected."""
        return self._is_open and self.serial is not None and self.serial.is_open
    
    def open(self, capture_boot_log: bool = False, reset_wait: float = 0.5) -> bool:
        """Open serial connection to device.
        
        Args:
            capture_boot_log: If True, perform hardware reset and capture boot log
            reset_wait: Seconds to wait after reset (default: 0.5s)
        
        Returns:
            True if connection successful, False otherwise
        """
        try:
            # Open serial port
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=1.0
            )
            
            self._is_open = True
            
            # Perform reset and capture boot log if requested
            if capture_boot_log:
                self.reset(wait_time=reset_wait, capture_log=True)
            else:
                # Small delay for connection stability
                time.sleep(0.1)
                self.flush_input()
            
            return True
            
        except serial.SerialException as e:
            print(f"Error opening {self.port}: {e}", file=sys.stderr)
            self._is_open = False
            return False
    
    def close(self):
        """Close serial connection."""
        if self.serial and self.serial.is_open:
            self.serial.close()
        self._is_open = False
    
    def reset(self, wait_time: float = 0.5, capture_log: bool = True) -> bool:
        """Perform hardware reset via DTR/RTS pins.
        
        This triggers the ESP32 bootloader sequence by toggling DTR and RTS pins.
        The standard ESP32 reset sequence is:
        1. Set RTS=1, DTR=0 (enter reset)
        2. Set RTS=0, DTR=1 (exit reset, enter bootloader mode)
        3. Set DTR=0 (normal boot)
        
        Args:
            wait_time: Seconds to wait after reset for firmware to boot
            capture_log: If True, capture boot log lines
        
        Returns:
            True if reset successful, False otherwise
        """
        if not self.is_open:
            return False
        
        try:
            # Clear old boot log
            self.boot_log = []
            
            # Flush any pending data
            self.flush_input()
            
            # ESP32 reset sequence
            self.serial.setRTS(True)   # RTS=1
            self.serial.setDTR(False)  # DTR=0
            time.sleep(0.1)
            
            self.serial.setRTS(False)  # RTS=0
            self.serial.setDTR(True)   # DTR=1
            time.sleep(0.1)
            
            self.serial.setDTR(False)  # DTR=0 (normal boot)
            
            # Wait for boot and capture log
            if capture_log:
                self._capture_boot_log(timeout=wait_time + 2.0)
            else:
                time.sleep(wait_time)
            
            return True
            
        except serial.SerialException as e:
            print(f"Error during reset: {e}", file=sys.stderr)
            return False
    
    def _capture_boot_log(self, timeout: float = 2.5):
        """Capture boot log lines until firmware ready.
        
        Reads lines from serial until:
        - "Ready" prompt detected (firmware ready)
        - Timeout expires
        - No data for 1.5 seconds (increased patience for slower boots)
        
        Args:
            timeout: Maximum time to capture boot log
        """
        start_time = time.time()
        line_buffer = ""
        last_data_time = time.time()
        no_data_timeout = 1.5  # Increased from 0.5s to handle boot pauses
        
        while time.time() - start_time < timeout:
            # Read available data
            if self.serial.in_waiting > 0:
                try:
                    chunk = self.serial.read(self.serial.in_waiting).decode('utf-8', errors='replace')
                    line_buffer += chunk
                    last_data_time = time.time()
                    
                    # Process complete lines
                    while '\n' in line_buffer:
                        line, line_buffer = line_buffer.split('\n', 1)
                        line = line.strip()
                        if line:
                            self.boot_log.append(line)
                            
                            # Check for firmware ready (more patterns)
                            line_lower = line.lower()
                            if ('ready' in line_lower or 
                                '[cantest]' in line_lower or
                                'ready>' in line_lower or
                                'command>' in line_lower or
                                'audio module ready' in line_lower):
                                # Give a bit more time for any final output
                                time.sleep(0.2)
                                return
                
                except UnicodeDecodeError:
                    # Skip invalid characters
                    pass
            
            # Stop if no data for extended period (firmware likely ready or idle)
            if time.time() - last_data_time > no_data_timeout:
                break
            
            time.sleep(0.01)  # Small delay to avoid busy loop
    
    def flush_input(self):
        """Flush input buffer (discard pending data)."""
        if self.is_open:
            self.serial.reset_input_buffer()
    
    def flush_output(self):
        """Flush output buffer."""
        if self.is_open:
            self.serial.reset_output_buffer()
    
    def write(self, data: str):
        """Write string to serial port.
        
        Args:
            data: String to write (will be encoded as UTF-8)
        """
        if not self.is_open:
            raise IOError("Device not open")
        
        # Add newline if not present
        if not data.endswith('\n'):
            data += '\n'
        
        # Write without blocking flush - let OS buffer handle it
        self.serial.write(data.encode('utf-8'))
        # Don't call flush() - it blocks until TX buffer empty
    
    def read(self, timeout: Optional[float] = None) -> str:
        """Read from serial port with timeout.
        
        Args:
            timeout: Read timeout in seconds (None = use default)
        
        Returns:
            String data read from serial port
        """
        if not self.is_open:
            raise IOError("Device not open")
        
        # Set temporary timeout if specified
        old_timeout = self.serial.timeout
        if timeout is not None:
            self.serial.timeout = timeout
        
        try:
            # Read all available data
            data = b''
            start_time = time.time()
            effective_timeout = timeout if timeout is not None else self.timeout
            
            while time.time() - start_time < effective_timeout:
                if self.serial.in_waiting > 0:
                    chunk = self.serial.read(self.serial.in_waiting)
                    data += chunk
                    # Continue reading if more data might come
                    time.sleep(0.01)
                else:
                    if data:  # Got some data, stop reading
                        break
                    time.sleep(0.01)
            
            return data.decode('utf-8', errors='replace')
            
        finally:
            # Restore original timeout
            if timeout is not None:
                self.serial.timeout = old_timeout
    
    def read_line(self, timeout: Optional[float] = None) -> str:
        """Read a single line from serial port.
        
        Args:
            timeout: Read timeout in seconds (None = use default)
        
        Returns:
            Single line (without newline characters)
        """
        if not self.is_open:
            raise IOError("Device not open")
        
        # Set temporary timeout if specified
        old_timeout = self.serial.timeout
        if timeout is not None:
            self.serial.timeout = timeout
        
        try:
            line = self.serial.readline().decode('utf-8', errors='replace')
            return line.strip()
            
        finally:
            # Restore original timeout
            if timeout is not None:
                self.serial.timeout = old_timeout
    
    def print_boot_log(self):
        """Print captured boot log to stderr."""
        if self.boot_log:
            print("\n=== Boot Log ===", file=sys.stderr)
            for line in self.boot_log:
                print(f"  {line}", file=sys.stderr)
            print("================\n", file=sys.stderr)
        else:
            print("No boot log captured", file=sys.stderr)
    
    def get_boot_log_text(self) -> str:
        """Get boot log as single string.
        
        Returns:
            Boot log lines joined with newlines
        """
        return '\n'.join(self.boot_log)
    
    # Context manager support
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
        return False  # Don't suppress exceptions
    
    @staticmethod
    def list_ports() -> List[DeviceInfo]:
        """List all available serial ports.
        
        Returns:
            List of DeviceInfo objects for all detected ports
        """
        ports = []
        for port in serial.tools.list_ports.comports():
            info = DeviceInfo(
                port=port.device,
                description=port.description,
                hwid=port.hwid,
                vid=port.vid,
                pid=port.pid
            )
            ports.append(info)
        return ports
    
    @staticmethod
    def find_esp32_devices() -> List[DeviceInfo]:
        """Auto-detect ESP32 devices by USB VID/PID.
        
        Returns:
            List of DeviceInfo objects for detected ESP32 devices
        """
        esp32_ports = []
        
        for port in ESP32Device.list_ports():
            if port.vid and port.pid:
                for vid, pid in ESP32Device.ESP32_VID_PID:
                    if port.vid == vid and port.pid == pid:
                        esp32_ports.append(port)
                        break
        
        return esp32_ports
    
    @staticmethod
    def auto_detect_port() -> Optional[str]:
        """Auto-detect first available ESP32 device port.
        
        Returns:
            Port path if found, None otherwise
        """
        # Try ESP32-specific detection first
        esp32_devices = ESP32Device.find_esp32_devices()
        if esp32_devices:
            return esp32_devices[0].port
        
        # Fallback: Try common port names
        common_ports = ['/dev/ttyUSB0', '/dev/ttyACM0', '/dev/ttyUSB1']
        for port_path in common_ports:
            try:
                # Try to open port briefly
                test_serial = serial.Serial(port_path, timeout=0.1)
                test_serial.close()
                return port_path
            except (serial.SerialException, FileNotFoundError):
                continue
        
        return None


# Convenience function for quick testing
def quick_test(port: Optional[str] = None):
    """Quick test function to verify device connection.
    
    Args:
        port: Serial port (None = auto-detect)
    """
    if port is None:
        port = ESP32Device.auto_detect_port()
        if port is None:
            print("No ESP32 device found!", file=sys.stderr)
            return
        print(f"Auto-detected device: {port}")
    
    print(f"Testing connection to {port}...")
    
    with ESP32Device(port) as device:
        if not device.open(capture_boot_log=True):
            print(f"Failed to connect to {port}", file=sys.stderr)
            return
        
        print("✓ Connected successfully")
        device.print_boot_log()
        
        # Send help command
        print("Sending 'h' (help) command...")
        device.write('h')
        time.sleep(0.2)
        response = device.read(timeout=1.0)
        print(f"Response:\n{response}")


if __name__ == '__main__':
    # Run quick test if executed directly
    import argparse
    
    parser = argparse.ArgumentParser(description='ESP32 device test utility')
    parser.add_argument('--port', help='Serial port (default: auto-detect)')
    parser.add_argument('--list', action='store_true', help='List all serial ports')
    
    args = parser.parse_args()
    
    if args.list:
        print("Available serial ports:")
        for info in ESP32Device.list_ports():
            print(f"  {info.port}: {info.description}")
            if info.vid and info.pid:
                print(f"    VID:PID = {info.vid:04X}:{info.pid:04X}")
        
        print("\nESP32 devices:")
        esp32_devices = ESP32Device.find_esp32_devices()
        if esp32_devices:
            for info in esp32_devices:
                print(f"  {info.port}: {info.description}")
        else:
            print("  None found")
    else:
        quick_test(args.port)
