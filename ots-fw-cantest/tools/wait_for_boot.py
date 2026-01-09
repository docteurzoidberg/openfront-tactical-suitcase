#!/usr/bin/env python3
"""
Wait for device to boot and show expected marker in serial output.

Usage:
    ./wait_for_boot.py --port /dev/ttyACM0 --marker "CAN Test Tool" --timeout 10
    ./wait_for_boot.py --port /dev/ttyUSB0 --marker "Audio module ready" --timeout 15
    ./wait_for_boot.py --port /dev/ttyACM0 --marker "Boot complete" --marker "System ready" --timeout 5

Exit codes:
    0 - Marker found
    1 - Timeout (marker not found)
    2 - Serial port error
"""

import argparse
import sys
import time
import serial


def wait_for_marker(port: str, markers: list, timeout: float, baud: int = 115200, 
                   flush_first: bool = True, verbose: bool = False) -> bool:
    """
    Wait for one of the markers to appear in serial output.
    
    Returns True if marker found, False on timeout.
    """
    try:
        ser = serial.Serial(port, baud, timeout=1)
        
        if flush_first:
            # Flush any old data in buffer
            ser.reset_input_buffer()
            if verbose:
                print(f"Flushed input buffer on {port}")
        
        start_time = time.time()
        
        if verbose:
            print(f"Waiting for markers: {markers}")
            print(f"Timeout: {timeout}s")
        
        while time.time() - start_time < timeout:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if verbose and line:
                        print(f"[{port}] {line}")
                    
                    # Check for any marker
                    for marker in markers:
                        if marker in line:
                            if not verbose:
                                print(line)  # Print matching line
                            print(f"✓ Marker found: '{marker}'")
                            ser.close()
                            return True
                            
                except UnicodeDecodeError:
                    # Ignore decode errors
                    pass
            else:
                time.sleep(0.1)
        
        ser.close()
        print(f"✗ Timeout: No marker found in {timeout}s", file=sys.stderr)
        return False
        
    except serial.SerialException as e:
        print(f"✗ Serial error: {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"✗ Error: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Wait for device boot marker in serial output",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Wait for cantest boot
  %(prog)s --port /dev/ttyACM0 --marker "CAN Test Tool" --timeout 10

  # Wait for audiomodule boot
  %(prog)s --port /dev/ttyUSB0 --marker "Audio module ready" --timeout 15

  # Multiple markers (OR logic)
  %(prog)s --port /dev/ttyACM0 --marker "Boot complete" --marker "System ready"

  # Verbose output (show all lines)
  %(prog)s --port /dev/ttyACM0 --marker "CAN" --timeout 5 --verbose
"""
    )
    
    parser.add_argument("--port", required=True,
                       help="Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0)")
    
    parser.add_argument("--marker", action="append", required=True,
                       help="Marker string to wait for (can specify multiple)")
    
    parser.add_argument("--timeout", type=float, default=10.0,
                       help="Timeout in seconds (default: 10)")
    
    parser.add_argument("--baud", type=int, default=115200,
                       help="Baud rate (default: 115200)")
    
    parser.add_argument("--no-flush", action="store_true",
                       help="Don't flush input buffer before reading")
    
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Print all received lines")
    
    args = parser.parse_args()
    
    success = wait_for_marker(
        port=args.port,
        markers=args.marker,
        timeout=args.timeout,
        baud=args.baud,
        flush_first=not args.no_flush,
        verbose=args.verbose
    )
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
