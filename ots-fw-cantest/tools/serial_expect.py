#!/usr/bin/env python3
"""
Wait for pattern in serial output (like expect for serial ports).

Usage:
    ./serial_expect.py --port /dev/ttyACM0 --pattern "Audio module detected" --timeout 10
    ./serial_expect.py --port /dev/ttyACM0 --pattern "^\\[CAN\\] TX.*0x420" --regex --timeout 5
    ./serial_expect.py --port /dev/ttyACM0 --pattern "Boot" --pattern "Ready" --timeout 5

Exit codes:
    0 - Pattern found
    1 - Timeout (pattern not found)
    2 - Serial port error
"""

import argparse
import sys
import time
import serial
import re


def wait_for_pattern(port: str, patterns: list, timeout: float, 
                    use_regex: bool = False, baud: int = 115200, 
                    verbose: bool = False) -> tuple:
    """
    Wait for one of the patterns to match in serial output.
    
    Returns (True, matching_line) if pattern found, (False, None) on timeout.
    """
    try:
        ser = serial.Serial(port, baud, timeout=1)
        ser.reset_input_buffer()
        
        # Compile regexes if needed
        compiled_patterns = []
        if use_regex:
            compiled_patterns = [re.compile(p) for p in patterns]
        
        if verbose:
            print(f"Waiting for patterns: {patterns}")
            print(f"Regex mode: {use_regex}")
            print(f"Timeout: {timeout}s")
        
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if verbose and line:
                        print(f"[{port}] {line}")
                    
                    if not line:
                        continue
                    
                    # Check patterns
                    for i, pattern in enumerate(patterns):
                        matched = False
                        
                        if use_regex:
                            matched = compiled_patterns[i].search(line) is not None
                        else:
                            matched = pattern in line
                        
                        if matched:
                            ser.close()
                            return (True, line)
                            
                except UnicodeDecodeError:
                    pass
            else:
                time.sleep(0.05)
        
        ser.close()
        return (False, None)
        
    except serial.SerialException as e:
        print(f"✗ Serial error: {e}", file=sys.stderr)
        return (False, None)
    except Exception as e:
        print(f"✗ Error: {e}", file=sys.stderr)
        return (False, None)


def main():
    parser = argparse.ArgumentParser(
        description="Wait for pattern in serial output",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Wait for specific log line
  %(prog)s --port /dev/ttyACM0 --pattern "Audio module detected" --timeout 10

  # Multiple patterns (OR logic)
  %(prog)s --port /dev/ttyACM0 --pattern "Boot complete" --pattern "System ready"

  # Regex pattern
  %(prog)s --port /dev/ttyACM0 --pattern "^\\[CAN\\] TX.*0x420" --regex --timeout 5

  # Verbose (show all lines)
  %(prog)s --port /dev/ttyACM0 --pattern "Ready" --timeout 10 --verbose
"""
    )
    
    parser.add_argument("--port", required=True,
                       help="Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0)")
    
    parser.add_argument("--pattern", action="append", required=True,
                       help="Pattern to wait for (can specify multiple for OR logic)")
    
    parser.add_argument("--timeout", type=float, default=10.0,
                       help="Timeout in seconds (default: 10)")
    
    parser.add_argument("--regex", action="store_true",
                       help="Treat patterns as regular expressions")
    
    parser.add_argument("--baud", type=int, default=115200,
                       help="Baud rate (default: 115200)")
    
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Print all received lines")
    
    args = parser.parse_args()
    
    success, matching_line = wait_for_pattern(
        port=args.port,
        patterns=args.pattern,
        timeout=args.timeout,
        use_regex=args.regex,
        baud=args.baud,
        verbose=args.verbose
    )
    
    if success:
        if not args.verbose:
            print(matching_line)
        print(f"✓ Pattern matched", file=sys.stderr)
        return 0
    else:
        print(f"✗ Timeout: Pattern not found in {args.timeout}s", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
