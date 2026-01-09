#!/usr/bin/env python3
"""
Send command to cantest interactive CLI.

Usage:
    ./send_cantest_command.py --port /dev/ttyACM0 --command "c"
    ./send_cantest_command.py --port /dev/ttyUSB0 --command "a"
    ./send_cantest_command.py --port /dev/ttyACM0 --command "d"
    ./send_cantest_command.py --port /dev/ttyACM0 --command "p 1"

Exit codes:
    0 - Success
    1 - Timeout or error
    2 - Serial port error
"""

import argparse
import sys
import time
import serial


def send_command(port: str, command: str, baud: int = 115200, 
                wait_response: float = 1.0, verbose: bool = False) -> bool:
    """
    Send command to cantest and wait for response.
    
    Returns True on success, False on error.
    """
    try:
        ser = serial.Serial(port, baud, timeout=1)
        
        # Send command + newline
        command_bytes = (command + '\n').encode('utf-8')
        ser.write(command_bytes)
        ser.flush()
        
        if verbose:
            print(f"Sent: '{command}'")
        
        # Wait for response
        time.sleep(wait_response)
        
        # Read any response
        responses = []
        while ser.in_waiting:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    responses.append(line)
                    if verbose:
                        print(f"[{port}] {line}")
            except UnicodeDecodeError:
                pass
        
        ser.close()
        
        # Consider success if we got any response or command is single char
        # (cantest often doesn't echo responses for single-char commands)
        if responses or len(command) == 1:
            if not verbose and responses:
                for line in responses:
                    print(line)
            return True
        else:
            if verbose:
                print(f"Warning: No response received", file=sys.stderr)
            return True  # Still success - cantest may not respond
            
    except serial.SerialException as e:
        print(f"✗ Serial error: {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"✗ Error: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Send command to cantest interactive CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Enter controller mode
  %(prog)s --port /dev/ttyACM0 --command "c"

  # Enter audio simulator mode
  %(prog)s --port /dev/ttyUSB0 --command "a"

  # Send discovery query
  %(prog)s --port /dev/ttyACM0 --command "d"

  # Send play sound command
  %(prog)s --port /dev/ttyACM0 --command "p 1"

  # Verbose output
  %(prog)s --port /dev/ttyACM0 --command "c" --verbose
"""
    )
    
    parser.add_argument("--port", required=True,
                       help="Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0)")
    
    parser.add_argument("--command", required=True,
                       help="Command to send (will append newline automatically)")
    
    parser.add_argument("--baud", type=int, default=115200,
                       help="Baud rate (default: 115200)")
    
    parser.add_argument("--wait", type=float, default=1.0,
                       help="Wait time for response in seconds (default: 1.0)")
    
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Print command and responses")
    
    args = parser.parse_args()
    
    success = send_command(
        port=args.port,
        command=args.command,
        baud=args.baud,
        wait_response=args.wait,
        verbose=args.verbose
    )
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
